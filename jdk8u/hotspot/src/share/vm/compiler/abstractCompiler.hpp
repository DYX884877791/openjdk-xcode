/*
 * Copyright (c) 1999, 2013, Oracle and/or its affiliates. All rights reserved.
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

#ifndef SHARE_VM_COMPILER_ABSTRACTCOMPILER_HPP
#define SHARE_VM_COMPILER_ABSTRACTCOMPILER_HPP

#include "ci/compilerInterface.hpp"

// AbstractCompiler是编译器的基类，定义了获取编译器相关属性的公共方法。AbstractCompiler的定义位于hospot/src/share/vm/compiler/abstractCompiler.hpp中，
// 其定义的属性只有两个，_num_compiler_threads和_compiler_state，前者表示编译器编译线程的个数，后者表示编译器的状态，是一个枚举值
// AbstractCompiler定义的方法都是读写其属性的方法，如set_num_compiler_threads，num_compiler_threads等，负责编译方法的只有一个方法compile_method

// AbstractCompiler的子类有Compiler、C2Compiler，SharkCompiler
// 其中Compiler就是C1编译器即client编译器，其定义位于hospot src/share/vm/c1/c1_Compiler.hpp中，c1目录下所有类都是该编译器的实现；
// C2Compiler就是C2编译器即server编译器，其定义位于hospot src/share/vm/opto/c2Compiler.hpp中，opto目录所有类都是该编译器的相关实现；
// SharkCompiler就是新的基于LLVM架构的编译器，其定义位于hospot src/share/vm/shark/sharkCompiler.hpp中，shark目录下相关类都是该编译器的相关实现，
// 因为SharkCompiler目前是基于老旧的LLVM 3.x开发的，很长时间都未更新且存在明显性能问题，所以OpenJDK计划将其完全移除。
class AbstractCompiler : public CHeapObj<mtCompiler> {
 private:
  volatile int _num_compiler_threads;

 protected:
  volatile int _compiler_state;
  // Used for tracking global state of compiler runtime initialization
  enum { uninitialized, initializing, initialized, failed, shut_down };

  // This method returns true for the first compiler thread that reaches that methods.
  // This thread will initialize the compiler runtime.
  bool should_perform_init();

 public:
  AbstractCompiler() : _compiler_state(uninitialized), _num_compiler_threads(0) {}

  // This function determines the compiler thread that will perform the
  // shutdown of the corresponding compiler runtime.
  bool should_perform_shutdown();

  // Name of this compiler
  virtual const char* name() = 0;

  // Missing feature tests
  virtual bool supports_native()                 { return true; }
  virtual bool supports_osr   ()                 { return true; }
  virtual bool can_compile_method(methodHandle method)  { return true; }
#if defined(TIERED) || ( !defined(COMPILER1) && !defined(COMPILER2) && !defined(SHARK))
  virtual bool is_c1   ()                        { return false; }
  virtual bool is_c2   ()                        { return false; }
  virtual bool is_shark()                        { return false; }
#else
#ifdef COMPILER1
  bool is_c1   ()                                { return true; }
  bool is_c2   ()                                { return false; }
  bool is_shark()                                { return false; }
#endif // COMPILER1
#ifdef COMPILER2
  bool is_c1   ()                                { return false; }
  bool is_c2   ()                                { return true; }
  bool is_shark()                                { return false; }
#endif // COMPILER2
#ifdef SHARK
  bool is_c1   ()                                { return false; }
  bool is_c2   ()                                { return false; }
  bool is_shark()                                { return true; }
#endif // SHARK
#endif // TIERED

  // Customization
  virtual void initialize () = 0;

  void set_num_compiler_threads(int num) { _num_compiler_threads = num;  }
  int num_compiler_threads()             { return _num_compiler_threads; }

  // Get/set state of compiler objects
  bool is_initialized()           { return _compiler_state == initialized; }
  bool is_failed     ()           { return _compiler_state == failed;}
  void set_state     (int state);
  void set_shut_down ()           { set_state(shut_down); }
  // Compilation entry point for methods
  virtual void compile_method(ciEnv* env, ciMethod* target, int entry_bci) {
    ShouldNotReachHere();
  }


  // Print compilation timers and statistics
  virtual void print_timers() {
    ShouldNotReachHere();
  }
};

#endif // SHARE_VM_COMPILER_ABSTRACTCOMPILER_HPP
