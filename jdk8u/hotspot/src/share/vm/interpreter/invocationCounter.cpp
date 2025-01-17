/*
 * Copyright (c) 1997, 2013, Oracle and/or its affiliates. All rights reserved.
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
#include "interpreter/invocationCounter.hpp"
#include "runtime/frame.hpp"
#include "runtime/handles.inline.hpp"


// Implementation of InvocationCounter

// 调用该函数的位置为MethodCounters::MethodCounters()函数
void InvocationCounter::init() {
    //所有的位都初始化成0
  _counter = 0;  // reset all the bits, including the sticky carry
  reset();
}

void InvocationCounter::reset() {
  // Only reset the state and don't make the method look like it's never
  // been executed
  set_state(wait_for_compile);
}

void InvocationCounter::set_carry() {
    //执行set_carry_flag后，_counter会变得很大
  set_carry_flag();
  // The carry bit now indicates that this counter had achieved a very
  // large value.  Now reduce the value, so that the method can be
  // executed many more times before re-entering the VM.
  int old_count = count();
    //new_count的值一般情况下取后者
  int new_count = MIN2(old_count, (int) (CompileThreshold / 2));
  // prevent from going to zero, to distinguish from never-executed methods
  if (new_count == 0)  new_count = 1;
    //重置调用计数
  if (old_count != new_count)  set(state(), new_count);
}

void InvocationCounter::set_state(State state) {
    //校验state的合法性
  assert(0 <= state && state < number_of_states, "illegal state");
    //获取该state下的调用计数，初始为0
  int init = _init[state];
  // prevent from going to zero, to distinguish from never-executed methods
    //初始状态下count()返回0，init也是0
    //当运行一段时间发生状态切换后，count()返回值大于0，如果此时init==0说明是第一次执行此状态下的调用，将init初始化为1
  if (init == 0 && count() > 0)  init = 1;
  int carry = (_counter & carry_mask);    // the carry bit is sticky
    // 初始化counter
  _counter = (init << number_of_noncount_bits) | carry | state;
}


void InvocationCounter::print() {
  tty->print_cr("invocation count: up = %d, limit = %d, carry = %s, state = %s",
                                   count(), limit(),
                                   carry() ? "true" : "false",
                                   state_as_string(state()));
}

void InvocationCounter::print_short() {
  tty->print(" [%d%s;%s]", count(), carry()?"+carry":"", state_as_short_string(state()));
}

// Initialization

int                       InvocationCounter::_init  [InvocationCounter::number_of_states];
InvocationCounter::Action InvocationCounter::_action[InvocationCounter::number_of_states];
int                       InvocationCounter::InterpreterInvocationLimit;
int                       InvocationCounter::InterpreterBackwardBranchLimit;
int                       InvocationCounter::InterpreterProfileLimit;


const char* InvocationCounter::state_as_string(State state) {
  switch (state) {
    case wait_for_nothing            : return "wait_for_nothing";
    case wait_for_compile            : return "wait_for_compile";
  }
  ShouldNotReachHere();
  return NULL;
}

const char* InvocationCounter::state_as_short_string(State state) {
  switch (state) {
    case wait_for_nothing            : return "not comp.";
    case wait_for_compile            : return "compileable";
  }
  ShouldNotReachHere();
  return NULL;
}


static address do_nothing(methodHandle method, TRAPS) {
  // dummy action for inactive invocation counters
    //获取并校验目标方法的MethodCounters
  MethodCounters* mcs = method->method_counters();
  assert(mcs != NULL, "");
    //重置调用计数为CompileThreshold的一半
  mcs->invocation_counter()->set_carry();
    //显示的将状态置为wait_for_nothing
  mcs->invocation_counter()->set_state(InvocationCounter::wait_for_nothing);
  return NULL;
}


static address do_decay(methodHandle method, TRAPS) {
  // decay invocation counters so compilation gets delayed
    //获取并校验目标方法的MethodCounters
  MethodCounters* mcs = method->method_counters();
  assert(mcs != NULL, "");
  mcs->invocation_counter()->decay();
  return NULL;
}


/**
 * InvocationCounter::def方法只有InvocationCounter::reinitialize方法调用。reinitialize方法的两个调用方，
 * 其传递的参数都是全局属性DelayCompilationDuringStartup，该属性默认为true，表示在启动的时候延迟编译从而快速启动
 */
void InvocationCounter::def(State state, int init, Action action) {
  assert(0 <= state && state < number_of_states, "illegal state");
  assert(0 <= init  && init  < count_limit, "initial value out of range");
  _init  [state] = init;
  _action[state] = action;
}

address dummy_invocation_counter_overflow(methodHandle m, TRAPS) {
  ShouldNotReachHere();
  return NULL;
}

// 主要用于初始化InvocationCounter的静态属性
void InvocationCounter::reinitialize(bool delay_overflow) {
  // define states
    //确保number_of_states小于等于4
  guarantee((int)number_of_states <= (int)state_limit, "adjust number_of_state_bits");
    //设置两种状态下的触发动作
  def(wait_for_nothing, 0, do_nothing);
    //如果延迟处理，delay_overflow肯定是true，所以不会走到dummy_invocation_counter_overflow，该方法是空实现
  if (delay_overflow) {
    def(wait_for_compile, 0, do_decay);
  } else {
    def(wait_for_compile, 0, dummy_invocation_counter_overflow);
  }

    //计算InterpreterInvocationLimit等阈值
  InterpreterInvocationLimit = CompileThreshold << number_of_noncount_bits;
  InterpreterProfileLimit = ((CompileThreshold * InterpreterProfilePercentage) / 100)<< number_of_noncount_bits;

  // When methodData is collected, the backward branch limit is compared against a
  // methodData counter, rather than an InvocationCounter.  In the former case, we
  // don't need the shift by number_of_noncount_bits, but we do need to adjust
  // the factor by which we scale the threshold.
  if (ProfileInterpreter) {
    InterpreterBackwardBranchLimit = (CompileThreshold * (OnStackReplacePercentage - InterpreterProfilePercentage)) / 100;
  } else {
    InterpreterBackwardBranchLimit = ((CompileThreshold * OnStackReplacePercentage) / 100) << number_of_noncount_bits;
  }

    //校验计算结果的合法性
  assert(0 <= InterpreterBackwardBranchLimit,
         "OSR threshold should be non-negative");
  assert(0 <= InterpreterProfileLimit &&
         InterpreterProfileLimit <= InterpreterInvocationLimit,
         "profile threshold should be less than the compilation threshold "
         "and non-negative");
}

void invocationCounter_init() {
  InvocationCounter::reinitialize(DelayCompilationDuringStartup);
}
