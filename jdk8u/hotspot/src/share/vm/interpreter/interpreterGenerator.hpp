/*
 * Copyright (c) 1997, 2011, Oracle and/or its affiliates. All rights reserved.
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

#ifndef SHARE_VM_INTERPRETER_INTERPRETERGENERATOR_HPP
#define SHARE_VM_INTERPRETER_INTERPRETERGENERATOR_HPP

#include "interpreter/cppInterpreter.hpp"
#include "interpreter/cppInterpreterGenerator.hpp"
#include "interpreter/templateInterpreter.hpp"
#include "interpreter/templateInterpreterGenerator.hpp"

// This file contains the platform-independent parts
// of the interpreter generator.


// InterpreterGenerator的定义在同目录下的interpreterGenerator.hpp中，表示一个解释器生成器。
// 其实现跟Interpreter类似，只是一个统一的门面而已，通过宏决定其继承的子类是CppInterpreterGenerator或者TemplateInterpreterGenerator
// 默认是继承TemplateInterpreterGenerator
// 跟平台相关的部分通过图中的宏定义引入，interpreterGenerator_x86.hpp中定义的都是私有方法
class InterpreterGenerator: public CC_INTERP_ONLY(CppInterpreterGenerator)
                                   NOT_CC_INTERP(TemplateInterpreterGenerator) {

public:

InterpreterGenerator(StubQueue* _code);

#ifdef TARGET_ARCH_x86
# include "interpreterGenerator_x86.hpp"
#endif
#ifdef TARGET_ARCH_aarch64
# include "interpreterGenerator_aarch64.hpp"
#endif
#ifdef TARGET_ARCH_sparc
# include "interpreterGenerator_sparc.hpp"
#endif
#ifdef TARGET_ARCH_zero
# include "interpreterGenerator_zero.hpp"
#endif
#ifdef TARGET_ARCH_arm
# include "interpreterGenerator_arm.hpp"
#endif
#ifdef TARGET_ARCH_ppc
# include "interpreterGenerator_ppc.hpp"
#endif


};

#endif // SHARE_VM_INTERPRETER_INTERPRETERGENERATOR_HPP
