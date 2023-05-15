/*
 * Copyright (c) 1997, 2014, Oracle and/or its affiliates. All rights reserved.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 * This code is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 only, as
 * published by the Free Software Foundation.
 *
 * This code is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * version 2 for more details (a copy is included in the LICENSE file that
 * accompanied this code).
 *
 * You should have received a copy of the GNU General Public License version
 * 2 along with this work; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA.
 *
 * Please contact Oracle, 500 Oracle Parkway, Redwood Shores, CA 94065 USA
 * or visit www.oracle.com if you need additional information or have any
 * questions.
 *
 */

#include "precompiled.hpp"
#include "asm/macroAssembler.hpp"
#include "asm/macroAssembler.inline.hpp"
#include "code/codeCache.hpp"
#include "compiler/disassembler.hpp"
#include "oops/oop.inline.hpp"
#include "prims/forte.hpp"
#include "runtime/stubCodeGenerator.hpp"


// Implementation of StubCodeDesc

StubCodeDesc* volatile StubCodeDesc::_list = NULL;
int                    StubCodeDesc::_count = 0;


StubCodeDesc* StubCodeDesc::desc_for(address pc) {
  StubCodeDesc* p = (StubCodeDesc*)OrderAccess::load_ptr_acquire(&_list);
    //遍历所有的StubCodeDesc实例，直到找到一个包含目标地址的实例
  while (p != NULL && !p->contains(pc)) p = p->_next;
  // p == NULL || p->contains(pc)
  return p;
}


StubCodeDesc* StubCodeDesc::desc_for_index(int index) {
  StubCodeDesc* p = (StubCodeDesc*)OrderAccess::load_ptr_acquire(&_list);
  while (p != NULL && p->index() != index) p = p->_next;
  return p;
}

//获取包含指定地址的StubCodeDesc的name属性
const char* StubCodeDesc::name_for(address pc) {
  StubCodeDesc* p = desc_for(pc);
  return p == NULL ? NULL : p->name();
}


void StubCodeDesc::print_on(outputStream* st) const {
  st->print("%s", group());
  st->print("::");
  st->print("%s", name());
  st->print(" [" INTPTR_FORMAT ", " INTPTR_FORMAT "[ (%d bytes)", p2i(begin()), p2i(end()), size_in_bytes());
}

// Implementation of StubCodeGenerator

StubCodeGenerator::StubCodeGenerator(CodeBuffer* code, bool print_code) {
    //构造一个新的MacroAssembler实例
  _masm = new MacroAssembler(code);
  _first_stub = _last_stub = NULL;
  _print_code = print_code;
}

extern "C" {
  static int compare_cdesc(const void* void_a, const void* void_b) {
    int ai = (*((StubCodeDesc**) void_a))->index();
    int bi = (*((StubCodeDesc**) void_b))->index();
    return ai - bi;
  }
}

StubCodeGenerator::~StubCodeGenerator() {
  if (PrintStubCode || _print_code) {
    CodeBuffer* cbuf = _masm->code();
      //CodeBuffer的inst section的start是用CodeBlob的content_start初始化的，这里是反向查找
    CodeBlob*   blob = CodeCache::find_blob_unsafe(cbuf->insts()->start());
    if (blob != NULL) {
        //将cbuf的_code_strings设置到blob中
      blob->set_strings(cbuf->strings());
    }
    bool saw_first = false;
    StubCodeDesc* toprint[1000];
    int toprint_len = 0;
      //遍历所有的StubCodeDesc，将其放到toprint数组中
    for (StubCodeDesc* cdesc = _last_stub; cdesc != NULL; cdesc = cdesc->_next) {
      toprint[toprint_len++] = cdesc;
      if (cdesc == _first_stub) { saw_first = true; break; }
    }
    assert(saw_first, "must get both first & last");
    // Print in reverse order:
      //将其按照StubCodeDesc的index属性排序
    qsort(toprint, toprint_len, sizeof(toprint[0]), compare_cdesc);
      //遍历排序后的StubCodeDesc
    for (int i = 0; i < toprint_len; i++) {
      StubCodeDesc* cdesc = toprint[i];
        //打印原始的汇编代码
      cdesc->print();
        //写入换行符，tty是outputStream*
      tty->cr();
        //打印反汇编代码，即数字形式的汇编指令转换成对应的速记符，并加上java程序的相关信息
      Disassembler::decode(cdesc->begin(), cdesc->end());
      tty->cr();
    }
  }
}

//stub_prolog是一个虚方法，默认是空实现
void StubCodeGenerator::stub_prolog(StubCodeDesc* cdesc) {
  // default implementation - do nothing
}


//stub_epilog是一个虚方法，下列是默认实现
void StubCodeGenerator::stub_epilog(StubCodeDesc* cdesc) {
  // default implementation - record the cdesc
    //设置StubCodeGenerator的_last_stub属性
  if (_first_stub == NULL)  _first_stub = cdesc;
  _last_stub = cdesc;
}


// Implementation of CodeMark

StubCodeMark::StubCodeMark(StubCodeGenerator* cgen, const char* group, const char* name) {
  _cgen  = cgen;
    //_cgen->assembler()->pc()返回的是StubCodeDesc的start属性，即stub code的起始地址
  _cdesc = new StubCodeDesc(group, name, _cgen->assembler()->pc());
  _cgen->stub_prolog(_cdesc);
  // define the stub's beginning (= entry point) to be after the prolog:
    //重置stub code的起始地址，避免stub_prolog中改变了起始地址
  _cdesc->set_begin(_cgen->assembler()->pc());
}

StubCodeMark::~StubCodeMark() {
    //flush方法将生成的汇编代码写入到CodeBuffer中
  _cgen->assembler()->flush();
    //设置end属性
  _cdesc->set_end(_cgen->assembler()->pc());
    //校验当前StubCodeDesc处于链表头部，即在StubCodeMark构造完成到析构前没有创建一个新的StubCodeDesc实例
  assert(StubCodeDesc::_list == _cdesc, "expected order on list");
  _cgen->stub_epilog(_cdesc);
    //将生成的stub注册到操作系统中，相当于操作系统加载了某个函数的实现到当前进程的代码区
  Forte::register_stub(_cdesc->name(), _cdesc->begin(), _cdesc->end());
    //发布Jvmti事件
  if (JvmtiExport::should_post_dynamic_code_generated()) {
    JvmtiExport::post_dynamic_code_generated(_cdesc->name(), _cdesc->begin(), _cdesc->end());
  }
}
