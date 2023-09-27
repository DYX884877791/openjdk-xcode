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

#ifndef SHARE_VM_INTERPRETER_TEMPLATEINTERPRETER_HPP
#define SHARE_VM_INTERPRETER_TEMPLATEINTERPRETER_HPP

#include "interpreter/abstractInterpreter.hpp"
#include "interpreter/templateTable.hpp"

// This file contains the platform-independent parts
// of the template interpreter and the template interpreter generator.

#ifndef CC_INTERP

//------------------------------------------------------------------------------------------------------------------------
// A little wrapper class to group tosca-specific entry points into a unit.
// (tosca = Top-Of-Stack CAche)

class EntryPoint VALUE_OBJ_CLASS_SPEC {
 private:
    // number_of_states是枚举TosState中表示state个数的枚举值
  address _entry[number_of_states];

 public:
  // Construction
  EntryPoint();
  EntryPoint(address bentry, address zentry, address centry, address sentry, address aentry, address ientry, address lentry, address fentry, address dentry, address ventry);

  // Attributes
  address entry(TosState state) const;                // return target address for a given tosca state
  void    set_entry(TosState state, address entry);   // set    target address for a given tosca state
  void    print();

  // Comparison
  bool operator == (const EntryPoint& y);             // for debugging only
};


//------------------------------------------------------------------------------------------------------------------------
// A little wrapper class to group tosca-specific dispatch tables into a unit.

class DispatchTable VALUE_OBJ_CLASS_SPEC {
 public:
    // BitsPerByte表示一个字节多少位，固定值8。
 // BitsPerByte的值为8
  enum { length = 1 << BitsPerByte };                 // an entry point for each byte value (also for undefined bytecodes)

 private:
 // number_of_states=9,length=256
 // _table是字节码分发表 ，address为u_char*类型的别名
 // _table是一个二维数组的表，维度为栈顶状态（共有9种）和字节码（最多有256个），存储的是每个栈顶状态对应的字节码的入口点
 // _table的一维为栈顶缓存状态，二维为Opcode，通过这2个维度能够找到一段机器指令，这就是根据当前的栈顶缓存状态定位到的字节码需要执行的机器指令片段。
  address _table[number_of_states][length];           // dispatch tables, indexed by tosca and bytecode

 public:
  // Attributes
  EntryPoint entry(int i) const;                      // return entry point for a given bytecode i
  void       set_entry(int i, EntryPoint& entry);     // set    entry point for a given bytecode i
  // address为u_char*类型的别名。_table是一个二维数组的表，维度为栈顶状态（共有9种）和字节码（最多有256个），存储的是每个栈顶状态对应的字节码的入口点
  address*   table_for(TosState state)          { return _table[state]; }
  address*   table_for()                        { return table_for((TosState)0); }
  int        distance_from(address *table)      { return table - table_for(); }
  int        distance_from(TosState state)      { return distance_from(table_for(state)); }

  // Comparison
  bool operator == (DispatchTable& y);                // for debugging only
};

//  模板解释器TemplateInterpreter又分为三个组成部分：
//      templateInterpreterGenerator 解释器生成器
//      templateTable 字节码实现
//      templateInterpreter 解释器

// TemplateInterpreter继承自AbstractInterpreter，其定义在同目录下的templateInterpreter.hpp中。
// TemplateInterpreter在此基础上增加了很多的完成特定功能的函数的调用入口
// TemplateInterpreter新增的方法主要有以下几种：
//
//  1. 获取特定TosState的调用入口相关的，如continuation，dispatch_table，invoke_return_entry_table等
//  2. 逆向优化相关的，如deopt_continue_after_entry，bytecode_should_reexecute等
// TemplateInterpreter中定义的都是平台无关的部分，跟平台相关的部分通过宏的方式引入
//
//
// 还有一个问题，这些例程是谁写入的呢？找一找架构图，下半部分都是解释器生成器，它的名字也是自解释的，那么它就是答案了。
//  前面刻意说道解释器布局就是想突出它只是一个骨架，要得到可运行的解释器还需要解释器生成器填充这个骨架。
//
//  解释器生成器本来可以独自完成填充工作，可能为了解耦，也可能是为了结构清晰，hotspot将字节码的例程抽了出来放到了templateTable(模板表)中，它辅助模板解释器生成器(templateInterpreterGenerator)完成各例程填充。
//
//  只有这两个还不能完成任务，因为组成模板解释器的是本地代码例程，本地代码例程依赖于操作系统和CPU，这部分代码位于hotspot/cpu/x86/中，所以
//
//templateInterpreter =
//    templateTable +
//    templateTable_x86 +
//    templateInterpreterGenerator +
//    templateInterpreterGenerator_x86 +
//    templateInterpreterGenerator_x86_64
// 虚拟机中有很多这样的设计：在hotspot/share/的某个头文件写出定义，在源文件实现OS/CPU无关的代码，然后在hotspot/cpu/x86中实现CPU相关的代码，在hostpot/os实现OS相关的代码。
class TemplateInterpreter: public AbstractInterpreter {
  friend class VMStructs;
  friend class InterpreterMacroAssembler;
  /**
   *
   */
  friend class TemplateInterpreterGenerator;
  friend class InterpreterGenerator;
  friend class TemplateTable;
  // friend class Interpreter;
 public:

  enum MoreConstants {
    number_of_return_entries  = number_of_states,               // number of return entry points
    number_of_deopt_entries   = number_of_states,               // number of deoptimization entry points
    number_of_return_addrs    = number_of_states                // number of return addresses
  };

  // 里面很多address变量,EntryPoint是一个address数组，DispatchTable也是。
  // 模板解释器就是由一系列例程(routine)组成的，即address变量，它们每个都表示一个例程的入口地址，比如异常处理例程，invoke指令例程，用于gc的safepoint例程...
  // 抽象解释器定义了必要的例程，具体的解释器在这之上还有自己的特设的例程。模板解释器就是一个例子，它继承自抽象解释器，在那些例程之上还有自己的特设例程：
 protected:
    // 数组越界异常例程
  static address    _throw_ArrayIndexOutOfBoundsException_entry;
    // 数组存储异常例程
  static address    _throw_ArrayStoreException_entry;
    // 算术异常例程
  static address    _throw_ArithmeticException_entry;
    // 类型转换异常例程
  static address    _throw_ClassCastException_entry;
  static address    _throw_WrongMethodType_entry;
    // 空指针异常例程
  static address    _throw_NullPointerException_entry;
    // 抛异常公共例程
  static address    _throw_exception_entry;

  static address    _throw_StackOverflowError_entry;

  static address    _remove_activation_entry;                   // continuation address if an exception is not handled by current frame
#ifdef HOTSWAP
  static address    _remove_activation_preserving_args_entry;   // continuation address when current frame is being popped
#endif // HOTSWAP

// EntryPoint就是一个address的数组的包装类
#ifndef PRODUCT
  static EntryPoint _trace_code;
#endif // !PRODUCT
  static EntryPoint _return_entry[number_of_return_entries];    // entry points to return to from a call
  static EntryPoint _earlyret_entry;                            // entry point to return early from a call
  static EntryPoint _deopt_entry[number_of_deopt_entries];      // entry points to return to from a deoptimization
  static EntryPoint _continuation_entry;
  static EntryPoint _safept_entry;

  static address _invoke_return_entry[number_of_return_addrs];           // for invokestatic, invokespecial, invokevirtual return entries
  static address _invokeinterface_return_entry[number_of_return_addrs];  // for invokeinterface return entries
  static address _invokedynamic_return_entry[number_of_return_addrs];    // for invokedynamic return entries
// DispatchTable是一个address二维数组的包装类
  static DispatchTable _active_table;                           // the active    dispatch table (used by the interpreter for dispatch)
  static DispatchTable _normal_table;                           // the normal    dispatch table (used to set the active table in normal mode)
  static DispatchTable _safept_table;                           // the safepoint dispatch table (used to set the active table for safepoints)
  static address       _wentry_point[DispatchTable::length];    // wide instructions only (vtos tosca always)


 public:
  // Initialization/debugging
  static void       initialize();
  // this only returns whether a pc is within generated code for the interpreter.
  static bool       contains(address pc)                        { return _code != NULL && _code->contains(pc); }

 public:

  static address    remove_activation_early_entry(TosState state) { return _earlyret_entry.entry(state); }
#ifdef HOTSWAP
  static address    remove_activation_preserving_args_entry()   { return _remove_activation_preserving_args_entry; }
#endif // HOTSWAP

  static address    remove_activation_entry()                   { return _remove_activation_entry; }
  static address    throw_exception_entry()                     { return _throw_exception_entry; }
  static address    throw_ArithmeticException_entry()           { return _throw_ArithmeticException_entry; }
  static address    throw_WrongMethodType_entry()               { return _throw_WrongMethodType_entry; }
  static address    throw_NullPointerException_entry()          { return _throw_NullPointerException_entry; }
  static address    throw_StackOverflowError_entry()            { return _throw_StackOverflowError_entry; }

  // Code generation
#ifndef PRODUCT
  static address    trace_code    (TosState state)              { return _trace_code.entry(state); }
#endif // !PRODUCT
  static address    continuation  (TosState state)              { return _continuation_entry.entry(state); }
  // 看下TemplateInterpreter的_active_table属性是如何初始化的，相关代码在TemplateInterpreter::initialize()中
  // 在_active_table中获取对应栈顶缓存状态的入口地址，_active_table变量定义在TemplateInterpreter类中
  static address*   dispatch_table(TosState state)              { return _active_table.table_for(state); }
  static address*   dispatch_table()                            { return _active_table.table_for(); }
  static int        distance_from_dispatch_table(TosState state){ return _active_table.distance_from(state); }
  static address*   normal_table(TosState state)                { return _normal_table.table_for(state); }
  static address*   normal_table()                              { return _normal_table.table_for(); }

  // Support for invokes
  static address*   invoke_return_entry_table()                 { return _invoke_return_entry; }
  static address*   invokeinterface_return_entry_table()        { return _invokeinterface_return_entry; }
  static address*   invokedynamic_return_entry_table()          { return _invokedynamic_return_entry; }
  static int        TosState_as_index(TosState state);

  static address* invoke_return_entry_table_for(Bytecodes::Code code);

  static address deopt_entry(TosState state, int length);
  static address return_entry(TosState state, int length, Bytecodes::Code code);

  // Safepoint support
  static void       notice_safepoints();                        // stops the thread when reaching a safepoint
  static void       ignore_safepoints();                        // ignores safepoints

  // Deoptimization support
  // Compute the entry address for continuation after
  static address deopt_continue_after_entry(Method* method,
                                            address bcp,
                                            int callee_parameters,
                                            bool is_top_frame);
  // Deoptimization should reexecute this bytecode
  static bool    bytecode_should_reexecute(Bytecodes::Code code);
  // Compute the address for reexecution
  static address deopt_reexecute_entry(Method* method, address bcp);

#ifdef TARGET_ARCH_x86
# include "templateInterpreter_x86.hpp"
#endif
#ifdef TARGET_ARCH_aarch64
# include "templateInterpreter_aarch64.hpp"
#endif
#ifdef TARGET_ARCH_sparc
# include "templateInterpreter_sparc.hpp"
#endif
#ifdef TARGET_ARCH_zero
# include "templateInterpreter_zero.hpp"
#endif
#ifdef TARGET_ARCH_arm
# include "templateInterpreter_arm.hpp"
#endif
#ifdef TARGET_ARCH_ppc
# include "templateInterpreter_ppc.hpp"
#endif


};

#endif // !CC_INTERP

#endif // SHARE_VM_INTERPRETER_TEMPLATEINTERPRETER_HPP
