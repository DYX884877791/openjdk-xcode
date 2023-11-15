/*
 * Copyright (c) 1997, 2016, Oracle and/or its affiliates. All rights reserved.
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
#include "interpreter/interpreter.hpp"
#include "interpreter/interpreterGenerator.hpp"
#include "interpreter/interpreterRuntime.hpp"
#include "interpreter/templateTable.hpp"
#include "utilities/slog.hpp"

#ifndef CC_INTERP

# define __ _masm->

void TemplateInterpreter::initialize() {
  slog_trace("进入hotspot/src/share/vm/interpreter/templateInterpreter.cpp中的TemplateInterpreter::initialize函数...");
  if (_code != NULL) return;
  // assertions
    //校验字节码的个数必须小于等于DispatchTable的长度
  assert((int)Bytecodes::number_of_codes <= (int)DispatchTable::length,
         "dispatch table too small");

    //初始化
    // 抽象解释器AbstractInterpreter的初始化，
    // AbstractInterpreter是基于汇编模型的解释器的共同基类，
    // 定义了解释器和解释器生成器的抽象接口
  AbstractInterpreter::initialize();

    // 模板表TemplateTable的初始化，模板表TemplateTable保存了各个字节码的模板（目标代码生成函数和参数）；
  TemplateTable::initialize();

  // generate interpreter
  { ResourceMark rm;
    TraceTime timer("Interpreter generation", TraceStartupTime);
      // InterpreterCodeSize是在平台相关的templateInterpreter_x86.hpp中定义的，64位下是256 * 1024
    int code_size = InterpreterCodeSize;
    NOT_PRODUCT(code_size *= 4;)  // debug uses extra interpreter code space
      //创建一个StubQueue
      // CodeCache的Stub队列StubQueue的初始化
      // StubQueue是用来保存生成的本地代码的Stub队列，队列每一个元素对应一个InterpreterCodelet对象，InterpreterCodelet对象继承自抽象基类Stub，包含了字节码对应的本地代码以及一些调试和输出信息

      // 使用new关键字初始化定义在AbstractInterpreter类中的_code静态属性
      // 由于TemplateInterpreter继承自AbstractInterpreter，所以在TemplateInterpreter中初始化的_code属性其实就是AbstractInterpreter类中定义的_code属性。
    _code = new StubQueue(new InterpreterCodeletInterface, code_size, NULL,
                          "Interpreter");
      //初始化InterpreterGenerator，初始化的时候会生成所有的调用函数
      //  实例化模板解释器生成器对象TemplateInterpreterGenerator，里面也涉及到初始化。
    slog_trace("即将创建InterpreterGenerator实例...");
    InterpreterGenerator g(_code);
    if (PrintInterpreter) print();
  }

  // initialize dispatch table
    // 初始化字节分发表
    // _normal_table是如何初始化的了？答案在TemplateInterpreterGenerator::generate_all()方法调用的set_entry_points_for_all_bytes()方法的实现中
  slog_trace("将字节分发表_active_table赋值为_normal_table...");
  _active_table = _normal_table;
}

//------------------------------------------------------------------------------------------------------------------------
// Implementation of EntryPoint

EntryPoint::EntryPoint() {
  assert(number_of_states == 10, "check the code below");
  _entry[btos] = NULL;
  _entry[ztos] = NULL;
  _entry[ctos] = NULL;
  _entry[stos] = NULL;
  _entry[atos] = NULL;
  _entry[itos] = NULL;
  _entry[ltos] = NULL;
  _entry[ftos] = NULL;
  _entry[dtos] = NULL;
  _entry[vtos] = NULL;
}


EntryPoint::EntryPoint(address bentry, address zentry, address centry, address sentry, address aentry, address ientry, address lentry, address fentry, address dentry, address ventry) {
  slog_trace("进入hotspot/src/share/vm/interpreter/templateInterpreter.cpp中的EntryPoint::EntryPoint有参构造函数...");
  assert(number_of_states == 10, "check the code below");
  _entry[btos] = bentry;
  _entry[ztos] = zentry;
  _entry[ctos] = centry;
  _entry[stos] = sentry;
  _entry[atos] = aentry;
  _entry[itos] = ientry;
  _entry[ltos] = lentry;
  _entry[ftos] = fentry;
  _entry[dtos] = dentry;
  _entry[vtos] = ventry;
}


void EntryPoint::set_entry(TosState state, address entry) {
  assert(0 <= state && state < number_of_states, "state out of bounds");
  _entry[state] = entry;
}


address EntryPoint::entry(TosState state) const {
  assert(0 <= state && state < number_of_states, "state out of bounds");
  return _entry[state];
}


void EntryPoint::print() {
  tty->print("[");
  for (int i = 0; i < number_of_states; i++) {
    if (i > 0) tty->print(", ");
    tty->print(INTPTR_FORMAT, p2i(_entry[i]));
  }
  tty->print("]");
}


bool EntryPoint::operator == (const EntryPoint& y) {
  int i = number_of_states;
  while (i-- > 0) {
    if (_entry[i] != y._entry[i]) return false;
  }
  return true;
}


//------------------------------------------------------------------------------------------------------------------------
// Implementation of DispatchTable

EntryPoint DispatchTable::entry(int i) const {
  assert(0 <= i && i < length, "index out of bounds");
  return
    EntryPoint(
      _table[btos][i],
      _table[ztos][i],
      _table[ctos][i],
      _table[stos][i],
      _table[atos][i],
      _table[itos][i],
      _table[ltos][i],
      _table[ftos][i],
      _table[dtos][i],
      _table[vtos][i]
    );
}


// 这个函数显示了对每个字节码的每个栈顶状态都设置入口地址
// 其中的参数i就是opcode，各个字节码及对应的opcode可参考https://docs.oracle.com/javase/specs/jvms/se8/html/index.html。
// _table的一维为栈顶缓存状态，二维为Opcode，通过这2个维度能够找到一段机器指令，这就是根据当前的栈顶缓存状态定位到的字节码需要执行的机器指令片段。
// ===========
// 注意：
// 栈顶缓存技术实际是模板解释器的一个副产物，模板解释器下所有的字节码指令的实现都是通过指定的汇编指令实现，正是基于此才能将栈顶值显示的放到rax寄存器中，
// 且需要频繁读取栈顶值的字节码指令可以显示的读取rax寄存器中内容。C++解释器虽然最终也是编译成汇编代码执行，但是C++语言中无法直接操作寄存器，
// 只能暗示编译器这样处理寄存器，无法保证编译器最终一定会按照代码中的想法操作寄存器。栈顶缓存技术的实现跟字节码指令的实现是紧密关联，相互配合的，
// 每个字节码指令通过指令模板指定该指令在执行前后栈顶缓存的值类型，每个字节码指令执行前都需要判断当前栈帧的栈顶缓存的值类型，
// 必要时通过将栈顶的值pop到rax中或者push到栈帧中，让当前栈帧的的栈顶缓存的值类型满足将要执行的字节码指令的栈顶缓存的值类型。
// 每个字节码指令执行完成后都将执行完后的栈顶缓存值类型即当前栈帧的栈顶缓存值类型传递给下一个指令，下一个指令据此选择对应值类型下的汇编指令入口地址执行。
void DispatchTable::set_entry(int i, EntryPoint& entry) {
  assert(0 <= i && i < length, "index out of bounds");
  assert(number_of_states == 10, "check the code below");
  _table[btos][i] = entry.entry(btos);
  _table[ztos][i] = entry.entry(ztos);
  _table[ctos][i] = entry.entry(ctos);
  _table[stos][i] = entry.entry(stos);
  _table[atos][i] = entry.entry(atos);
  _table[itos][i] = entry.entry(itos);
  _table[ltos][i] = entry.entry(ltos);
  _table[ftos][i] = entry.entry(ftos);
  _table[dtos][i] = entry.entry(dtos);
  _table[vtos][i] = entry.entry(vtos);
}


bool DispatchTable::operator == (DispatchTable& y) {
  int i = length;
  while (i-- > 0) {
    EntryPoint t = y.entry(i); // for compiler compatibility (BugId 4150096)
    if (!(entry(i) == t)) return false;
  }
  return true;
}

address    TemplateInterpreter::_remove_activation_entry                    = NULL;
address    TemplateInterpreter::_remove_activation_preserving_args_entry    = NULL;


address    TemplateInterpreter::_throw_ArrayIndexOutOfBoundsException_entry = NULL;
address    TemplateInterpreter::_throw_ArrayStoreException_entry            = NULL;
address    TemplateInterpreter::_throw_ArithmeticException_entry            = NULL;
address    TemplateInterpreter::_throw_ClassCastException_entry             = NULL;
address    TemplateInterpreter::_throw_NullPointerException_entry           = NULL;
address    TemplateInterpreter::_throw_StackOverflowError_entry             = NULL;
address    TemplateInterpreter::_throw_exception_entry                      = NULL;

#ifndef PRODUCT
EntryPoint TemplateInterpreter::_trace_code;
#endif // !PRODUCT
EntryPoint TemplateInterpreter::_return_entry[TemplateInterpreter::number_of_return_entries];
EntryPoint TemplateInterpreter::_earlyret_entry;
EntryPoint TemplateInterpreter::_deopt_entry [TemplateInterpreter::number_of_deopt_entries ];
EntryPoint TemplateInterpreter::_continuation_entry;
EntryPoint TemplateInterpreter::_safept_entry;

address TemplateInterpreter::_invoke_return_entry[TemplateInterpreter::number_of_return_addrs];
address TemplateInterpreter::_invokeinterface_return_entry[TemplateInterpreter::number_of_return_addrs];
address TemplateInterpreter::_invokedynamic_return_entry[TemplateInterpreter::number_of_return_addrs];

DispatchTable TemplateInterpreter::_active_table;
DispatchTable TemplateInterpreter::_normal_table;
DispatchTable TemplateInterpreter::_safept_table;
address    TemplateInterpreter::_wentry_point[DispatchTable::length];

TemplateInterpreterGenerator::TemplateInterpreterGenerator(StubQueue* _code): AbstractInterpreterGenerator(_code) {
  _unimplemented_bytecode    = NULL;
  _illegal_bytecode_sequence = NULL;
}

static const BasicType types[Interpreter::number_of_result_handlers] = {
  T_BOOLEAN,
  T_CHAR   ,
  T_BYTE   ,
  T_SHORT  ,
  T_INT    ,
  T_LONG   ,
  T_VOID   ,
  T_FLOAT  ,
  T_DOUBLE ,
  T_OBJECT
};

// generate_all方法用于生成Interpreter中定义的各种调用入口地址（即初始化了所有的例程：）
// 调用的generate_all()函数将生成一系列HotSpot运行过程中所执行的一些公共代码的入口和所有字节码的InterpreterCodelet
// 另外，既然已经涉及到机器码了，单独的templateInterpreterGenerator显然是不能完成这件事的，它还需要配合
//
//hotspot\src\cpu\x86\vm\templateInterpreterGenerator_x86.cpp&&hotspot\src\cpu\x86\vm\templateInterpreterGenerator_x86_64.cpp一起做事
// 使用-XX:+UnlockDiagnosticVMOptions -XX:+PrintInterpreter -XX:+LogCompilation -XX:LogFile=file.log保存结果到文件，可以查看生成的这些例程。
void TemplateInterpreterGenerator::generate_all() {
  slog_trace("进入hotspot/src/share/vm/interpreter/templateInterpreter.cpp中的TemplateInterpreterGenerator::generate_all函数...");
  AbstractInterpreterGenerator::generate_all();

  { CodeletMark cm(_masm, "error exits");
      //生成包含错误提示的退出函数
    _unimplemented_bytecode    = generate_error_exit("unimplemented bytecode");
    _illegal_bytecode_sequence = generate_error_exit("illegal bytecode sequence - method not verified");
  }

#ifndef PRODUCT
  if (TraceBytecodes) {
    CodeletMark cm(_masm, "bytecode tracing support");
    Interpreter::_trace_code =
      EntryPoint(
        generate_trace_code(btos),
        generate_trace_code(ztos),
        generate_trace_code(ctos),
        generate_trace_code(stos),
        generate_trace_code(atos),
        generate_trace_code(itos),
        generate_trace_code(ltos),
        generate_trace_code(ftos),
        generate_trace_code(dtos),
        generate_trace_code(vtos)
      );
  }
#endif // !PRODUCT

  { CodeletMark cm(_masm, "return entry points");
    const int index_size = sizeof(u2);
      //初始化Interpreter::_return_entry数组
    for (int i = 0; i < Interpreter::number_of_return_entries; i++) {
      Interpreter::_return_entry[i] =
        EntryPoint(
          generate_return_entry_for(itos, i, index_size),
          generate_return_entry_for(itos, i, index_size),
          generate_return_entry_for(itos, i, index_size),
          generate_return_entry_for(itos, i, index_size),
          generate_return_entry_for(atos, i, index_size),
          generate_return_entry_for(itos, i, index_size),
          generate_return_entry_for(ltos, i, index_size),
          generate_return_entry_for(ftos, i, index_size),
          generate_return_entry_for(dtos, i, index_size),
          generate_return_entry_for(vtos, i, index_size)
        );
    }
  }

  { CodeletMark cm(_masm, "invoke return entry points");
    // These states are in order specified in TosState, except btos/ztos/ctos/stos are
    // really the same as itos since there is no top of stack optimization for these types
      //btos/ztos/ctos/stos这四种栈顶值类型都会转换成int，所以states将其替换成itos
    const TosState states[] = {itos, itos, itos, itos, itos, ltos, ftos, dtos, atos, vtos, ilgl};
      //获取三个指令的指令长度
    const int invoke_length = Bytecodes::length_for(Bytecodes::_invokestatic);
    const int invokeinterface_length = Bytecodes::length_for(Bytecodes::_invokeinterface);
    const int invokedynamic_length = Bytecodes::length_for(Bytecodes::_invokedynamic);

      //逐一初始化_invoke_return_entry，_invokeinterface_return_entry，_invokedynamic_return_entry
    for (int i = 0; i < Interpreter::number_of_return_addrs; i++) {
      TosState state = states[i];
      assert(state != ilgl, "states array is wrong above");
      Interpreter::_invoke_return_entry[i] = generate_return_entry_for(state, invoke_length, sizeof(u2));
      Interpreter::_invokeinterface_return_entry[i] = generate_return_entry_for(state, invokeinterface_length, sizeof(u2));
      Interpreter::_invokedynamic_return_entry[i] = generate_return_entry_for(state, invokedynamic_length, sizeof(u4));
    }
  }

  { CodeletMark cm(_masm, "earlyret entry points");
    Interpreter::_earlyret_entry =
      EntryPoint(
        generate_earlyret_entry_for(btos),
        generate_earlyret_entry_for(ztos),
        generate_earlyret_entry_for(ctos),
        generate_earlyret_entry_for(stos),
        generate_earlyret_entry_for(atos),
        generate_earlyret_entry_for(itos),
        generate_earlyret_entry_for(ltos),
        generate_earlyret_entry_for(ftos),
        generate_earlyret_entry_for(dtos),
        generate_earlyret_entry_for(vtos)
      );
  }

  { CodeletMark cm(_masm, "deoptimization entry points");
    for (int i = 0; i < Interpreter::number_of_deopt_entries; i++) {
      Interpreter::_deopt_entry[i] =
        EntryPoint(
          generate_deopt_entry_for(itos, i),
          generate_deopt_entry_for(itos, i),
          generate_deopt_entry_for(itos, i),
          generate_deopt_entry_for(itos, i),
          generate_deopt_entry_for(atos, i),
          generate_deopt_entry_for(itos, i),
          generate_deopt_entry_for(ltos, i),
          generate_deopt_entry_for(ftos, i),
          generate_deopt_entry_for(dtos, i),
          generate_deopt_entry_for(vtos, i)
        );
    }
  }

  { CodeletMark cm(_masm, "result handlers for native calls");
    // The various result converter stublets.
    int is_generated[Interpreter::number_of_result_handlers];
    memset(is_generated, 0, sizeof(is_generated));

    for (int i = 0; i < Interpreter::number_of_result_handlers; i++) {
      BasicType type = types[i];
      if (!is_generated[Interpreter::BasicType_as_index(type)]++) {
        Interpreter::_native_abi_to_tosca[Interpreter::BasicType_as_index(type)] = generate_result_handler_for(type);
      }
    }
  }

  { CodeletMark cm(_masm, "continuation entry points");
    Interpreter::_continuation_entry =
      EntryPoint(
        generate_continuation_for(btos),
        generate_continuation_for(ztos),
        generate_continuation_for(ctos),
        generate_continuation_for(stos),
        generate_continuation_for(atos),
        generate_continuation_for(itos),
        generate_continuation_for(ltos),
        generate_continuation_for(ftos),
        generate_continuation_for(dtos),
        generate_continuation_for(vtos)
      );
  }

  { CodeletMark cm(_masm, "safepoint entry points");
    Interpreter::_safept_entry =
      EntryPoint(
        generate_safept_entry_for(btos, CAST_FROM_FN_PTR(address, InterpreterRuntime::at_safepoint)),
        generate_safept_entry_for(ztos, CAST_FROM_FN_PTR(address, InterpreterRuntime::at_safepoint)),
        generate_safept_entry_for(ctos, CAST_FROM_FN_PTR(address, InterpreterRuntime::at_safepoint)),
        generate_safept_entry_for(stos, CAST_FROM_FN_PTR(address, InterpreterRuntime::at_safepoint)),
        generate_safept_entry_for(atos, CAST_FROM_FN_PTR(address, InterpreterRuntime::at_safepoint)),
        generate_safept_entry_for(itos, CAST_FROM_FN_PTR(address, InterpreterRuntime::at_safepoint)),
        generate_safept_entry_for(ltos, CAST_FROM_FN_PTR(address, InterpreterRuntime::at_safepoint)),
        generate_safept_entry_for(ftos, CAST_FROM_FN_PTR(address, InterpreterRuntime::at_safepoint)),
        generate_safept_entry_for(dtos, CAST_FROM_FN_PTR(address, InterpreterRuntime::at_safepoint)),
        generate_safept_entry_for(vtos, CAST_FROM_FN_PTR(address, InterpreterRuntime::at_safepoint))
      );
  }

  { CodeletMark cm(_masm, "exception handling");
    // (Note: this is not safepoint safe because thread may return to compiled code)
    generate_throw_exception();
  }

  { CodeletMark cm(_masm, "throw exception entrypoints");
    Interpreter::_throw_ArrayIndexOutOfBoundsException_entry = generate_ArrayIndexOutOfBounds_handler("java/lang/ArrayIndexOutOfBoundsException");
    Interpreter::_throw_ArrayStoreException_entry            = generate_klass_exception_handler("java/lang/ArrayStoreException"                 );
    Interpreter::_throw_ArithmeticException_entry            = generate_exception_handler("java/lang/ArithmeticException"           , "/ by zero");
    Interpreter::_throw_ClassCastException_entry             = generate_ClassCastException_handler();
    Interpreter::_throw_NullPointerException_entry           = generate_exception_handler("java/lang/NullPointerException"          , NULL       );
    Interpreter::_throw_StackOverflowError_entry             = generate_StackOverflowError_handler();
  }



//宏定义，调用generate_method_entry生成汇编入口
#define method_entry(kind)                                                                    \
  { CodeletMark cm(_masm, "method entry point (kind = " #kind ")");                    \
    Interpreter::_entry_table[Interpreter::kind] = generate_method_entry(Interpreter::kind);  \
  }

    //初始化_entry_table
    // 普通的、没有native关键字修饰的Java方法生成入口
    // method_entry是一个宏，扩展后如上的method_entry(zerolocals)语句变为如下的形式：
    // Interpreter::_entry_table[Interpreter::zerolocals] = generate_method_entry(Interpreter::zerolocals);
    // _entry_table变量定义在AbstractInterpreter类中，如下
    // static address  _entry_table[number_of_method_entries];
    // 调用generate_method_entry()函数为各种类型的方法生成对应的方法入口
    // InterpreterGenerator::generate_normal_entry()函数最终会返回生成机器码的入口执行地址，然后通过变量_entry_table数组来保存，这样就可以使用方法类型做为数组下标获取对应的方法入口了。
    //
    //生成各种Java方法入口
    // 大部分MethodKind对应的address都是通过generate_method_entry方法生成，除了method_handle_invoke_FIRST到method_handle_invoke_LAST之间的几个，
    // 他们是通过AbstractInterpreterGenerator::initialize_method_handle_entries完成初始化的
  // all non-native method kinds
  slog_trace("即将调用method_entry宏来生成各种类型Java方法的入口...");
  method_entry(zerolocals)
  method_entry(zerolocals_synchronized)
  method_entry(empty)
  method_entry(accessor)
  method_entry(abstract)
  method_entry(java_lang_math_sin  )
  method_entry(java_lang_math_cos  )
  method_entry(java_lang_math_tan  )
  method_entry(java_lang_math_abs  )
  method_entry(java_lang_math_sqrt )
  method_entry(java_lang_math_log  )
  method_entry(java_lang_math_log10)
  method_entry(java_lang_math_exp  )
  method_entry(java_lang_math_pow  )
  method_entry(java_lang_ref_reference_get)

  if (UseCRC32Intrinsics) {
    method_entry(java_util_zip_CRC32_update)
    method_entry(java_util_zip_CRC32_updateBytes)
    method_entry(java_util_zip_CRC32_updateByteBuffer)
  }

  initialize_method_handle_entries();

  // all native method kinds (must be one contiguous block)
  Interpreter::_native_entry_begin = Interpreter::code()->code_end();
  method_entry(native)
  method_entry(native_synchronized)
  Interpreter::_native_entry_end = Interpreter::code()->code_end();

#undef method_entry

  // Bytecodes
    //生成所有字节码对应的汇编指令
  set_entry_points_for_all_bytes();
  set_safepoints_for_all_bytes();
}

//------------------------------------------------------------------------------------------------------------------------

address TemplateInterpreterGenerator::generate_error_exit(const char* msg) {
  address entry = __ pc();
  __ stop(msg);
  return entry;
}


//------------------------------------------------------------------------------------------------------------------------

// 生成各个字节码对应的例程。最终会调用到TemplateInterpreterGenerator::generate_and_dispatch()函数。
void TemplateInterpreterGenerator::set_entry_points_for_all_bytes() {
  slog_trace("进入hotspot/src/share/vm/interpreter/templateInterpreter.cpp中的TemplateInterpreterGenerator::set_entry_points_for_all_bytes函数，生成所有字节码对应的汇编指令...");
    //逐一遍历所有的字节码
  for (int i = 0; i < DispatchTable::length; i++) {
    Bytecodes::Code code = (Bytecodes::Code)i;
      //如果定义了这个字节码
    if (Bytecodes::is_defined(code)) {
        //生成对应字节码的汇编指令
      set_entry_points(code);
    } else {
        //将其标记成未实现
      set_unimplemented(i);
    }
  }
}


void TemplateInterpreterGenerator::set_safepoints_for_all_bytes() {
  for (int i = 0; i < DispatchTable::length; i++) {
    Bytecodes::Code code = (Bytecodes::Code)i;
    if (Bytecodes::is_defined(code)) Interpreter::_safept_table.set_entry(code, Interpreter::_safept_entry);
  }
}


void TemplateInterpreterGenerator::set_unimplemented(int i) {
  address e = _unimplemented_bytecode;
  EntryPoint entry(e, e, e, e, e, e, e, e, e, e);
  Interpreter::_normal_table.set_entry(i, entry);
  Interpreter::_wentry_point[i] = _unimplemented_bytecode;
}


/**
 * 模板解释器生成器调用   TemplateInterpreterGenerator::set_entry_points()  为每个字节码设置例程，可以看到这个函数的参数就是 Bytecodes::Code 表示一个字节码指令
 */
void TemplateInterpreterGenerator::set_entry_points(Bytecodes::Code code) {
  slog_trace("进入hotspot/src/share/vm/interpreter/templateInterpreter.cpp中的TemplateInterpreterGenerator::set_entry_points函数...");
    //这里的CodeletMark会生成一个InterpreterCodelet用以保存字节码和汇编码
    //在CodeletMark析构时会将InterpreterCodelet提交到StubQueue
  CodeletMark cm(_masm, Bytecodes::name(code), code);
  // initialize entry points
    // 初始化 entry points
  assert(_unimplemented_bytecode    != NULL, "should have been generated before");
  assert(_illegal_bytecode_sequence != NULL, "should have been generated before");
  address bep = _illegal_bytecode_sequence;
  address zep = _illegal_bytecode_sequence;
  address cep = _illegal_bytecode_sequence;
  address sep = _illegal_bytecode_sequence;
  address aep = _illegal_bytecode_sequence;
  address iep = _illegal_bytecode_sequence;
  address lep = _illegal_bytecode_sequence;
  address fep = _illegal_bytecode_sequence;
  address dep = _illegal_bytecode_sequence;
  address vep = _unimplemented_bytecode;
  address wep = _unimplemented_bytecode;
  // code for short & wide version of bytecode
    //如果定义了字节码
  if (Bytecodes::is_defined(code)) {
      //获取该字节码对应的Template
    Template* t = TemplateTable::template_for(code);
    assert(t->is_valid(), "just checking");
      //生成汇编代码，最终调用Template::generate方法生成
    set_short_entry_points(t, bep, cep, sep, aep, iep, lep, fep, dep, vep);
  }
    //如果是宽字节
  if (Bytecodes::wide_is_defined(code)) {
      //获取该字节码对应的Template
    Template* t = TemplateTable::template_for_wide(code);
    assert(t->is_valid(), "just checking");
      //生成汇编代码，最终调用Template::generate方法生成
    set_wide_entry_point(t, wep);
  }
  // set entry points
    // 将生成的 entry points放入_normal_table中
  EntryPoint entry(bep, zep, cep, sep, aep, iep, lep, fep, dep, vep);
  Interpreter::_normal_table.set_entry(code, entry);
  Interpreter::_wentry_point[code] = wep;
}


void TemplateInterpreterGenerator::set_wide_entry_point(Template* t, address& wep) {
  assert(t->is_valid(), "template must exist");
  assert(t->tos_in() == vtos, "only vtos tos_in supported for wide instructions");
  wep = __ pc(); generate_and_dispatch(t);
}


//bep，cep，sep等分别对应不同栈顶缓存（即rax寄存器中的）值类型下的同一个字节码的调用入口地址，bep对应btos,cep对应ctos，依次类推
// 该方法用来实际生成_normal_table中各个字节码的调用入口地址
void TemplateInterpreterGenerator::set_short_entry_points(Template* t, address& bep, address& cep, address& sep, address& aep, address& iep, address& lep, address& fep, address& dep, address& vep) {
  assert(t->is_valid(), "template must exist");
  switch (t->tos_in()) {
    case btos:
    case ztos:
    case ctos:
    case stos:
        // 上述四种对应的变量类型byte，char，short在JVM中都会转化成int处理，所以实际的指令的tos_in不可能有上述四种类型.
      ShouldNotReachHere();  // btos/ctos/stos should use itos.
      break;
        //以atos为例说明，如果字节码指令本身要求在指令执行前栈顶缓存的值是atos类型的，当执行到此指令时，如果栈顶缓存的值类型是vtos，即没有缓存任何值，则需要
        //执行pop(atos)，从当前栈帧pop一个atos类型的变量到rax中以满足指令执行前的栈顶缓存值类型。如果栈顶缓存的值类型就是vtos，则直接执行该字节码对应的指令。
    case atos: vep = __ pc(); __ pop(atos); aep = __ pc(); generate_and_dispatch(t); break;
    case itos: vep = __ pc(); __ pop(itos); iep = __ pc(); generate_and_dispatch(t); break;
    case ltos: vep = __ pc(); __ pop(ltos); lep = __ pc(); generate_and_dispatch(t); break;
    case ftos: vep = __ pc(); __ pop(ftos); fep = __ pc(); generate_and_dispatch(t); break;
    case dtos: vep = __ pc(); __ pop(dtos); dep = __ pc(); generate_and_dispatch(t); break;
        //如果字节码指令本身要求在指令执行前栈顶缓存的值是vtos类型的，即不需要缓存任何值，那么如果此时栈顶缓存值类型不是vtos，比如是itos则需要将栈顶缓存的值push到
        //栈帧中，push完成栈顶缓存的值类型就变成vtos
    case vtos: set_vtos_entry_points(t, bep, cep, sep, aep, iep, lep, fep, dep, vep);     break;
    default  : ShouldNotReachHere();                                                 break;
  }
}


//------------------------------------------------------------------------------------------------------------------------

void TemplateInterpreterGenerator::generate_and_dispatch(Template* t, TosState tos_out) {
  if (PrintBytecodeHistogram)                                    histogram_bytecode(t);
#ifndef PRODUCT
  // debugging code
  if (CountBytecodes || TraceBytecodes || StopInterpreterAt > 0) count_bytecode();
  if (PrintBytecodePairHistogram)                                histogram_bytecode_pair(t);
  if (TraceBytecodes)                                            trace_bytecode(t);
  if (StopInterpreterAt > 0)                                     stop_interpreter_at();
  __ verify_FPU(1, t->tos_in());
#endif // !PRODUCT
  int step = 0;
    //如果不是dispatch类型的指令，一般只有跳转性质的如方法返回的ret指令属于dispatch类型的
  if (!t->does_dispatch()) {
      //计算指令的宽度，指令宽度是指指令本身占用的一个字节加上指令参数的字节数，即跳过这个宽度就可以读取下一个字节码指令了
    step = t->is_wide() ? Bytecodes::wide_length_for(t->bytecode()) : Bytecodes::length_for(t->bytecode());
    if (tos_out == ilgl) tos_out = t->tos_out();
    // compute bytecode size
    assert(step > 0, "just checkin'");
    // setup stuff for dispatching next bytecode
    if (ProfileInterpreter && VerifyDataPointer
        && MethodData::bytecode_has_profile(t->bytecode())) {
      __ verify_method_data_pointer();
    }
      //86x64下是空方法
    __ dispatch_prolog(tos_out, step);
  }
  // generate template
    //生成该字节码对应的汇编指令
  t->generate(_masm);
  // advance
  if (t->does_dispatch()) {
#ifdef ASSERT
    // make sure execution doesn't go beyond this point if code is broken
    __ should_not_reach_here();
#endif // ASSERT
  } else {
    // dispatch to next bytecode
      //跳转到下一个字节码指令，这里需要注意的是dispatch_epilog对应的汇编指令会写入到该字节码对应的汇编指令中
      //注意这里是将tos_out传递下去了，即实际执行dispatch_next时的state是上一个指令执行完后的栈顶缓存的值类型，即当前栈顶缓存的值类型，跳转下一个字节码指令实现时会选择在当前栈顶缓存值类型对应的汇编指令实现
      //至此整个字节码执行以及栈顶缓存的实现就串联起来了
    __ dispatch_epilog(tos_out, step);
  }
}

//------------------------------------------------------------------------------------------------------------------------
// Entry points

/**
 * Returns the return entry table for the given invoke bytecode.
 */
address* TemplateInterpreter::invoke_return_entry_table_for(Bytecodes::Code code) {
  switch (code) {
  case Bytecodes::_invokestatic:
  case Bytecodes::_invokespecial:
  case Bytecodes::_invokevirtual:
  case Bytecodes::_invokehandle:
    return Interpreter::invoke_return_entry_table();
  case Bytecodes::_invokeinterface:
    return Interpreter::invokeinterface_return_entry_table();
  case Bytecodes::_invokedynamic:
    return Interpreter::invokedynamic_return_entry_table();
  default:
    fatal(err_msg("invalid bytecode: %s", Bytecodes::name(code)));
    return NULL;
  }
}

/**
 * Returns the return entry address for the given top-of-stack state and bytecode.
 */
address TemplateInterpreter::return_entry(TosState state, int length, Bytecodes::Code code) {
  guarantee(0 <= length && length < Interpreter::number_of_return_entries, "illegal length");
  const int index = TosState_as_index(state);
  switch (code) {
  case Bytecodes::_invokestatic:
  case Bytecodes::_invokespecial:
  case Bytecodes::_invokevirtual:
  case Bytecodes::_invokehandle:
    return _invoke_return_entry[index];
  case Bytecodes::_invokeinterface:
    return _invokeinterface_return_entry[index];
  case Bytecodes::_invokedynamic:
    return _invokedynamic_return_entry[index];
  default:
    assert(!Bytecodes::is_invoke(code), err_msg("invoke instructions should be handled separately: %s", Bytecodes::name(code)));
    return _return_entry[length].entry(state);
  }
}


address TemplateInterpreter::deopt_entry(TosState state, int length) {
  guarantee(0 <= length && length < Interpreter::number_of_deopt_entries, "illegal length");
  return _deopt_entry[length].entry(state);
}

//------------------------------------------------------------------------------------------------------------------------
// Suport for invokes

int TemplateInterpreter::TosState_as_index(TosState state) {
  assert( state < number_of_states , "Invalid state in TosState_as_index");
  assert(0 <= (int)state && (int)state < TemplateInterpreter::number_of_return_addrs, "index out of bounds");
  return (int)state;
}


//------------------------------------------------------------------------------------------------------------------------
// Safepoint suppport

static inline void copy_table(address* from, address* to, int size) {
  // Copy non-overlapping tables. The copy has to occur word wise for MT safety.
    //每次复制8字节的数组，DispatchTable就是一个address二维数组，因此在不加锁的情况下也能保证解释器依然正常执行
    //在解释器并行执行时，部分已复制的字节码走检查安全点的逻辑，未复制的字节码继续走原来的逻辑，不检查安全点
  while (size-- > 0) *to++ = *from++;
}

//  notice_safepoints用于通知解释器进入安全点了
// 当达到安全点后，_active_table会被改成_safept_table
void TemplateInterpreter::notice_safepoints() {
  if (!_notice_safepoints) {
    // switch to safepoint dispatch table
      //如果_notice_safepoints为false，即未进入安全点，将_notice_safepoints置为true
    _notice_safepoints = true;
      //将_safept_table复制到_active_table，即执行字节码时会执行_safept_table中的对应字节码的执行逻辑
    copy_table((address*)&_safept_table, (address*)&_active_table, sizeof(_active_table) / sizeof(address));
  }
}

// switch from the dispatch table which notices safepoints back to the
// normal dispatch table.  So that we can notice single stepping points,
// keep the safepoint dispatch table if we are single stepping in JVMTI.
// Note that the should_post_single_step test is exactly as fast as the
// JvmtiExport::_enabled test and covers both cases.
// ignore_safepoints用于通知解释器退出安全点了
void TemplateInterpreter::ignore_safepoints() {
  if (_notice_safepoints) {
      //如果_notice_safepoints为true，表示已经进入安全点
    if (!JvmtiExport::should_post_single_step()) {
      // switch to normal dispatch table
        //将_notice_safepoints置为false
      _notice_safepoints = false;
        //将_normal_table复制到_active_table
      copy_table((address*)&_normal_table, (address*)&_active_table, sizeof(_active_table) / sizeof(address));
    }
  }
}

//------------------------------------------------------------------------------------------------------------------------
// Deoptimization support

// If deoptimization happens, this function returns the point of next bytecode to continue execution
address TemplateInterpreter::deopt_continue_after_entry(Method* method, address bcp, int callee_parameters, bool is_top_frame) {
  return AbstractInterpreter::deopt_continue_after_entry(method, bcp, callee_parameters, is_top_frame);
}

// If deoptimization happens, this function returns the point where the interpreter reexecutes
// the bytecode.
// Note: Bytecodes::_athrow (C1 only) and Bytecodes::_return are the special cases
//       that do not return "Interpreter::deopt_entry(vtos, 0)"
address TemplateInterpreter::deopt_reexecute_entry(Method* method, address bcp) {
  assert(method->contains(bcp), "just checkin'");
  Bytecodes::Code code   = Bytecodes::java_code_at(method, bcp);
  if (code == Bytecodes::_return) {
    // This is used for deopt during registration of finalizers
    // during Object.<init>.  We simply need to resume execution at
    // the standard return vtos bytecode to pop the frame normally.
    // reexecuting the real bytecode would cause double registration
    // of the finalizable object.
    return _normal_table.entry(Bytecodes::_return).entry(vtos);
  } else {
    return AbstractInterpreter::deopt_reexecute_entry(method, bcp);
  }
}

// If deoptimization happens, the interpreter should reexecute this bytecode.
// This function mainly helps the compilers to set up the reexecute bit.
bool TemplateInterpreter::bytecode_should_reexecute(Bytecodes::Code code) {
  if (code == Bytecodes::_return) {
    //Yes, we consider Bytecodes::_return as a special case of reexecution
    return true;
  } else {
    return AbstractInterpreter::bytecode_should_reexecute(code);
  }
}

#endif // !CC_INTERP
