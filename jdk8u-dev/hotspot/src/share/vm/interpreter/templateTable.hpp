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

#ifndef SHARE_VM_INTERPRETER_TEMPLATETABLE_HPP
#define SHARE_VM_INTERPRETER_TEMPLATETABLE_HPP

#include "interpreter/bytecodes.hpp"
#include "memory/allocation.hpp"
#include "runtime/frame.hpp"
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

#ifndef CC_INTERP
// All the necessary definitions used for (bytecode) template generation. Instead of
// spreading the implementation functionality for each bytecode in the interpreter
// and the snippet generator, a template is assigned to each bytecode which can be
// used to generate the bytecode's implementation if needed.


// A Template describes the properties of a code template for a given bytecode
// and provides a generator to generate the code template.
// Template的定义位于templateTable.hpp中，用来描述一个字节码模板的属性，并提供一个用来生成字节码模板的函数
class Template VALUE_OBJ_CLASS_SPEC {
 private:
  enum Flags {
    // 标志需要使用字节码指针（byte code pointer，数值为字节码基址+字节码偏移量）。表示生成的模板代码中是否需要使用指向字节码指令的指针，其实也就是说是否需要读取字节码指令的操作数，所以含有操作数的指令大部分都需要bcp，但是有一些是不需要的，如monitorenter与monitorexit等，这些的操作数都在表达式栈中，表达式栈顶就是其操作数，并不需要从Class文件中读取，所以不需要bcp；
    uses_bcp_bit,                                // set if template needs the bcp pointing to bytecode
    // 标志表示自己是否含有控制流转发逻辑，如tableswitch、lookupswitch、invokevirtual、ireturn等字节码指令，本身就需要进行控制流转发；
    does_dispatch_bit,                           // set if template dispatches on its own 就其本身而言; 靠自己
    // 标志是否需要调用JVM函数，在调用TemplateTable::call_VM()函数时都会判断是否有这个标志，通常方法调用JVM函数时都会通过调用TemplateTable::call_VM()函数来间接完成调用。JVM函数就是用C++写的函数。
    calls_vm_bit,                                // set if template calls the vm
    // 标志是否是wide指令（使用附加字节扩展全局变量索引）
    wide_bit                                     // set if template belongs to a wide instruction
  };

  typedef void (*generator)(int arg);

  // 用来描述字节码模板的属性，相关属性通过枚举Flags指定
  int       _flags;                              // describes interpreter template properties (bcp unknown)
  // 执行字节码指令前的栈顶值类型
  TosState  _tos_in;                             // tos cache state before template execution
  // 执行字节码指令后的栈顶值类型
  TosState  _tos_out;                            // tos cache state after  template execution
  // generator是用来生成字节码模板的函数的别名，其函数定义是typedef void (*generator)(int arg);
  generator _gen;                                // template code generator
  // 用来生成字节码模板的参数
  int       _arg;                                // argument for template code generator

  // Template定义的方法中除属性相关的如uses_bcp外，就三个方法initialize、bytecode、generate
  void      initialize(int flags, TosState tos_in, TosState tos_out, generator gen, int arg);

  friend class TemplateTable;

 public:
  Bytecodes::Code bytecode() const;
  bool      is_valid() const                     { return _gen != NULL; }
  bool      uses_bcp() const                     { return (_flags & (1 << uses_bcp_bit     )) != 0; }
  bool      does_dispatch() const                { return (_flags & (1 << does_dispatch_bit)) != 0; }
  bool      calls_vm() const                     { return (_flags & (1 << calls_vm_bit     )) != 0; }
  bool      is_wide() const                      { return (_flags & (1 << wide_bit         )) != 0; }
  TosState  tos_in() const                       { return _tos_in; }
  TosState  tos_out() const                      { return _tos_out; }
  void      generate(InterpreterMacroAssembler* masm);
};


// The TemplateTable defines all Templates and provides accessor functions
// to get the template for a given bytecode.
// OpenJDK的模板解释器会维护一个模板表，这个模板表会建立一个opcode到machine code的对应关系，
// 模板表TemplateTable保存了各个字节码的模板（目标代码生成函数和参数），即各个字节码转换机器码片段的模板，通过他的函数名称就知道是和字节码指令 opcode 是对应的。
//
// OpenJDK模板解释器中的模板表实现分为两部分：一部分是架构无关的公共代码，主要位于src/hotspot/share/interpreter/templateTable.hpp
// 和src/hotspot/share/interpreter/templateTable.cpp中；
// 一部分是架构相关的代码，主要位于hotspot/src/cpu/x86/vm/templateTable_x86_64.hpp和hotspot/src/cpu/x86/vm/templateTable_x86_64.cpp之中。
// 这两者结合起来构成了一个完整的模板表。公共代码部分主要是类的整体实现，包含了类的初始化等；
// 平台相关的代码主要是包含了模板表中具体opcode所对应的生成函数，这个生成函数可以为对应的opcode生成machine code，
// 这也就是模板表所建立的从opcode到machine code的对应关系——为每个opcode做一个生成函数来生成对应的machine code。所以，在不同平台进行OpenJDK移植的时候，主要是关心目标平台相关的这部分模板表。
//
//目标平台相关的模板表中包含了具体指令的生成函数，生成函数一般是用目标平台的汇编语言所编写，最终会生成机器码。
// TemplateTable的定义位于同目录的templateTable.hpp中，表示字节码指令的模板类，定义了所有指令的指令模板，并提供了获取给定字节码指令的模板的方法
// 跟平台相关的方法定义通过宏的方式引入
class TemplateTable: AllStatic {
 public:
  enum Operation { add, sub, mul, div, rem, _and, _or, _xor, shl, shr, ushr };
  enum Condition { equal, not_equal, less, less_equal, greater, greater_equal };
  enum CacheByte { f1_byte = 1, f2_byte = 2 };  // byte_no codes

 private:
    // 是否完成初始化
  static bool            _is_initialized;        // true if TemplateTable has been initialized
    // Template数组，表示各字节码指令对应的Template
  static Template        _template_table     [Bytecodes::number_of_codes];
    // Template数组，宽字节下的各字节码指令对应的Template
  static Template        _template_table_wide[Bytecodes::number_of_codes];
    // 当前正在生成的Template模板
  static Template*       _desc;                  // the current template to be generated
  static Bytecodes::Code bytecode()              { return _desc->bytecode(); }
    // 关联的BarrierSet实例
  static BarrierSet*     _bs;                    // Cache the barrier set.
 public:
  //%note templates_1
  // 用来生成字节码模板的Assembler实例
  static InterpreterMacroAssembler* _masm;       // the assembler used when generating templates

 private:

  // special registers
  static inline Address at_bcp(int offset);

  // helpers
  static void unimplemented_bc();
  static void patch_bytecode(Bytecodes::Code bc, Register bc_reg,
                             Register temp_reg, bool load_bc_into_bc_reg = true, int byte_no = -1);

  // C calls
  static void call_VM(Register oop_result, address entry_point);
  static void call_VM(Register oop_result, address entry_point, Register arg_1);
  static void call_VM(Register oop_result, address entry_point, Register arg_1, Register arg_2);
  static void call_VM(Register oop_result, address entry_point, Register arg_1, Register arg_2, Register arg_3);

  // these overloadings are not presently used on SPARC:
  static void call_VM(Register oop_result, Register last_java_sp, address entry_point);
  static void call_VM(Register oop_result, Register last_java_sp, address entry_point, Register arg_1);
  static void call_VM(Register oop_result, Register last_java_sp, address entry_point, Register arg_1, Register arg_2);
  static void call_VM(Register oop_result, Register last_java_sp, address entry_point, Register arg_1, Register arg_2, Register arg_3);

  // 私有方法大部分都是跟指令对应的方法,如下：
  // bytecodes
  static void nop();

  static void aconst_null();
  static void iconst(int value);
  static void lconst(int value);
  static void fconst(int value);
  static void dconst(int value);

  static void bipush();
  static void sipush();
  static void ldc(bool wide);
  static void ldc2_w();
  static void fast_aldc(bool wide);

  static void locals_index(Register reg, int offset = 1);
  static void iload();
  static void fast_iload();
  static void fast_iload2();
  static void fast_icaload();
  static void lload();
  static void fload();
  static void dload();
  static void aload();

  static void locals_index_wide(Register reg);
  static void wide_iload();
  static void wide_lload();
  static void wide_fload();
  static void wide_dload();
  static void wide_aload();

  static void iaload();
  static void laload();
  static void faload();
  static void daload();
  static void aaload();
  static void baload();
  static void caload();
  static void saload();

  static void iload(int n);
  static void lload(int n);
  static void fload(int n);
  static void dload(int n);
  static void aload(int n);
  static void aload_0();

  static void istore();
  static void lstore();
  static void fstore();
  static void dstore();
  static void astore();

  static void wide_istore();
  static void wide_lstore();
  static void wide_fstore();
  static void wide_dstore();
  static void wide_astore();

  static void iastore();
  static void lastore();
  static void fastore();
  static void dastore();
  static void aastore();
  static void bastore();
  static void castore();
  static void sastore();

  static void istore(int n);
  static void lstore(int n);
  static void fstore(int n);
  static void dstore(int n);
  static void astore(int n);

  static void pop();
  static void pop2();
  static void dup();
  static void dup_x1();
  static void dup_x2();
  static void dup2();
  static void dup2_x1();
  static void dup2_x2();
  static void swap();

  static void iop2(Operation op);
  static void lop2(Operation op);
  static void fop2(Operation op);
  static void dop2(Operation op);

  static void idiv();
  static void irem();

  static void lmul();
  static void ldiv();
  static void lrem();
  static void lshl();
  static void lshr();
  static void lushr();

  static void ineg();
  static void lneg();
  static void fneg();
  static void dneg();

  static void iinc();
  static void wide_iinc();
  static void convert();
  static void lcmp();

  static void float_cmp (bool is_float, int unordered_result);
  static void float_cmp (int unordered_result);
  static void double_cmp(int unordered_result);

  static void count_calls(Register method, Register temp);
  static void branch(bool is_jsr, bool is_wide);
  static void if_0cmp   (Condition cc);
  static void if_icmp   (Condition cc);
  static void if_nullcmp(Condition cc);
  static void if_acmp   (Condition cc);

  static void _goto();
  static void jsr();
  static void ret();
  static void wide_ret();

  static void goto_w();
  static void jsr_w();

  static void tableswitch();
  static void lookupswitch();
  static void fast_linearswitch();
  static void fast_binaryswitch();

  static void _return(TosState state);

  static void resolve_cache_and_index(int byte_no,       // one of 1,2,11
                                      Register cache,    // output for CP cache
                                      Register index,    // output for CP index
                                      size_t index_size); // one of 1,2,4
  static void load_invoke_cp_cache_entry(int byte_no,
                                         Register method,
                                         Register itable_index,
                                         Register flags,
                                         bool is_invokevirtual,
                                         bool is_virtual_final,
                                         bool is_invokedynamic);
  static void load_field_cp_cache_entry(Register obj,
                                        Register cache,
                                        Register index,
                                        Register offset,
                                        Register flags,
                                        bool is_static);
  static void invokevirtual(int byte_no);
  static void invokespecial(int byte_no);
  static void invokestatic(int byte_no);
  static void invokeinterface(int byte_no);
  static void invokedynamic(int byte_no);
  static void invokehandle(int byte_no);
  static void fast_invokevfinal(int byte_no);

  static void getfield_or_static(int byte_no, bool is_static);
  static void putfield_or_static(int byte_no, bool is_static);
  static void getfield(int byte_no);
  static void putfield(int byte_no);
  static void getstatic(int byte_no);
  static void putstatic(int byte_no);
  static void pop_and_check_object(Register obj);

  static void _new();
  static void newarray();
  static void anewarray();
  static void arraylength();
  static void checkcast();
  static void instanceof();

  static void athrow();

  static void monitorenter();
  static void monitorexit();

  static void wide();
  static void multianewarray();

  static void fast_xaccess(TosState state);
  static void fast_accessfield(TosState state);
  static void fast_storefield(TosState state);

  static void _breakpoint();

  static void shouldnotreachhere();

  // jvmti support
  static void jvmti_post_field_access(Register cache, Register index, bool is_static, bool has_tos);
  static void jvmti_post_field_mod(Register cache, Register index, bool is_static);
  static void jvmti_post_fast_field_mod();

  // debugging of TemplateGenerator
  static void transition(TosState tos_in, TosState tos_out);// checks if in/out states expected by template generator correspond to table entries

  // initialization helpers
  static void def(Bytecodes::Code code, int flags, TosState in, TosState out, void (*gen)(            ), char filler );
  static void def(Bytecodes::Code code, int flags, TosState in, TosState out, void (*gen)(int arg     ), int arg     );
 static void def(Bytecodes::Code code, int flags, TosState in, TosState out, void (*gen)(bool arg    ), bool arg    );
  static void def(Bytecodes::Code code, int flags, TosState in, TosState out, void (*gen)(TosState tos), TosState tos);
  static void def(Bytecodes::Code code, int flags, TosState in, TosState out, void (*gen)(Operation op), Operation op);
  static void def(Bytecodes::Code code, int flags, TosState in, TosState out, void (*gen)(Condition cc), Condition cc);

  friend class Template;

  // InterpreterMacroAssembler::is_a(), etc., need TemplateTable::call_VM().
  friend class InterpreterMacroAssembler;

 public:
    //  对外public static方法就4个
  // Initialization
  static void initialize();
  static void pd_initialize();

  // Templates
    //先检查是否是非宽字节指令，如果是则返回对应的Template
  static Template* template_for     (Bytecodes::Code code)  { Bytecodes::check     (code); return &_template_table     [code]; }
  static Template* template_for_wide(Bytecodes::Code code)  { Bytecodes::wide_check(code); return &_template_table_wide[code]; }

  // Platform specifics
#if defined TEMPLATETABLE_MD_HPP
# include TEMPLATETABLE_MD_HPP
#elif defined TARGET_ARCH_MODEL_x86_32
# include "templateTable_x86_32.hpp"
#elif defined TARGET_ARCH_MODEL_x86_64
# include "templateTable_x86_64.hpp"
#elif defined TARGET_ARCH_MODEL_aarch64
# include "templateTable_aarch64.hpp"
#elif defined TARGET_ARCH_MODEL_sparc
# include "templateTable_sparc.hpp"
#elif defined TARGET_ARCH_MODEL_zero
# include "templateTable_zero.hpp"
#elif defined TARGET_ARCH_MODEL_ppc_64
# include "templateTable_ppc_64.hpp"
#endif

};
#endif /* !CC_INTERP */

#endif // SHARE_VM_INTERPRETER_TEMPLATETABLE_HPP
