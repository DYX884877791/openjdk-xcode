/*
 * Copyright (c) 2002, 2013, Oracle and/or its affiliates. All rights reserved.
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
#include "asm/assembler.hpp"
#include "asm/register.hpp"
#include "register_x86.hpp"
#ifdef TARGET_ARCH_x86
# include "interp_masm_x86.hpp"
#endif

/**
 * 在cpu/x86/vm/register_definitions_x86.cpp文件中定义的寄存器如下:
 *
 * const Register  noreg = ((Register)noreg_RegisterEnumValue)
 * const Register  rax =   ((Register)rax_RegisterEnumValue)
 * const Register  rcx =   ((Register)rcx_RegisterEnumValue)
 * const Register  rdx =   ((Register)rdx_RegisterEnumValue)
 * const Register  rbx =   ((Register)rbx_RegisterEnumValue)
 * const Register  rsp =   ((Register)rsp_RegisterEnumValue)
 * const Register  rbp =   ((Register)rbp_RegisterEnumValue)
 * const Register  rsi =   ((Register)rsi_RegisterEnumValue)
 * const Register  rdi =   ((Register)rdi_RegisterEnumValue)
 * const Register  r8 =  ((Register)r8_RegisterEnumValue)
 * const Register  r9 =  ((Register)r9_RegisterEnumValue)
 * const Register  r10 = ((Register)r10_RegisterEnumValue)
 * const Register  r11 = ((Register)r11_RegisterEnumValue)
 * const Register  r12 = ((Register)r12_RegisterEnumValue)
 * const Register  r13 = ((Register)r13_RegisterEnumValue)
 * const Register  r14 = ((Register)r14_RegisterEnumValue)
 * const Register  r15 = ((Register)r15_RegisterEnumValue)
 * 当我们需要使用通用寄存器时，通过rax、rcx等变量引用就可以了。
 */
REGISTER_DEFINITION(Register, noreg);
REGISTER_DEFINITION(Register, rax);
REGISTER_DEFINITION(Register, rcx);
REGISTER_DEFINITION(Register, rdx);
REGISTER_DEFINITION(Register, rbx);
REGISTER_DEFINITION(Register, rsp);
REGISTER_DEFINITION(Register, rbp);
REGISTER_DEFINITION(Register, rsi);
REGISTER_DEFINITION(Register, rdi);
#ifdef AMD64
REGISTER_DEFINITION(Register, r8);
REGISTER_DEFINITION(Register, r9);
REGISTER_DEFINITION(Register, r10);
REGISTER_DEFINITION(Register, r11);
REGISTER_DEFINITION(Register, r12);
REGISTER_DEFINITION(Register, r13);
REGISTER_DEFINITION(Register, r14);
REGISTER_DEFINITION(Register, r15);
#endif // AMD64

REGISTER_DEFINITION(XMMRegister, xnoreg);
REGISTER_DEFINITION(XMMRegister, xmm0 );
REGISTER_DEFINITION(XMMRegister, xmm1 );
REGISTER_DEFINITION(XMMRegister, xmm2 );
REGISTER_DEFINITION(XMMRegister, xmm3 );
REGISTER_DEFINITION(XMMRegister, xmm4 );
REGISTER_DEFINITION(XMMRegister, xmm5 );
REGISTER_DEFINITION(XMMRegister, xmm6 );
REGISTER_DEFINITION(XMMRegister, xmm7 );
#ifdef AMD64
REGISTER_DEFINITION(XMMRegister, xmm8);
REGISTER_DEFINITION(XMMRegister, xmm9);
REGISTER_DEFINITION(XMMRegister, xmm10);
REGISTER_DEFINITION(XMMRegister, xmm11);
REGISTER_DEFINITION(XMMRegister, xmm12);
REGISTER_DEFINITION(XMMRegister, xmm13);
REGISTER_DEFINITION(XMMRegister, xmm14);
REGISTER_DEFINITION(XMMRegister, xmm15);

REGISTER_DEFINITION(Register, c_rarg0);
REGISTER_DEFINITION(Register, c_rarg1);
REGISTER_DEFINITION(Register, c_rarg2);
REGISTER_DEFINITION(Register, c_rarg3);

REGISTER_DEFINITION(XMMRegister, c_farg0);
REGISTER_DEFINITION(XMMRegister, c_farg1);
REGISTER_DEFINITION(XMMRegister, c_farg2);
REGISTER_DEFINITION(XMMRegister, c_farg3);

// Non windows OS's have a few more argument registers
#ifndef _WIN64
REGISTER_DEFINITION(Register, c_rarg4);
REGISTER_DEFINITION(Register, c_rarg5);

REGISTER_DEFINITION(XMMRegister, c_farg4);
REGISTER_DEFINITION(XMMRegister, c_farg5);
REGISTER_DEFINITION(XMMRegister, c_farg6);
REGISTER_DEFINITION(XMMRegister, c_farg7);
#endif /* _WIN64 */

REGISTER_DEFINITION(Register, j_rarg0);
REGISTER_DEFINITION(Register, j_rarg1);
REGISTER_DEFINITION(Register, j_rarg2);
REGISTER_DEFINITION(Register, j_rarg3);
REGISTER_DEFINITION(Register, j_rarg4);
REGISTER_DEFINITION(Register, j_rarg5);

REGISTER_DEFINITION(XMMRegister, j_farg0);
REGISTER_DEFINITION(XMMRegister, j_farg1);
REGISTER_DEFINITION(XMMRegister, j_farg2);
REGISTER_DEFINITION(XMMRegister, j_farg3);
REGISTER_DEFINITION(XMMRegister, j_farg4);
REGISTER_DEFINITION(XMMRegister, j_farg5);
REGISTER_DEFINITION(XMMRegister, j_farg6);
REGISTER_DEFINITION(XMMRegister, j_farg7);

REGISTER_DEFINITION(Register, rscratch1);
REGISTER_DEFINITION(Register, rscratch2);

REGISTER_DEFINITION(Register, r12_heapbase);
REGISTER_DEFINITION(Register, r15_thread);
#endif // AMD64

/**
 * cpu/x86/vm/register_definitions_x86.cpp文件中定义的寄存器变量如下：
 * const MMXRegister  mnoreg = ((MMXRegister)mnoreg_MMXRegisterEnumValue)
 * const MMXRegister  mmx0 =   ((MMXRegister)mmx0_MMXRegisterEnumValue)
 * const MMXRegister  mmx1 =   ((MMXRegister)mmx1_MMXRegisterEnumValue)
 * const MMXRegister  mmx2 =   ((MMXRegister)mmx2_MMXRegisterEnumValue)
 * const MMXRegister  mmx3 =   ((MMXRegister)mmx3_MMXRegisterEnumValue)
 * const MMXRegister  mmx4 =   ((MMXRegister)mmx4_MMXRegisterEnumValue)
 * const MMXRegister  mmx5 =   ((MMXRegister)mmx5_MMXRegisterEnumValue)
 * const MMXRegister  mmx6 =   ((MMXRegister)mmx6_MMXRegisterEnumValue)
 * const MMXRegister  mmx7 =   ((MMXRegister)mmx7_MMXRegisterEnumValue)
 * 当我们需要使用MMX寄存器时，通过mmx0、mmx1等变量引用就可以了。
 */
REGISTER_DEFINITION(MMXRegister, mnoreg );
REGISTER_DEFINITION(MMXRegister, mmx0 );
REGISTER_DEFINITION(MMXRegister, mmx1 );
REGISTER_DEFINITION(MMXRegister, mmx2 );
REGISTER_DEFINITION(MMXRegister, mmx3 );
REGISTER_DEFINITION(MMXRegister, mmx4 );
REGISTER_DEFINITION(MMXRegister, mmx5 );
REGISTER_DEFINITION(MMXRegister, mmx6 );
REGISTER_DEFINITION(MMXRegister, mmx7 );

// JSR 292
REGISTER_DEFINITION(Register, rbp_mh_SP_save);
