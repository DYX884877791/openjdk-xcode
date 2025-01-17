/*
 * Copyright (c) 1999, 2010, Oracle and/or its affiliates. All rights reserved.
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

#ifndef OS_CPU_LINUX_X86_VM_BYTES_LINUX_X86_INLINE_HPP
#define OS_CPU_LINUX_X86_VM_BYTES_LINUX_X86_INLINE_HPP

#include <byteswap.h>

// Efficient swapping of data bytes from Java byte
// ordering to native byte ordering and vice versa.
// 针对基于Linux内核的ubuntu的x86架构下64位版本代码的实现。其中调用的bswap_<x>系列函数是gcc提供的几个内建函数。
//  由于HotSpot需要跨平台兼容，所以会增加一些针对各平台的特定实现
// 其中的AMD64表示x86架构下的64位指令集，所以x86-64位机器会选择AMD64位下的实现。
// 如果是非AMD64位的系统，使用gcc内联汇编来实现相关的功能，其将x 的值读入某个寄存器,然后在指令中使用相应寄存器，并将该值移动到%ax中，
// 然后通过xchg 交换%eax中的高低位。然后将最终的结果送入某个寄存器，最后将该结果送到ret中。　
inline u2   Bytes::swap_u2(u2 x) {
#ifdef AMD64
  return bswap_16(x);
#else
  u2 ret;
  __asm__ __volatile__ (
    "movw %0, %%ax;"
    "xchg %%al, %%ah;"
    "movw %%ax, %0"
    :"=r" (ret)      // output : register 0 => ret
    :"0"  (x)        // input  : x => register 0
    :"ax", "0"       // clobbered registers
  );
  return ret;
#endif // AMD64
}

inline u4   Bytes::swap_u4(u4 x) {
#ifdef AMD64
  return bswap_32(x);
#else
  u4 ret;
  __asm__ __volatile__ (
    "bswap %0"
    :"=r" (ret)      // output : register 0 => ret
    :"0"  (x)        // input  : x => register 0
    :"0"             // clobbered register
  );
  return ret;
#endif // AMD64
}

#ifdef AMD64
inline u8 Bytes::swap_u8(u8 x) {
#ifdef SPARC_WORKS
  // workaround for SunStudio12 CR6615391
  __asm__ __volatile__ (
    "bswapq %0"
    :"=r" (x)        // output : register 0 => x
    :"0"  (x)        // input  : x => register 0
    :"0"             // clobbered register
  );
  return x;
#else
  return bswap_64(x);
#endif
}
#else
// Helper function for swap_u8
inline u8   Bytes::swap_u8_base(u4 x, u4 y) {
  return (((u8)swap_u4(x))<<32) | swap_u4(y);
}

inline u8 Bytes::swap_u8(u8 x) {
  return swap_u8_base(*(u4*)&x, *(((u4*)&x)+1));
}
#endif // !AMD64

#endif // OS_CPU_LINUX_X86_VM_BYTES_LINUX_X86_INLINE_HPP
