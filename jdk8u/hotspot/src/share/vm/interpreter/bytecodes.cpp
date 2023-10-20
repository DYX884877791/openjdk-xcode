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
#include "interpreter/bytecodes.hpp"
#include "memory/resourceArea.hpp"
#include "oops/method.hpp"
#ifdef TARGET_ARCH_x86
# include "bytes_x86.hpp"
#endif
#ifdef TARGET_ARCH_aarch64
# include "bytes_aarch64.hpp"
#endif
#ifdef TARGET_ARCH_sparc
# include "bytes_sparc.hpp"
#endif
#ifdef TARGET_ARCH_zero
# include "bytes_zero.hpp"
#endif
#ifdef TARGET_ARCH_arm
# include "bytes_arm.hpp"
#endif
#ifdef TARGET_ARCH_ppc
# include "bytes_ppc.hpp"
#endif


#if defined(WIN32) && (defined(_MSC_VER) && (_MSC_VER < 1600))
// Windows AMD64 Compiler Hangs compiling this file
// unless optimization is off
#ifdef _M_AMD64
#pragma optimize ("", off)
#endif
#endif


bool            Bytecodes::_is_initialized = false;
// Bytecodes::number_of_codes的值为234，足够存储所有的字节码指令了（包含虚拟机内部扩展的指令）。
const char*     Bytecodes::_name          [Bytecodes::number_of_codes];
BasicType       Bytecodes::_result_type   [Bytecodes::number_of_codes];
s_char          Bytecodes::_depth         [Bytecodes::number_of_codes];
u_char          Bytecodes::_lengths       [Bytecodes::number_of_codes];
Bytecodes::Code Bytecodes::_java_code     [Bytecodes::number_of_codes];
u_short         Bytecodes::_flags         [(1<<BitsPerByte)*2];

#ifdef ASSERT
bool Bytecodes::check_method(const Method* method, address bcp) {
  return method->contains(bcp);
}
#endif

bool Bytecodes::check_must_rewrite(Bytecodes::Code code) {
  assert(can_rewrite(code), "post-check only");

  // Some codes are conditionally rewriting.  Look closely at them.
  switch (code) {
  case Bytecodes::_aload_0:
    // Even if RewriteFrequentPairs is turned on,
    // the _aload_0 code might delay its rewrite until
    // a following _getfield rewrites itself.
    return false;

  case Bytecodes::_lookupswitch:
    return false;  // the rewrite is not done by the interpreter

  case Bytecodes::_new:
    // (Could actually look at the class here, but the profit would be small.)
    return false;  // the rewrite is not always done
  }

  // No other special cases.
  return true;
}

Bytecodes::Code Bytecodes::code_at(Method* method, int bci) {
  return code_at(method, method->bcp_from(bci));
}

Bytecodes::Code Bytecodes::non_breakpoint_code_at(const Method* method, address bcp) {
  assert(method != NULL, "must have the method for breakpoint conversion");
  assert(method->contains(bcp), "must be valid bcp in method");
  return method->orig_bytecode_at(method->bci_from(bcp));
}

int Bytecodes::special_length_at(Bytecodes::Code code, address bcp, address end) {
  switch (code) {
  case _wide:
    if (end != NULL && bcp + 1 >= end) {
      return -1; // don't read past end of code buffer
    }
    return wide_length_for(cast(*(bcp + 1)));
  case _tableswitch:
    { address aligned_bcp = (address)round_to((intptr_t)bcp + 1, jintSize);
      if (end != NULL && aligned_bcp + 3*jintSize >= end) {
        return -1; // don't read past end of code buffer
      }
      jlong lo = (jint)Bytes::get_Java_u4(aligned_bcp + 1*jintSize);
      jlong hi = (jint)Bytes::get_Java_u4(aligned_bcp + 2*jintSize);
      jlong len = (aligned_bcp - bcp) + (3 + hi - lo + 1)*jintSize;
      // only return len if it can be represented as a positive int;
      // return -1 otherwise
      return (len > 0 && len == (int)len) ? len : -1;
    }

  case _lookupswitch:      // fall through
  case _fast_binaryswitch: // fall through
  case _fast_linearswitch:
    { address aligned_bcp = (address)round_to((intptr_t)bcp + 1, jintSize);
      if (end != NULL && aligned_bcp + 2*jintSize >= end) {
        return -1; // don't read past end of code buffer
      }
      jlong npairs = (jint)Bytes::get_Java_u4(aligned_bcp + jintSize);
      jlong len = (aligned_bcp - bcp) + (2 + 2*npairs)*jintSize;
      // only return len if it can be represented as a positive int;
      // return -1 otherwise
      return (len > 0 && len == (int)len) ? len : -1;
    }
  }
  // Note: Length functions must return <=0 for invalid bytecodes.
  return 0;
}

// At a breakpoint instruction, this returns the breakpoint's length,
// otherwise, it's the same as special_length_at().  This is used by
// the RawByteCodeStream, which wants to see the actual bytecode
// values (including breakpoint).  RawByteCodeStream is used by the
// verifier when reading in bytecode to verify.  Other mechanisms that
// run at runtime (such as generateOopMaps) need to iterate over the code
// and don't expect to see breakpoints: they want to see the instruction
// which was replaced so that they can get the correct length and find
// the next bytecode.
//
// 'end' indicates the end of the code buffer, which we should not try to read
// past.
int Bytecodes::raw_special_length_at(address bcp, address end) {
  Code code = code_or_bp_at(bcp);
  if (code == _breakpoint) {
    return 1;
  } else {
    return special_length_at(code, bcp, end);
  }
}



void Bytecodes::def(Code code, const char* name, const char* format, const char* wide_format, BasicType result_type, int depth, bool can_trap) {
  def(code, name, format, wide_format, result_type, depth, can_trap, code);
}


void Bytecodes::def(Code code, const char* name, const char* format, const char* wide_format, BasicType result_type, int depth, bool can_trap, Code java_code) {
  assert(wide_format == NULL || format != NULL, "short form must exist if there's a wide form");
  int len  = (format      != NULL ? (int) strlen(format)      : 0);
  int wlen = (wide_format != NULL ? (int) strlen(wide_format) : 0);
    // _name、_result_type等都是在Bytecodes类中定义的静态数组，其下标为Opcode值，而存储的值就是name、result_type等
  _name          [code] = name;
  _result_type   [code] = result_type;
  _depth         [code] = depth;
    // 0xF的二进制值为1111
  _lengths       [code] = (wlen << 4) | (len & 0xF);
  _java_code     [code] = java_code;
  int bc_flags = 0;
    // ldc、ldc_w、ldc2_w、_aload_0、iaload、iastore、idiv、ldiv、ireturn等
    // 字节码指令都会含有_bc_can_trap
  if (can_trap)           bc_flags |= _bc_can_trap;
    // 虚拟机内部定义的指令都会有_bc_can_rewrite
  if (java_code != code)  bc_flags |= _bc_can_rewrite;
    // 在这里对_flags赋值操作
  _flags[(u1)code+0*(1<<BitsPerByte)] = compute_flags(format,      bc_flags);
  _flags[(u1)code+1*(1<<BitsPerByte)] = compute_flags(wide_format, bc_flags);
  assert(is_defined(code)      == (format != NULL),      "");
  assert(wide_is_defined(code) == (wide_format != NULL), "");
  assert(length_for(code)      == len, "");
  assert(wide_length_for(code) == wlen, "");
}


// Format strings interpretation:
//
// b: bytecode
// c: signed constant, Java byte-ordering
// i: unsigned local index, Java byte-ordering (I = native byte ordering)
// j: unsigned CP cache index, Java byte-ordering (J = native byte ordering)
// k: unsigned CP index, Java byte-ordering
// o: branch offset, Java byte-ordering
// _: unused/ignored
// w: wide bytecode
//
// Note: The format strings are used for 2 purposes:
//       1. to specify the length of the bytecode
//          (= number of characters in format string)
//       2. to derive bytecode format flags (_fmt_has_k, etc.)
//
// Note: For bytecodes with variable length, the format string is the empty string.

// 调用compute_flags()函数: 根据传入的wide_format和format来计算字节码的一些属性，然后存储到高8位和低8位中
// 函数要根据wide_format和format来计算flags的值，通过flags中的值能够表示字节码的b、c、i、j、k、o、w（在之前介绍format时介绍过）和字节码操作数的大小（操作数是2字节还是4字节）。以_fmt开头的一些变量在枚举类中已经定义
int Bytecodes::compute_flags(const char* format, int more_flags) {
  if (format == NULL)  return 0;  // not even more_flags
  int flags = more_flags;
  const char* fp = format;
  switch (*fp) {
  case '\0':
    flags |= _fmt_not_simple; // but variable
    break;
  case 'b':
    flags |= _fmt_not_variable;  // but simple
    ++fp;  // skip 'b'
    break;
  case 'w':
    flags |= _fmt_not_variable | _fmt_not_simple;
    ++fp;  // skip 'w'
    guarantee(*fp == 'b', "wide format must start with 'wb'");
    ++fp;  // skip 'b'
    break;
  }

  int has_nbo = 0, has_jbo = 0, has_size = 0;
  for (;;) {
    int this_flag = 0;
    char fc = *fp++;
    switch (fc) {
    case '\0':  // end of string
      assert(flags == (jchar)flags, "change _format_flags");
      return flags;

    case '_': continue;         // ignore these

    case 'j': this_flag = _fmt_has_j; has_jbo = 1; break;
    case 'k': this_flag = _fmt_has_k; has_jbo = 1; break;
    case 'i': this_flag = _fmt_has_i; has_jbo = 1; break;
    case 'c': this_flag = _fmt_has_c; has_jbo = 1; break;
    case 'o': this_flag = _fmt_has_o; has_jbo = 1; break;

    // uppercase versions mark native byte order (from Rewriter)
    // actually, only the 'J' case happens currently
    case 'J': this_flag = _fmt_has_j; has_nbo = 1; break;
    case 'K': this_flag = _fmt_has_k; has_nbo = 1; break;
    case 'I': this_flag = _fmt_has_i; has_nbo = 1; break;
    case 'C': this_flag = _fmt_has_c; has_nbo = 1; break;
    case 'O': this_flag = _fmt_has_o; has_nbo = 1; break;
    default:  guarantee(false, "bad char in format");
    }

    flags |= this_flag;

    guarantee(!(has_jbo && has_nbo), "mixed byte orders in format");
    if (has_nbo)
      flags |= _fmt_has_nbo;

    int this_size = 1;
    if (*fp == fc) {
      // advance beyond run of the same characters
      this_size = 2;
      while (*++fp == fc)  this_size++;
      switch (this_size) {
          // 如sipush、ldc_w、ldc2_w、wide iload等
      case 2: flags |= _fmt_has_u2; break;
          // 如goto_w和invokedynamic指令
      case 4: flags |= _fmt_has_u4; break;
      default: guarantee(false, "bad rep count in format");
      }
    }
    guarantee(has_size == 0 ||                     // no field yet
              this_size == has_size ||             // same size
              this_size < has_size && *fp == '\0', // last field can be short
              "mixed field sizes in format");
    has_size = this_size;
  }
}

// 字节码指令的定义。在Bytecodes::initialize()函数中会定义字节码指令的一些属性
// 现在Java虚拟机规范定义的202个字节码指令都会在这里调用def()函数进行定义，我们需要重点关注调用def()函数时传递的参数bytecode name、format等
void Bytecodes::initialize() {
  if (_is_initialized) return;
  assert(number_of_codes <= 256, "too many bytecodes");

  // initialize bytecode tables - didn't use static array initializers
  // (such as {}) so we can do additional consistency checks and init-
  // code is independent of actual bytecode numbering.
  //
  // Note 1: NULL for the format string means the bytecode doesn't exist
  //         in that form.
  //
  // Note 2: The result type is T_ILLEGAL for bytecodes where the top of stack
  //         type after execution is not only determined by the bytecode itself.

  // bytecode name就是字节码名称；
  // wide表示字节码前面是否可以加wide，如果可以，则值为"wbii"；
  // result tp表示指令执行后的结果类型，如为T_ILLEGAL时，表示只参考当前字节码无法决定执行结果的类型，如_invokevirtual方法调用指令，结果类型应该为方法返回类型，但是此时只参考这个调用方法的字节码指令是无法决定的；
  // stk表示对表达式栈深度的影响，如_nop指令不执行任何操作，所以对表达式栈的深度无影响，stk的值为0；当用_iconst_0向栈中压入0时，栈的深度增加1，所以stk的值为1。当为_lconst_0时，栈的深度会增加2；当为_lstore_0时，栈的深度会减少2；
  // traps表示can_trap，这个比较重要，在后面会详细介绍。
  // format，这个属性能表达2个意思，首先能表达字节码的格式，另外还能表示字节码的长度。
  // 重点介绍一下format这个参数。format表示字节码的格式，当字符串中有一个字符时就是一个字节长度的字节码，当为2个字符时就是2个字节长度的字节码...，如_iconst_0就是一个字节宽度的字节码，_istore的format为"bi"，所以是2个字节宽度。format还可能为空字符串，当为空字符串时，表示当前的字节码不是Java虚拟机规范中定义的字节码，如为了提高解释执行效率的_fast_agetfield、_fast_bgetfield等字节码，这些字节码是虚拟机内部定义的。还能表达字节码的格式，其中的字符串中各个字符的含义如下：
  //
  //  b： 表示字节码指令是非可变长度的，所以对于tableswitch、lookupswitch这种可变长度的指令来说，format字符串中不会含有b字符；
  //
  //  c：操作数为有符号的常量，如bipush指令将byte带符号扩展为一个int类型的值，然后将这个值入栈到操作数栈中；
  //
  //  i：操作数为无符号的本地变量表索引值，如iload指令从局部变量表加载一个int类型的值到操作数栈中；
  //
  //  j：操作数为常量池缓存的索引，注意常量池缓存索引不同与常量池索引，关于常量池索引，在《深入剖析Java虚拟机：源码剖析与实例详解》基础卷中详细介绍过，这里不再介绍；
  //
  //  k：操作数为无符号的常量池索引，如ldc指令将从运行时常量池中提取数据并压入操作数栈，所以格式为"bk"；
  //
  //  o：操作数为分支偏移，如ifeq表示整数与零比较，如果整数为0，则比较结果为真，将操作数看为分支偏移量进行跳转，所以格式为”boo“；
  //
  //  _：可直接忽略
  //
  //  w：可用来扩展局部变量表索引的字节码，这些字节码有iload、fload等，所以wild的值为"wbii"；
  //  Java bytecodes
  //  bytecode               bytecode name           format   wide f.   result tp  stk traps
  def(_nop                 , "nop"                 , "b"    , NULL    , T_VOID   ,  0, false);
  def(_aconst_null         , "aconst_null"         , "b"    , NULL    , T_OBJECT ,  1, false);
  def(_iconst_m1           , "iconst_m1"           , "b"    , NULL    , T_INT    ,  1, false);
  def(_iconst_0            , "iconst_0"            , "b"    , NULL    , T_INT    ,  1, false);
  def(_iconst_1            , "iconst_1"            , "b"    , NULL    , T_INT    ,  1, false);
  def(_iconst_2            , "iconst_2"            , "b"    , NULL    , T_INT    ,  1, false);
  def(_iconst_3            , "iconst_3"            , "b"    , NULL    , T_INT    ,  1, false);
  def(_iconst_4            , "iconst_4"            , "b"    , NULL    , T_INT    ,  1, false);
  def(_iconst_5            , "iconst_5"            , "b"    , NULL    , T_INT    ,  1, false);
  def(_lconst_0            , "lconst_0"            , "b"    , NULL    , T_LONG   ,  2, false);
  def(_lconst_1            , "lconst_1"            , "b"    , NULL    , T_LONG   ,  2, false);
  def(_fconst_0            , "fconst_0"            , "b"    , NULL    , T_FLOAT  ,  1, false);
  def(_fconst_1            , "fconst_1"            , "b"    , NULL    , T_FLOAT  ,  1, false);
  def(_fconst_2            , "fconst_2"            , "b"    , NULL    , T_FLOAT  ,  1, false);
  def(_dconst_0            , "dconst_0"            , "b"    , NULL    , T_DOUBLE ,  2, false);
  def(_dconst_1            , "dconst_1"            , "b"    , NULL    , T_DOUBLE ,  2, false);
  def(_bipush              , "bipush"              , "bc"   , NULL    , T_INT    ,  1, false);
  def(_sipush              , "sipush"              , "bcc"  , NULL    , T_INT    ,  1, false);
  def(_ldc                 , "ldc"                 , "bk"   , NULL    , T_ILLEGAL,  1, true );
  def(_ldc_w               , "ldc_w"               , "bkk"  , NULL    , T_ILLEGAL,  1, true );
  def(_ldc2_w              , "ldc2_w"              , "bkk"  , NULL    , T_ILLEGAL,  2, true );
  def(_iload               , "iload"               , "bi"   , "wbii"  , T_INT    ,  1, false);
  def(_lload               , "lload"               , "bi"   , "wbii"  , T_LONG   ,  2, false);
  def(_fload               , "fload"               , "bi"   , "wbii"  , T_FLOAT  ,  1, false);
  def(_dload               , "dload"               , "bi"   , "wbii"  , T_DOUBLE ,  2, false);
  def(_aload               , "aload"               , "bi"   , "wbii"  , T_OBJECT ,  1, false);
  def(_iload_0             , "iload_0"             , "b"    , NULL    , T_INT    ,  1, false);
  def(_iload_1             , "iload_1"             , "b"    , NULL    , T_INT    ,  1, false);
  def(_iload_2             , "iload_2"             , "b"    , NULL    , T_INT    ,  1, false);
  def(_iload_3             , "iload_3"             , "b"    , NULL    , T_INT    ,  1, false);
  def(_lload_0             , "lload_0"             , "b"    , NULL    , T_LONG   ,  2, false);
  def(_lload_1             , "lload_1"             , "b"    , NULL    , T_LONG   ,  2, false);
  def(_lload_2             , "lload_2"             , "b"    , NULL    , T_LONG   ,  2, false);
  def(_lload_3             , "lload_3"             , "b"    , NULL    , T_LONG   ,  2, false);
  def(_fload_0             , "fload_0"             , "b"    , NULL    , T_FLOAT  ,  1, false);
  def(_fload_1             , "fload_1"             , "b"    , NULL    , T_FLOAT  ,  1, false);
  def(_fload_2             , "fload_2"             , "b"    , NULL    , T_FLOAT  ,  1, false);
  def(_fload_3             , "fload_3"             , "b"    , NULL    , T_FLOAT  ,  1, false);
  def(_dload_0             , "dload_0"             , "b"    , NULL    , T_DOUBLE ,  2, false);
  def(_dload_1             , "dload_1"             , "b"    , NULL    , T_DOUBLE ,  2, false);
  def(_dload_2             , "dload_2"             , "b"    , NULL    , T_DOUBLE ,  2, false);
  def(_dload_3             , "dload_3"             , "b"    , NULL    , T_DOUBLE ,  2, false);
  def(_aload_0             , "aload_0"             , "b"    , NULL    , T_OBJECT ,  1, true ); // rewriting in interpreter
  def(_aload_1             , "aload_1"             , "b"    , NULL    , T_OBJECT ,  1, false);
  def(_aload_2             , "aload_2"             , "b"    , NULL    , T_OBJECT ,  1, false);
  def(_aload_3             , "aload_3"             , "b"    , NULL    , T_OBJECT ,  1, false);
  def(_iaload              , "iaload"              , "b"    , NULL    , T_INT    , -1, true );
  def(_laload              , "laload"              , "b"    , NULL    , T_LONG   ,  0, true );
  def(_faload              , "faload"              , "b"    , NULL    , T_FLOAT  , -1, true );
  def(_daload              , "daload"              , "b"    , NULL    , T_DOUBLE ,  0, true );
  def(_aaload              , "aaload"              , "b"    , NULL    , T_OBJECT , -1, true );
  def(_baload              , "baload"              , "b"    , NULL    , T_INT    , -1, true );
  def(_caload              , "caload"              , "b"    , NULL    , T_INT    , -1, true );
  def(_saload              , "saload"              , "b"    , NULL    , T_INT    , -1, true );
  def(_istore              , "istore"              , "bi"   , "wbii"  , T_VOID   , -1, false);
  def(_lstore              , "lstore"              , "bi"   , "wbii"  , T_VOID   , -2, false);
  def(_fstore              , "fstore"              , "bi"   , "wbii"  , T_VOID   , -1, false);
  def(_dstore              , "dstore"              , "bi"   , "wbii"  , T_VOID   , -2, false);
  def(_astore              , "astore"              , "bi"   , "wbii"  , T_VOID   , -1, false);
  def(_istore_0            , "istore_0"            , "b"    , NULL    , T_VOID   , -1, false);
  def(_istore_1            , "istore_1"            , "b"    , NULL    , T_VOID   , -1, false);
  def(_istore_2            , "istore_2"            , "b"    , NULL    , T_VOID   , -1, false);
  def(_istore_3            , "istore_3"            , "b"    , NULL    , T_VOID   , -1, false);
  def(_lstore_0            , "lstore_0"            , "b"    , NULL    , T_VOID   , -2, false);
  def(_lstore_1            , "lstore_1"            , "b"    , NULL    , T_VOID   , -2, false);
  def(_lstore_2            , "lstore_2"            , "b"    , NULL    , T_VOID   , -2, false);
  def(_lstore_3            , "lstore_3"            , "b"    , NULL    , T_VOID   , -2, false);
  def(_fstore_0            , "fstore_0"            , "b"    , NULL    , T_VOID   , -1, false);
  def(_fstore_1            , "fstore_1"            , "b"    , NULL    , T_VOID   , -1, false);
  def(_fstore_2            , "fstore_2"            , "b"    , NULL    , T_VOID   , -1, false);
  def(_fstore_3            , "fstore_3"            , "b"    , NULL    , T_VOID   , -1, false);
  def(_dstore_0            , "dstore_0"            , "b"    , NULL    , T_VOID   , -2, false);
  def(_dstore_1            , "dstore_1"            , "b"    , NULL    , T_VOID   , -2, false);
  def(_dstore_2            , "dstore_2"            , "b"    , NULL    , T_VOID   , -2, false);
  def(_dstore_3            , "dstore_3"            , "b"    , NULL    , T_VOID   , -2, false);
  def(_astore_0            , "astore_0"            , "b"    , NULL    , T_VOID   , -1, false);
  def(_astore_1            , "astore_1"            , "b"    , NULL    , T_VOID   , -1, false);
  def(_astore_2            , "astore_2"            , "b"    , NULL    , T_VOID   , -1, false);
  def(_astore_3            , "astore_3"            , "b"    , NULL    , T_VOID   , -1, false);
  def(_iastore             , "iastore"             , "b"    , NULL    , T_VOID   , -3, true );
  def(_lastore             , "lastore"             , "b"    , NULL    , T_VOID   , -4, true );
  def(_fastore             , "fastore"             , "b"    , NULL    , T_VOID   , -3, true );
  def(_dastore             , "dastore"             , "b"    , NULL    , T_VOID   , -4, true );
  def(_aastore             , "aastore"             , "b"    , NULL    , T_VOID   , -3, true );
  def(_bastore             , "bastore"             , "b"    , NULL    , T_VOID   , -3, true );
  def(_castore             , "castore"             , "b"    , NULL    , T_VOID   , -3, true );
  def(_sastore             , "sastore"             , "b"    , NULL    , T_VOID   , -3, true );
  def(_pop                 , "pop"                 , "b"    , NULL    , T_VOID   , -1, false);
  def(_pop2                , "pop2"                , "b"    , NULL    , T_VOID   , -2, false);
  def(_dup                 , "dup"                 , "b"    , NULL    , T_VOID   ,  1, false);
  def(_dup_x1              , "dup_x1"              , "b"    , NULL    , T_VOID   ,  1, false);
  def(_dup_x2              , "dup_x2"              , "b"    , NULL    , T_VOID   ,  1, false);
  def(_dup2                , "dup2"                , "b"    , NULL    , T_VOID   ,  2, false);
  def(_dup2_x1             , "dup2_x1"             , "b"    , NULL    , T_VOID   ,  2, false);
  def(_dup2_x2             , "dup2_x2"             , "b"    , NULL    , T_VOID   ,  2, false);
  def(_swap                , "swap"                , "b"    , NULL    , T_VOID   ,  0, false);
  def(_iadd                , "iadd"                , "b"    , NULL    , T_INT    , -1, false);
  def(_ladd                , "ladd"                , "b"    , NULL    , T_LONG   , -2, false);
  def(_fadd                , "fadd"                , "b"    , NULL    , T_FLOAT  , -1, false);
  def(_dadd                , "dadd"                , "b"    , NULL    , T_DOUBLE , -2, false);
  def(_isub                , "isub"                , "b"    , NULL    , T_INT    , -1, false);
  def(_lsub                , "lsub"                , "b"    , NULL    , T_LONG   , -2, false);
  def(_fsub                , "fsub"                , "b"    , NULL    , T_FLOAT  , -1, false);
  def(_dsub                , "dsub"                , "b"    , NULL    , T_DOUBLE , -2, false);
  def(_imul                , "imul"                , "b"    , NULL    , T_INT    , -1, false);
  def(_lmul                , "lmul"                , "b"    , NULL    , T_LONG   , -2, false);
  def(_fmul                , "fmul"                , "b"    , NULL    , T_FLOAT  , -1, false);
  def(_dmul                , "dmul"                , "b"    , NULL    , T_DOUBLE , -2, false);
  def(_idiv                , "idiv"                , "b"    , NULL    , T_INT    , -1, true );
  def(_ldiv                , "ldiv"                , "b"    , NULL    , T_LONG   , -2, true );
  def(_fdiv                , "fdiv"                , "b"    , NULL    , T_FLOAT  , -1, false);
  def(_ddiv                , "ddiv"                , "b"    , NULL    , T_DOUBLE , -2, false);
  def(_irem                , "irem"                , "b"    , NULL    , T_INT    , -1, true );
  def(_lrem                , "lrem"                , "b"    , NULL    , T_LONG   , -2, true );
  def(_frem                , "frem"                , "b"    , NULL    , T_FLOAT  , -1, false);
  def(_drem                , "drem"                , "b"    , NULL    , T_DOUBLE , -2, false);
  def(_ineg                , "ineg"                , "b"    , NULL    , T_INT    ,  0, false);
  def(_lneg                , "lneg"                , "b"    , NULL    , T_LONG   ,  0, false);
  def(_fneg                , "fneg"                , "b"    , NULL    , T_FLOAT  ,  0, false);
  def(_dneg                , "dneg"                , "b"    , NULL    , T_DOUBLE ,  0, false);
  def(_ishl                , "ishl"                , "b"    , NULL    , T_INT    , -1, false);
  def(_lshl                , "lshl"                , "b"    , NULL    , T_LONG   , -1, false);
  def(_ishr                , "ishr"                , "b"    , NULL    , T_INT    , -1, false);
  def(_lshr                , "lshr"                , "b"    , NULL    , T_LONG   , -1, false);
  def(_iushr               , "iushr"               , "b"    , NULL    , T_INT    , -1, false);
  def(_lushr               , "lushr"               , "b"    , NULL    , T_LONG   , -1, false);
  def(_iand                , "iand"                , "b"    , NULL    , T_INT    , -1, false);
  def(_land                , "land"                , "b"    , NULL    , T_LONG   , -2, false);
  def(_ior                 , "ior"                 , "b"    , NULL    , T_INT    , -1, false);
  def(_lor                 , "lor"                 , "b"    , NULL    , T_LONG   , -2, false);
  def(_ixor                , "ixor"                , "b"    , NULL    , T_INT    , -1, false);
  def(_lxor                , "lxor"                , "b"    , NULL    , T_LONG   , -2, false);
  def(_iinc                , "iinc"                , "bic"  , "wbiicc", T_VOID   ,  0, false);
  def(_i2l                 , "i2l"                 , "b"    , NULL    , T_LONG   ,  1, false);
  def(_i2f                 , "i2f"                 , "b"    , NULL    , T_FLOAT  ,  0, false);
  def(_i2d                 , "i2d"                 , "b"    , NULL    , T_DOUBLE ,  1, false);
  def(_l2i                 , "l2i"                 , "b"    , NULL    , T_INT    , -1, false);
  def(_l2f                 , "l2f"                 , "b"    , NULL    , T_FLOAT  , -1, false);
  def(_l2d                 , "l2d"                 , "b"    , NULL    , T_DOUBLE ,  0, false);
  def(_f2i                 , "f2i"                 , "b"    , NULL    , T_INT    ,  0, false);
  def(_f2l                 , "f2l"                 , "b"    , NULL    , T_LONG   ,  1, false);
  def(_f2d                 , "f2d"                 , "b"    , NULL    , T_DOUBLE ,  1, false);
  def(_d2i                 , "d2i"                 , "b"    , NULL    , T_INT    , -1, false);
  def(_d2l                 , "d2l"                 , "b"    , NULL    , T_LONG   ,  0, false);
  def(_d2f                 , "d2f"                 , "b"    , NULL    , T_FLOAT  , -1, false);
  def(_i2b                 , "i2b"                 , "b"    , NULL    , T_BYTE   ,  0, false);
  def(_i2c                 , "i2c"                 , "b"    , NULL    , T_CHAR   ,  0, false);
  def(_i2s                 , "i2s"                 , "b"    , NULL    , T_SHORT  ,  0, false);
  def(_lcmp                , "lcmp"                , "b"    , NULL    , T_VOID   , -3, false);
  def(_fcmpl               , "fcmpl"               , "b"    , NULL    , T_VOID   , -1, false);
  def(_fcmpg               , "fcmpg"               , "b"    , NULL    , T_VOID   , -1, false);
  def(_dcmpl               , "dcmpl"               , "b"    , NULL    , T_VOID   , -3, false);
  def(_dcmpg               , "dcmpg"               , "b"    , NULL    , T_VOID   , -3, false);
  def(_ifeq                , "ifeq"                , "boo"  , NULL    , T_VOID   , -1, false);
  def(_ifne                , "ifne"                , "boo"  , NULL    , T_VOID   , -1, false);
  def(_iflt                , "iflt"                , "boo"  , NULL    , T_VOID   , -1, false);
  def(_ifge                , "ifge"                , "boo"  , NULL    , T_VOID   , -1, false);
  def(_ifgt                , "ifgt"                , "boo"  , NULL    , T_VOID   , -1, false);
  def(_ifle                , "ifle"                , "boo"  , NULL    , T_VOID   , -1, false);
  def(_if_icmpeq           , "if_icmpeq"           , "boo"  , NULL    , T_VOID   , -2, false);
  def(_if_icmpne           , "if_icmpne"           , "boo"  , NULL    , T_VOID   , -2, false);
  def(_if_icmplt           , "if_icmplt"           , "boo"  , NULL    , T_VOID   , -2, false);
  def(_if_icmpge           , "if_icmpge"           , "boo"  , NULL    , T_VOID   , -2, false);
  def(_if_icmpgt           , "if_icmpgt"           , "boo"  , NULL    , T_VOID   , -2, false);
  def(_if_icmple           , "if_icmple"           , "boo"  , NULL    , T_VOID   , -2, false);
  def(_if_acmpeq           , "if_acmpeq"           , "boo"  , NULL    , T_VOID   , -2, false);
  def(_if_acmpne           , "if_acmpne"           , "boo"  , NULL    , T_VOID   , -2, false);
  def(_goto                , "goto"                , "boo"  , NULL    , T_VOID   ,  0, false);
  def(_jsr                 , "jsr"                 , "boo"  , NULL    , T_INT    ,  0, false);
  def(_ret                 , "ret"                 , "bi"   , "wbii"  , T_VOID   ,  0, false);
  def(_tableswitch         , "tableswitch"         , ""     , NULL    , T_VOID   , -1, false); // may have backward branches
  def(_lookupswitch        , "lookupswitch"        , ""     , NULL    , T_VOID   , -1, false); // rewriting in interpreter
  def(_ireturn             , "ireturn"             , "b"    , NULL    , T_INT    , -1, true);
  def(_lreturn             , "lreturn"             , "b"    , NULL    , T_LONG   , -2, true);
  def(_freturn             , "freturn"             , "b"    , NULL    , T_FLOAT  , -1, true);
  def(_dreturn             , "dreturn"             , "b"    , NULL    , T_DOUBLE , -2, true);
  def(_areturn             , "areturn"             , "b"    , NULL    , T_OBJECT , -1, true);
  def(_return              , "return"              , "b"    , NULL    , T_VOID   ,  0, true);
  def(_getstatic           , "getstatic"           , "bJJ"  , NULL    , T_ILLEGAL,  1, true );
  def(_putstatic           , "putstatic"           , "bJJ"  , NULL    , T_ILLEGAL, -1, true );
  def(_getfield            , "getfield"            , "bJJ"  , NULL    , T_ILLEGAL,  0, true );
  def(_putfield            , "putfield"            , "bJJ"  , NULL    , T_ILLEGAL, -2, true );
  def(_invokevirtual       , "invokevirtual"       , "bJJ"  , NULL    , T_ILLEGAL, -1, true);
  def(_invokespecial       , "invokespecial"       , "bJJ"  , NULL    , T_ILLEGAL, -1, true);
  def(_invokestatic        , "invokestatic"        , "bJJ"  , NULL    , T_ILLEGAL,  0, true);
  def(_invokeinterface     , "invokeinterface"     , "bJJ__", NULL    , T_ILLEGAL, -1, true);
  def(_invokedynamic       , "invokedynamic"       , "bJJJJ", NULL    , T_ILLEGAL,  0, true );
  def(_new                 , "new"                 , "bkk"  , NULL    , T_OBJECT ,  1, true );
  def(_newarray            , "newarray"            , "bc"   , NULL    , T_OBJECT ,  0, true );
  def(_anewarray           , "anewarray"           , "bkk"  , NULL    , T_OBJECT ,  0, true );
  def(_arraylength         , "arraylength"         , "b"    , NULL    , T_VOID   ,  0, true );
  def(_athrow              , "athrow"              , "b"    , NULL    , T_VOID   , -1, true );
  def(_checkcast           , "checkcast"           , "bkk"  , NULL    , T_OBJECT ,  0, true );
  def(_instanceof          , "instanceof"          , "bkk"  , NULL    , T_INT    ,  0, true );
  def(_monitorenter        , "monitorenter"        , "b"    , NULL    , T_VOID   , -1, true );
  def(_monitorexit         , "monitorexit"         , "b"    , NULL    , T_VOID   , -1, true );
  def(_wide                , "wide"                , ""     , NULL    , T_VOID   ,  0, false);
  def(_multianewarray      , "multianewarray"      , "bkkc" , NULL    , T_OBJECT ,  1, true );
  def(_ifnull              , "ifnull"              , "boo"  , NULL    , T_VOID   , -1, false);
  def(_ifnonnull           , "ifnonnull"           , "boo"  , NULL    , T_VOID   , -1, false);
  def(_goto_w              , "goto_w"              , "boooo", NULL    , T_VOID   ,  0, false);
  def(_jsr_w               , "jsr_w"               , "boooo", NULL    , T_INT    ,  0, false);
  def(_breakpoint          , "breakpoint"          , ""     , NULL    , T_VOID   ,  0, true);

  //  JVM bytecodes
  //  bytecode               bytecode name           format   wide f.   result tp  stk traps  std code

  def(_fast_agetfield      , "fast_agetfield"      , "bJJ"  , NULL    , T_OBJECT ,  0, true , _getfield       );
  def(_fast_bgetfield      , "fast_bgetfield"      , "bJJ"  , NULL    , T_INT    ,  0, true , _getfield       );
  def(_fast_cgetfield      , "fast_cgetfield"      , "bJJ"  , NULL    , T_CHAR   ,  0, true , _getfield       );
  def(_fast_dgetfield      , "fast_dgetfield"      , "bJJ"  , NULL    , T_DOUBLE ,  0, true , _getfield       );
  def(_fast_fgetfield      , "fast_fgetfield"      , "bJJ"  , NULL    , T_FLOAT  ,  0, true , _getfield       );
  def(_fast_igetfield      , "fast_igetfield"      , "bJJ"  , NULL    , T_INT    ,  0, true , _getfield       );
  def(_fast_lgetfield      , "fast_lgetfield"      , "bJJ"  , NULL    , T_LONG   ,  0, true , _getfield       );
  def(_fast_sgetfield      , "fast_sgetfield"      , "bJJ"  , NULL    , T_SHORT  ,  0, true , _getfield       );

  def(_fast_aputfield      , "fast_aputfield"      , "bJJ"  , NULL    , T_OBJECT ,  0, true , _putfield       );
  def(_fast_bputfield      , "fast_bputfield"      , "bJJ"  , NULL    , T_INT    ,  0, true , _putfield       );
  def(_fast_zputfield      , "fast_zputfield"      , "bJJ"  , NULL    , T_INT    ,  0, true , _putfield       );
  def(_fast_cputfield      , "fast_cputfield"      , "bJJ"  , NULL    , T_CHAR   ,  0, true , _putfield       );
  def(_fast_dputfield      , "fast_dputfield"      , "bJJ"  , NULL    , T_DOUBLE ,  0, true , _putfield       );
  def(_fast_fputfield      , "fast_fputfield"      , "bJJ"  , NULL    , T_FLOAT  ,  0, true , _putfield       );
  def(_fast_iputfield      , "fast_iputfield"      , "bJJ"  , NULL    , T_INT    ,  0, true , _putfield       );
  def(_fast_lputfield      , "fast_lputfield"      , "bJJ"  , NULL    , T_LONG   ,  0, true , _putfield       );
  def(_fast_sputfield      , "fast_sputfield"      , "bJJ"  , NULL    , T_SHORT  ,  0, true , _putfield       );

  def(_fast_aload_0        , "fast_aload_0"        , "b"    , NULL    , T_OBJECT ,  1, true , _aload_0        );
  def(_fast_iaccess_0      , "fast_iaccess_0"      , "b_JJ" , NULL    , T_INT    ,  1, true , _aload_0        );
  def(_fast_aaccess_0      , "fast_aaccess_0"      , "b_JJ" , NULL    , T_OBJECT ,  1, true , _aload_0        );
  def(_fast_faccess_0      , "fast_faccess_0"      , "b_JJ" , NULL    , T_OBJECT ,  1, true , _aload_0        );

  def(_fast_iload          , "fast_iload"          , "bi"   , NULL    , T_INT    ,  1, false, _iload);
  def(_fast_iload2         , "fast_iload2"         , "bi_i" , NULL    , T_INT    ,  2, false, _iload);
  def(_fast_icaload        , "fast_icaload"        , "bi_"  , NULL    , T_INT    ,  0, false, _iload);

  // Faster method invocation.
  def(_fast_invokevfinal   , "fast_invokevfinal"   , "bJJ"  , NULL    , T_ILLEGAL, -1, true, _invokevirtual   );

  def(_fast_linearswitch   , "fast_linearswitch"   , ""     , NULL    , T_VOID   , -1, false, _lookupswitch   );
  def(_fast_binaryswitch   , "fast_binaryswitch"   , ""     , NULL    , T_VOID   , -1, false, _lookupswitch   );

  def(_return_register_finalizer , "return_register_finalizer" , "b"    , NULL    , T_VOID   ,  0, true, _return);

  def(_invokehandle        , "invokehandle"        , "bJJ"  , NULL    , T_ILLEGAL, -1, true, _invokevirtual   );

  def(_fast_aldc           , "fast_aldc"           , "bj"   , NULL    , T_OBJECT,   1, true,  _ldc   );
  def(_fast_aldc_w         , "fast_aldc_w"         , "bJJ"  , NULL    , T_OBJECT,   1, true,  _ldc_w );

  def(_shouldnotreachhere  , "_shouldnotreachhere" , "b"    , NULL    , T_VOID   ,  0, false);

  // platform specific JVM bytecodes
  pd_initialize();

  // compare can_trap information for each bytecode with the
  // can_trap information for the corresponding base bytecode
  // (if a rewritten bytecode can trap, so must the base bytecode)
  #ifdef ASSERT
    { for (int i = 0; i < number_of_codes; i++) {
        if (is_defined(i)) {
          Code code = cast(i);
          Code java = java_code(code);
          if (can_trap(code) && !can_trap(java))
            fatal(err_msg("%s can trap => %s can trap, too", name(code),
                          name(java)));
        }
      }
    }
  #endif

  // initialization successful
  _is_initialized = true;
}


// 字节码的初始化,初始化jvm的字节码,也是vm层面的指令.
void bytecodes_init() {
  Bytecodes::initialize();
}

// Restore optimization
#ifdef _M_AMD64
#pragma optimize ("", on)
#endif
