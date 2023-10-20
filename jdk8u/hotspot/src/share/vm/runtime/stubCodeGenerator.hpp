/*
 * Copyright (c) 1997, 2012, Oracle and/or its affiliates. All rights reserved.
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

#ifndef SHARE_VM_RUNTIME_STUBCODEGENERATOR_HPP
#define SHARE_VM_RUNTIME_STUBCODEGENERATOR_HPP

#include "asm/assembler.hpp"
#include "memory/allocation.hpp"

// All the basic framework for stubcode generation/debugging/printing.


// A StubCodeDesc describes a piece of generated code (usually stubs).
// This information is mainly useful for debugging and printing.
// Currently, code descriptors are simply chained in a linked list,
// this may have to change if searching becomes too slow.

// StubCodeDesc用来描述一段生成的Stub，StubCodeDesc保存的信息通常用于调试和打印日志。
// 目前所有的StubCodeDesc都是链式保存的，如果查找比较慢就可能会改变。StubCodeDesc同样定义在stubCodeGenerator.hpp中
// 除构造方法外，StubCodeDesc定义的方法主要有两种：
//
//  1. 属性操作相关的实例方法，如group，index，set_begin，size_in_bytes等
//  2. 在StubCodeDesc链表中搜索的静态方法，如desc_for，desc_for_index，name_for等
class StubCodeDesc: public CHeapObj<mtCode> {
 protected:
    // StubCodeDesc实例链表
  static StubCodeDesc* volatile _list;         // the list of all descriptors
    // 链表的长度
  static int           _count;                 // length of list

    // 当前StubCodeDesc实例的下一个StubCodeDesc实例
  StubCodeDesc*        _next;                  // the next element in the linked list
    // stub所属的组
  const char*          _group;                 // the group to which the stub code belongs
    // stub的名称
  const char*          _name;                  // the name assigned to the stub code
    // 当前StubCodeDesc实例的序号
  int                  _index;                 // serial number assigned to the stub
    // stub code的起始地址
  address              _begin;                 // points to the first byte of the stub code    (included)
    // stub code的结束地址，即对应内存区域的下一个字节的地址
  address              _end;                   // points to the first byte after the stub code (excluded)

  void set_end(address end) {
    assert(_begin <= end, "begin & end not properly ordered");
    _end = end;
  }

  void set_begin(address begin) {
    assert(begin >= _begin, "begin may not decrease");
    assert(_end == NULL || begin <= _end, "begin & end not properly ordered");
    _begin = begin;
  }

  friend class StubCodeMark;
  friend class StubCodeGenerator;

 public:
  static StubCodeDesc* desc_for(address pc);     // returns the code descriptor for the code containing pc or NULL
  static StubCodeDesc* desc_for_index(int);      // returns the code descriptor for the index or NULL
  static const char*   name_for(address pc);     // returns the name of the code containing pc or NULL

    // StubCodeDesc的构造方法的调用方只有一个StubCodeMark
  StubCodeDesc(const char* group, const char* name, address begin) {
    assert(name != NULL, "no name specified");
        //_list相当于链表头的StubCodeDesc指针，每创建一个新的StubCodeDesc实例则插入到链表的头部，将原来的头部实例作为当前实例的的_next
    _next           = (StubCodeDesc*)OrderAccess::load_ptr_acquire(&_list);
    _group          = group;
    _name           = name;
    _index          = ++_count; // (never zero)
    _begin          = begin;
    _end            = NULL;
        //将当前实例作为新的链表头部实例指针
    OrderAccess::release_store_ptr(&_list, this);
  };

  const char* group() const                      { return _group; }
  const char* name() const                       { return _name; }
  int         index() const                      { return _index; }
  address     begin() const                      { return _begin; }
  address     end() const                        { return _end; }
  int         size_in_bytes() const              { return _end - _begin; }
  bool        contains(address pc) const         { return _begin <= pc && pc < _end; }
  void        print_on(outputStream* st) const;
  void        print() const                      { print_on(tty); }
};

// The base class for all stub-generating code generators.
// Provides utility functions.

// StubCodeGenerator是所有的Stub generators的基类，主要提供一些公用的工具性方法
// StubCodeGenerator定义的方法不多，关键是其构建和析构方法
class StubCodeGenerator: public StackObj {
 protected:
    // 用来生成汇编代码
  MacroAssembler*  _masm;

    // 第一个生成的stub
  StubCodeDesc* _first_stub;
    // 最后一个生成的stub
  StubCodeDesc* _last_stub;
    // 是否打印汇编代码
  bool _print_code;

 public:
  StubCodeGenerator(CodeBuffer* code, bool print_code = false);
  ~StubCodeGenerator();

  MacroAssembler* assembler() const              { return _masm; }

  virtual void stub_prolog(StubCodeDesc* cdesc); // called by StubCodeMark constructor
  virtual void stub_epilog(StubCodeDesc* cdesc); // called by StubCodeMark destructor
};


// Stack-allocated helper class used to assciate a stub code with a name.
// All stub code generating functions that use a StubCodeMark will be registered
// in the global StubCodeDesc list and the generated stub code can be identified
// later via an address pointing into it.

// StubCodeMark是一个工具类，用于将一个生成的stub同其名称关联起来，StubCodeMark会给当前stub创建一个新的StubCodeDesc实例，
// 并将其注册到全局的StubCodeDesc链表中，stub可以通过地址查找到对应的StubCodeDesc实例。
// StubCodeMark的定义同样在stubCodeGenerator.hpp中，其定义的属性有两个
class StubCodeMark: public StackObj {
 protected:
    // 生成当前stub的StubCodeGenerator实例
  StubCodeGenerator* _cgen;
    // 描述当前stub的StubCodeDesc实例
  StubCodeDesc*      _cdesc;

 public:
    // StubCodeMark只有构造和析构函数，没有多余的方法
  StubCodeMark(StubCodeGenerator* cgen, const char* group, const char* name);
  ~StubCodeMark();

};

#endif // SHARE_VM_RUNTIME_STUBCODEGENERATOR_HPP
