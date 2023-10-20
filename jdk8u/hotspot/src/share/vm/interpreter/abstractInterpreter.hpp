/*
 * Copyright (c) 1997, 2015, Oracle and/or its affiliates. All rights reserved.
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

#ifndef SHARE_VM_INTERPRETER_ABSTRACTINTERPRETER_HPP
#define SHARE_VM_INTERPRETER_ABSTRACTINTERPRETER_HPP

#include "code/stubs.hpp"
#include "interpreter/bytecodes.hpp"
#include "runtime/thread.inline.hpp"
#include "runtime/vmThread.hpp"
#include "utilities/top.hpp"
#if defined INTERP_MASM_MD_HPP
# include INTERP_MASM_MD_HPP
#elif defined TARGET_ARCH_x86
# include "interp_masm_x86.hpp"
#elif defined TARGET_ARCH_MODEL_aarch64
# include "interp_masm_aarch64.hpp"
#elif defined TARGET_ARCH_MODEL_sparc
# include "interp_masm_sparc.hpp"
#elif defined TARGET_ARCH_MODEL_zero
# include "interp_masm_zero.hpp"
#elif defined TARGET_ARCH_MODEL_ppc_64
# include "interp_masm_ppc_64.hpp"
#endif

// This file contains the platform-independent parts
// of the abstract interpreter and the abstract interpreter generator.

// Organization of the interpreter(s). There exists two different interpreters in hotpot
// an assembly language version (aka template interpreter) and a high level language version
// (aka c++ interpreter). Th division of labor is as follows:

// Template Interpreter          C++ Interpreter        Functionality
//
// templateTable*                bytecodeInterpreter*   actual interpretation of bytecodes
//
// templateInterpreter*          cppInterpreter*        generation of assembly code that creates
//                                                      and manages interpreter runtime frames.
//                                                      Also code for populating interpreter
//                                                      frames created during deoptimization.
//
// For both template and c++ interpreter. There are common files for aspects of the interpreter
// that are generic to both interpreters. This is the layout:
//
// abstractInterpreter.hpp: generic description of the interpreter.
// interpreter*:            generic frame creation and handling.
//

//------------------------------------------------------------------------------------------------------------------------
// The C++ interface to the bytecode interpreter(s).

//  AbstractInterpreter的定义位于hotspot src/share/vm/interpreter/abstractInterpreter.hpp中，
// 是CppInterpreter和TemplateInterpreter共同的基类，用来抽象平台独立的解释器相关的属性和方法。AbstractInterpreter定义的属性都是protected

// 抽象解释器,这个抽象解释器描述了解释器的基本骨架:
// 所有的解释器(C++字节码解释器，模板解释器)都有这些例程和属性，然后子类的解释器还可以再扩展一些例程
// hotspot有一个C++字节码解释器，还有一个模板解释器 ，默认使用的是模板解释器的实现。这两个有什么区别呢？
// 举个例子，Java字节码有istore_0，iadd，如果是C++字节码解释器，那么它的工作流程就是这种：
// void cppInterpreter::work(){
//    for(int i=0;i<bytecode.length();i++){
//        switch(bytecode[i]){
//            case ISTORE_0:
//                int value = operandStack.pop();
//                localVar[0] = value;
//                break;
//            case IADD:
//                int v1 = operandStack.pop();
//                int v2 = operandStack.pop();
//                int res = v1+v2;
//                operandStack.push(res);
//                break;
//            ....
//        }
//    }
//}
// 它使用C++语言模拟字节码的执行，iadd是两个数相加，字节码解释器从栈上pop两个数据然后求和，再push到栈上。
//
//  如果是模板解释器就完全不一样了。模板解释器是一堆本地码的例程(routines)，它会在虚拟机创建的时候初始化好，
// 也就是说，模板解释器在初始化的时候会申请一片内存并设置为可读可写可执行，然后向那片内存写入本地码。在解释执行的时候遇到iadd，就执行那片内存里面的二进制代码。
//
//  这种运行时代码生成的机制可以说是JIT，只是通常意义的JIT是指对一块代码进行优化再生成本地代码，同一段代码可能因为分成编译产出不同的本地码，具有动态性；
// 而模板解释器是虚拟机在创建的时候JIT生成它自身，它的每个例程比如异常处理部分，安全点处理部分的本地码都是固定的，是静态的。

// hotspot默认使用解释+编译混合(-Xmixed)的方式执行代码。它首先使用模板解释器对字节码进行解释，当发现一段代码是热点的时候，就使用C1/C2 JIT进行优化编译再执行，
// 这也它的名字"热点"(hotspot)的由来。
class AbstractInterpreter: AllStatic {
  friend class VMStructs;
  friend class Interpreter;
  friend class CppInterpreterGenerator;
 public:
    // AbstractInterpreter定义了一个表示方法类型的枚举MethodKind，每个类型都对应_entry_table中一个数组元素，即一个处理该类型方法的方法调用的入口地址
    // 枚举在C/C++中其实就是一个int常量，默认情况下枚举值从0开始，依次往下递增，上述method_handle_invoke_LAST的定义比较特殊，
    // 正常method_handle_invoke_LAST应该比method_handle_invoke_FIRST大1，上述定义下就大vmIntrinsics::LAST_MH_SIG_POLY - vmIntrinsics::FIRST_MH_SIG_POLY，
    // 即method_handle_invoke_FIRST和method_handle_invoke_LAST之间实际还有几个未定义的但是合法的枚举值。

    // 为啥需要区分这么多种MethodKind了？主要是为了对特殊的MethodKind的方法做特殊处理，以获取最大的执行性能力，如这里的数学运算方法java.lang.Math.sin，
    // 计算文件签名的CRC32.update方法，可以直接使用汇编代码实现。
  enum MethodKind {
      // 普通的方法
    zerolocals,                                                 // method needs locals initialization
      // 普通的同步方法
    zerolocals_synchronized,                                    // method needs locals initialization & is synchronized
      // native方法
    native,                                                     // native method
      // native同步方法
    native_synchronized,                                        // native method & is synchronized
    empty,                                                      // empty method (code: _return)
    accessor,                                                   // accessor method (code: _aload_0, _getfield, _(a|i)return)
    abstract,                                                   // abstract method (throws an AbstractMethodException)
    method_handle_invoke_FIRST,                                 // java.lang.invoke.MethodHandles::invokeExact, etc.
    method_handle_invoke_LAST                                   = (method_handle_invoke_FIRST
                                                                   + (vmIntrinsics::LAST_MH_SIG_POLY
                                                                      - vmIntrinsics::FIRST_MH_SIG_POLY)),
    java_lang_math_sin,                                         // implementation of java.lang.Math.sin   (x)
    java_lang_math_cos,                                         // implementation of java.lang.Math.cos   (x)
    java_lang_math_tan,                                         // implementation of java.lang.Math.tan   (x)
    java_lang_math_abs,                                         // implementation of java.lang.Math.abs   (x)
    java_lang_math_sqrt,                                        // implementation of java.lang.Math.sqrt  (x)
    java_lang_math_log,                                         // implementation of java.lang.Math.log   (x)
    java_lang_math_log10,                                       // implementation of java.lang.Math.log10 (x)
    java_lang_math_pow,                                         // implementation of java.lang.Math.pow   (x,y)
    java_lang_math_exp,                                         // implementation of java.lang.Math.exp   (x)
    java_lang_ref_reference_get,                                // implementation of java.lang.ref.Reference.get()
    java_util_zip_CRC32_update,                                 // implementation of java.util.zip.CRC32.update()
    java_util_zip_CRC32_updateBytes,                            // implementation of java.util.zip.CRC32.updateBytes()
    java_util_zip_CRC32_updateByteBuffer,                       // implementation of java.util.zip.CRC32.updateByteBuffer()
    number_of_method_entries,
    invalid = -1
  };

  // Conversion from the part of the above enum to vmIntrinsics::_invokeExact, etc.
  static vmIntrinsics::ID method_handle_intrinsic(MethodKind kind) {
    if (kind >= method_handle_invoke_FIRST && kind <= method_handle_invoke_LAST)
      return (vmIntrinsics::ID)( vmIntrinsics::FIRST_MH_SIG_POLY + (kind - method_handle_invoke_FIRST) );
    else
      return vmIntrinsics::_none;
  }

  enum SomeConstants {
    number_of_result_handlers = 10                              // number of result handlers for native calls
  };

 protected:
    // 队列,用来保存生成的汇编代码的
  static StubQueue* _code;                                      // the interpreter code (codelets)

    // 是否激活了安全点机制
  static bool       _notice_safepoints;                         // true if safepoints are activated

    // JIT编译器产生的本地代码在内存中的起始位置
  static address    _native_entry_begin;                        // Region for native entry code
    // JIT编译器产生的本地代码在内存中的终止位置
  static address    _native_entry_end;

  // method entry points
    // _entry_table是AbstractInterpreter定义的一个protected address数组，数组长度是MethodKind的枚举值number_of_method_entries，
    // 即每个MethodKind都会有一个对应的表示方法执行入口地址的address。_entry_table属性的初始化就在TemplateInterpreterGenerator::generate_all()方法中完成

    // address数组，处理不同类型的方法的方法调用的入口地址，数组的长度就是枚举number_of_method_entries的值
    // number_of_method_entries表示方法类型的总数，使用方法类型做为数组下标就可以获取对应的方法入口
    // _entry_table也是个重要的属性，这个数组表示方法的例程，比如普通方法是入口点1_entry_table[0],带synchronized的方法是入口点2_entry_table[1]，
    // 这些_entry_table[0],_entry_table[1]指向的就是之前_code队列里面的小块例程
  static address    _entry_table[number_of_method_entries];     // entry points for a given method
    // address数组，处理不同类型的本地方法调用返回值的入口地址，数组的长度是枚举number_of_result_handlers的值，目前为10
  static address    _native_abi_to_tosca[number_of_result_handlers];  // for native method result handlers
    // 本地方法生成签名的入口地址
  static address    _slow_signature_handler;                              // the native method generic (slow) signature handler

    // 重新抛出异常的入口地址
  static address    _rethrow_exception_entry;                   // rethrows an activation in previous frame

  friend class      AbstractInterpreterGenerator;
  friend class              InterpreterGenerator;
  friend class      InterpreterMacroAssembler;

    // AbstractInterpreter定义的public方法可以分为以下几种：
    //
    // 方法类型操作相关的，如method_kind，entry_for_kind，set_entry_for_kind等
    // 解释器运行时支持相关的，如deopt_entry，deopt_continue_after_entry，deopt_reexecute_entry等
    // 本地方法调用支持相关的，如slow_signature_handler，result_handler，in_native_entry等
    // 解释器工具方法，如local_offset_in_bytes，oop_addr_in_slot，long_in_slot，set_long_in_slot等
 public:
  // Initialization/debugging
  static void       initialize();
  static StubQueue* code()                                      { return _code; }


  // Method activation
  static MethodKind method_kind(methodHandle m);
    // 这里直接返回了_entry_table数组中对应方法类型的entry_point地址。
    // 为了能尽快找到某个Java方法对应的entry_point入口，把这种对应关系保存到了_entry_table中，所以entry_for_kind()函数才能快速的获取到方法对应的entry_point入口。
    // 给数组中元素赋值专门有个方法：位置在hotspot/src/share/vm/interpreter/interpreter.cpp中
    // void AbstractInterpreter::set_entry_for_kind(AbstractInterpreter::MethodKind kind, address entry) {
    //  _entry_table[kind] = entry;
    // }

    // 这里涉及到Java方法的类型MethodKind，由于要通过entry_point进入Java世界，执行Java方法相关的逻辑，所以entry_point中一定会为对应的Java方法建立新的栈帧，
    // 但是不同方法的栈帧其实是有差别的，如Java普通方法、Java同步方法、有native关键字的Java方法等，所以就把所有的方法进行了归类，不同类型获取到不同的entry_point入口。
    // 到底有哪些类型，我们可以看一下MethodKind这个枚举类中定义出的枚举常量
    //
  static address    entry_for_kind(MethodKind k)                {
        //校验MethodKind是否合法，MethodKind是一个枚举，这里是检查枚举值
      assert(0 <= k && k < number_of_method_entries, "illegal kind");
      return _entry_table[k];
  }
    // 该方法完整的定义是static address    entry_for_method(methodHandle m)，用来获取解释器执行某个方法m的入口地址
    // 首先通过method_kind()函数拿到方法对应的类型，然后调用entry_for_kind()函数根据方法类型获取方法对应的入口entry_point
  static address    entry_for_method(methodHandle m)            { return entry_for_kind(method_kind(m)); }

  // used for bootstrapping method handles:
  static void       set_entry_for_kind(MethodKind k, address e);

  static void       print_method_kind(MethodKind kind)          PRODUCT_RETURN;

  static bool       can_be_compiled(methodHandle m);

  // Runtime support

  // length = invoke bytecode length (to advance to next bytecode)
  static address deopt_entry(TosState state, int length) { ShouldNotReachHere(); return NULL; }
  static address return_entry(TosState state, int length, Bytecodes::Code code) { ShouldNotReachHere(); return NULL; }

  static address    rethrow_exception_entry()                   { return _rethrow_exception_entry; }

  // Activation size in words for a method that is just being called.
  // Parameters haven't been pushed so count them too.
  static int        size_top_interpreter_activation(Method* method);

  // Deoptimization support
  // Compute the entry address for continuation after
  static address deopt_continue_after_entry(Method* method,
                                            address bcp,
                                            int callee_parameters,
                                            bool is_top_frame);
  // Compute the entry address for reexecution
  static address deopt_reexecute_entry(Method* method, address bcp);
  // Deoptimization should reexecute this bytecode
  static bool    bytecode_should_reexecute(Bytecodes::Code code);

  // deoptimization support
  static int        size_activation(int max_stack,
                                    int temps,
                                    int extra_args,
                                    int monitors,
                                    int callee_params,
                                    int callee_locals,
                                    bool is_top_frame);

  static void      layout_activation(Method* method,
                                     int temps,
                                     int popframe_args,
                                     int monitors,
                                     int caller_actual_parameters,
                                     int callee_params,
                                     int callee_locals,
                                     frame* caller,
                                     frame* interpreter_frame,
                                     bool is_top_frame,
                                     bool is_bottom_frame);

  // Runtime support
  static bool       is_not_reached(                       methodHandle method, int bci);
  // Safepoint support
  static void       notice_safepoints()                         { ShouldNotReachHere(); } // stops the thread when reaching a safepoint
  static void       ignore_safepoints()                         { ShouldNotReachHere(); } // ignores safepoints

  // Support for native calls
  static address    slow_signature_handler()                    { return _slow_signature_handler; }
  static address    result_handler(BasicType type)              { return _native_abi_to_tosca[BasicType_as_index(type)]; }
  static int        BasicType_as_index(BasicType type);         // computes index into result_handler_by_index table
  static bool       in_native_entry(address pc)                 { return _native_entry_begin <= pc && pc < _native_entry_end; }
  // Debugging/printing
  static void       print();                                    // prints the interpreter code

 public:
  // Interpreter helpers
  const static int stackElementWords   = 1;
  const static int stackElementSize    = stackElementWords * wordSize;
  const static int logStackElementSize = LogBytesPerWord;

  // Local values relative to locals[n]
  static int  local_offset_in_bytes(int n) {
    return ((frame::interpreter_frame_expression_stack_direction() * n) * stackElementSize);
  }

  // access to stacked values according to type:
  static oop* oop_addr_in_slot(intptr_t* slot_addr) {
    return (oop*) slot_addr;
  }
  static jint* int_addr_in_slot(intptr_t* slot_addr) {
    if ((int) sizeof(jint) < wordSize && !Bytes::is_Java_byte_ordering_different())
      // big-endian LP64
      return (jint*)(slot_addr + 1) - 1;
    else
      return (jint*) slot_addr;
  }
  static jlong long_in_slot(intptr_t* slot_addr) {
    if (sizeof(intptr_t) >= sizeof(jlong)) {
      return *(jlong*) slot_addr;
    } else {
      return Bytes::get_native_u8((address)slot_addr);
    }
  }
  static void set_long_in_slot(intptr_t* slot_addr, jlong value) {
    if (sizeof(intptr_t) >= sizeof(jlong)) {
      *(jlong*) slot_addr = value;
    } else {
      Bytes::put_native_u8((address)slot_addr, value);
    }
  }
  static void get_jvalue_in_slot(intptr_t* slot_addr, BasicType type, jvalue* value) {
    switch (type) {
    case T_BOOLEAN: value->z = *int_addr_in_slot(slot_addr);            break;
    case T_CHAR:    value->c = *int_addr_in_slot(slot_addr);            break;
    case T_BYTE:    value->b = *int_addr_in_slot(slot_addr);            break;
    case T_SHORT:   value->s = *int_addr_in_slot(slot_addr);            break;
    case T_INT:     value->i = *int_addr_in_slot(slot_addr);            break;
    case T_LONG:    value->j = long_in_slot(slot_addr);                 break;
    case T_FLOAT:   value->f = *(jfloat*)int_addr_in_slot(slot_addr);   break;
    case T_DOUBLE:  value->d = jdouble_cast(long_in_slot(slot_addr));   break;
    case T_OBJECT:  value->l = (jobject)*oop_addr_in_slot(slot_addr);   break;
    default:        ShouldNotReachHere();
    }
  }
  static void set_jvalue_in_slot(intptr_t* slot_addr, BasicType type, jvalue* value) {
    switch (type) {
    case T_BOOLEAN: *int_addr_in_slot(slot_addr) = (value->z != 0);     break;
    case T_CHAR:    *int_addr_in_slot(slot_addr) = value->c;            break;
    case T_BYTE:    *int_addr_in_slot(slot_addr) = value->b;            break;
    case T_SHORT:   *int_addr_in_slot(slot_addr) = value->s;            break;
    case T_INT:     *int_addr_in_slot(slot_addr) = value->i;            break;
    case T_LONG:    set_long_in_slot(slot_addr, value->j);              break;
    case T_FLOAT:   *(jfloat*)int_addr_in_slot(slot_addr) = value->f;   break;
    case T_DOUBLE:  set_long_in_slot(slot_addr, jlong_cast(value->d));  break;
    case T_OBJECT:  *oop_addr_in_slot(slot_addr) = (oop) value->l;      break;
    default:        ShouldNotReachHere();
    }
  }
};

//------------------------------------------------------------------------------------------------------------------------
// The interpreter generator.

class Template;
//  AbstractInterpreterGenerator的定义位于同目录下的abstractInterpreter.hpp中，只有一个属性InterpreterMacroAssembler* _masm，
//  即用来生成字节码的Assembler实例。AbstractInterpreterGenerator定义的方法都是protected方法
//  重点关注其构造方法和generate_all方法的实现
class AbstractInterpreterGenerator: public StackObj {
 protected:
  InterpreterMacroAssembler* _masm;

  // shared code sequences
  // Converter for native abi result to tosca result
  address generate_result_handler_for(BasicType type);
  address generate_slow_signature_handler();

  // entry point generator
  address generate_method_entry(AbstractInterpreter::MethodKind kind);

  void bang_stack_shadow_pages(bool native_call);

  void generate_all();
  void initialize_method_handle_entries();

 public:
  AbstractInterpreterGenerator(StubQueue* _code);
};

#endif // SHARE_VM_INTERPRETER_ABSTRACTINTERPRETER_HPP
