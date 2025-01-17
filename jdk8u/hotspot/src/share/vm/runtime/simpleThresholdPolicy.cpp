/*
 * Copyright (c) 2010, 2013, Oracle and/or its affiliates. All rights reserved.
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
#include "compiler/compileBroker.hpp"
#include "memory/resourceArea.hpp"
#include "runtime/arguments.hpp"
#include "runtime/simpleThresholdPolicy.hpp"
#include "runtime/simpleThresholdPolicy.inline.hpp"
#include "code/scopeDesc.hpp"


void SimpleThresholdPolicy::print_counters(const char* prefix, methodHandle mh) {
  int invocation_count = mh->invocation_count();
  int backedge_count = mh->backedge_count();
  MethodData* mdh = mh->method_data();
  int mdo_invocations = 0, mdo_backedges = 0;
  int mdo_invocations_start = 0, mdo_backedges_start = 0;
  if (mdh != NULL) {
    mdo_invocations = mdh->invocation_count();
    mdo_backedges = mdh->backedge_count();
    mdo_invocations_start = mdh->invocation_count_start();
    mdo_backedges_start = mdh->backedge_count_start();
  }
  tty->print(" %stotal=%d,%d %smdo=%d(%d),%d(%d)", prefix,
      invocation_count, backedge_count, prefix,
      mdo_invocations, mdo_invocations_start,
      mdo_backedges, mdo_backedges_start);
  tty->print(" %smax levels=%d,%d", prefix,
      mh->highest_comp_level(), mh->highest_osr_comp_level());
}

// Print an event.
void SimpleThresholdPolicy::print_event(EventType type, methodHandle mh, methodHandle imh,
                                        int bci, CompLevel level) {
  bool inlinee_event = mh() != imh();

  ttyLocker tty_lock;
  tty->print("%lf: [", os::elapsedTime());

  switch(type) {
  case CALL:
    tty->print("call");
    break;
  case LOOP:
    tty->print("loop");
    break;
  case COMPILE:
    tty->print("compile");
    break;
  case REMOVE_FROM_QUEUE:
    tty->print("remove-from-queue");
    break;
  case UPDATE_IN_QUEUE:
    tty->print("update-in-queue");
    break;
  case REPROFILE:
    tty->print("reprofile");
    break;
  case MAKE_NOT_ENTRANT:
    tty->print("make-not-entrant");
    break;
  default:
    tty->print("unknown");
  }

  tty->print(" level=%d ", level);

  ResourceMark rm;
  char *method_name = mh->name_and_sig_as_C_string();
  tty->print("[%s", method_name);
  if (inlinee_event) {
    char *inlinee_name = imh->name_and_sig_as_C_string();
    tty->print(" [%s]] ", inlinee_name);
  }
  else tty->print("] ");
  tty->print("@%d queues=%d,%d", bci, CompileBroker::queue_size(CompLevel_full_profile),
                                      CompileBroker::queue_size(CompLevel_full_optimization));

  print_specific(type, mh, imh, bci, level);

  if (type != COMPILE) {
    print_counters("", mh);
    if (inlinee_event) {
      print_counters("inlinee ", imh);
    }
    tty->print(" compilable=");
    bool need_comma = false;
    if (!mh->is_not_compilable(CompLevel_full_profile)) {
      tty->print("c1");
      need_comma = true;
    }
    if (!mh->is_not_osr_compilable(CompLevel_full_profile)) {
      if (need_comma) tty->print(",");
      tty->print("c1-osr");
      need_comma = true;
    }
    if (!mh->is_not_compilable(CompLevel_full_optimization)) {
      if (need_comma) tty->print(",");
      tty->print("c2");
      need_comma = true;
    }
    if (!mh->is_not_osr_compilable(CompLevel_full_optimization)) {
      if (need_comma) tty->print(",");
      tty->print("c2-osr");
    }
    tty->print(" status=");
    if (mh->queued_for_compilation()) {
      tty->print("in-queue");
    } else tty->print("idle");
  }
  tty->print_cr("]");
}

void SimpleThresholdPolicy::initialize() {
  if (FLAG_IS_DEFAULT(CICompilerCount)) {
    FLAG_SET_DEFAULT(CICompilerCount, 3);
  }
  int count = CICompilerCount;
  if (CICompilerCountPerCPU) {
    count = MAX2(log2_int(os::active_processor_count()), 1) * 3 / 2;
  }
  set_c1_count(MAX2(count / 3, 1));
  set_c2_count(MAX2(count - c1_count(), 1));
  FLAG_SET_ERGO(intx, CICompilerCount, c1_count() + c2_count());
}

void SimpleThresholdPolicy::set_carry_if_necessary(InvocationCounter *counter) {
  if (!counter->carry() && counter->count() > InvocationCounter::count_limit / 2) {
      //carry_flag表示计数是否超过一半
    counter->set_carry_flag();
  }
}

// Set carry flags on the counters if necessary
void SimpleThresholdPolicy::handle_counter_overflow(Method* method) {
  MethodCounters *mcs = method->method_counters();
  if (mcs != NULL) {
    set_carry_if_necessary(mcs->invocation_counter());
    set_carry_if_necessary(mcs->backedge_counter());
  }
  MethodData* mdo = method->method_data();
  if (mdo != NULL) {
    set_carry_if_necessary(mdo->invocation_counter());
    set_carry_if_necessary(mdo->backedge_counter());
  }
}

// Called with the queue locked and with at least one element
CompileTask* SimpleThresholdPolicy::select_task(CompileQueue* compile_queue) {
  return compile_queue->first();
}

void SimpleThresholdPolicy::reprofile(ScopeDesc* trap_scope, bool is_osr) {
  for (ScopeDesc* sd = trap_scope;; sd = sd->sender()) {
    if (PrintTieredEvents) {
      methodHandle mh(sd->method());
      print_event(REPROFILE, mh, mh, InvocationEntryBci, CompLevel_none);
    }
    MethodData* mdo = sd->method()->method_data();
    if (mdo != NULL) {
      mdo->reset_start_counters();
    }
    if (sd->is_top()) break;
  }
}

// event方法用于触发热点方法的编译，如果是osr方法则返回包含编译代码的nmethod实例，如果是非ors方法，则返回NULL，
// AdvancedThresholdPolicy没有覆写父类SimpleThresholdPolicy的event方法的实现，
// method_invocation_event方法用于触发普通方法调用下热点代码的编译，method_back_branch_event方法用于处理循环调用下热点代码即osr方法的编译，
// 这两个方法是SimpleThresholdPolicy::event方法的底层实现，AdvancedThresholdPolicy改写了SimpleThresholdPolicy中对应方法的实现
nmethod* SimpleThresholdPolicy::event(methodHandle method, methodHandle inlinee,
                                      int branch_bci, int bci, CompLevel comp_level, nmethod* nm, JavaThread* thread) {
    //如果目标线程是只使用解释模式执行且编译级别就是解释执行，则返回NULL
  if (comp_level == CompLevel_none &&
      JvmtiExport::can_post_interpreter_events() &&
      thread->is_interp_only_mode()) {
    return NULL;
  }
    //CompileTheWorld表示将启动路径所有类都所有方法都编译完成，
    //ReplayCompiles表示基于ReplayDataFile允许重新编译，这两个都是测试版本的选项，非生产版本的选项
  if (CompileTheWorld || ReplayCompiles) {
    // Don't trigger other compiles in testing mode
    return NULL;
  }

  handle_counter_overflow(method());
    //如果两个不等，热点代码编译的method和inlinee是同一个
  if (method() != inlinee()) {
    handle_counter_overflow(inlinee());
  }

    //打印日志
  if (PrintTieredEvents) {
    print_event(bci == InvocationEntryBci ? CALL : LOOP, method, inlinee, bci, comp_level);
  }

    //如果是非栈上替换方法
  if (bci == InvocationEntryBci) {
      //提交方法编译任务
    method_invocation_event(method, inlinee, comp_level, nm, thread);
  } else {
    // method == inlinee if the event originated in the main method
      //如果是栈上替换方法，触发方法编译
    method_back_branch_event(method, inlinee, bci, comp_level, nm, thread);
    // Check if event led to a higher level OSR compilation
      //查找不低于目标编译级别的nmethod实例
    nmethod* osr_nm = inlinee->lookup_osr_nmethod_for(bci, comp_level, false);
    if (osr_nm != NULL && osr_nm->comp_level() > comp_level) {
      // Perform OSR with new nmethod
      return osr_nm;
    }
  }
  return NULL;
}

// Check if the method can be compiled, change level if necessary
void SimpleThresholdPolicy::compile(methodHandle mh, int bci, CompLevel level, JavaThread* thread) {
    //校验编译级别有效性
  assert(level <= TieredStopAtLevel, "Invalid compilation level");
  if (level == CompLevel_none) {
    return;
  }
  // Check if the method can be compiled. If it cannot be compiled with C1, continue profiling
  // in the interpreter and then compile with C2 (the transition function will request that,
  // see common() ). If the method cannot be compiled with C2 but still can with C1, compile it with
  // pure C1.
    //如果不能通过C1编译则继续收集解释执行的性能信息，然后通过C2编译，如果不能通过C2编译，则只通过C1编译
  if (!can_be_compiled(mh, level)) {
    if (level == CompLevel_full_optimization && can_be_compiled(mh, CompLevel_simple)) {
        compile(mh, bci, CompLevel_simple, thread);
    }
    return;
  }
    //如果是栈上替换，但是无法编译则返回
  if (bci != InvocationEntryBci && mh->is_not_osr_compilable(level)) {
    return;
  }
    //如果该方法不在编译队列中
  if (!CompileBroker::compilation_is_in_queue(mh)) {
    if (PrintTieredEvents) {
        //打印日志
      print_event(COMPILE, mh, mh, bci, level);
    }
      //提交编译任务
    submit_compile(mh, bci, level, thread);
  }
}

// Tell the broker to compile the method
//  submit_compile方法是SimpleThresholdPolicy::compile方法中用来最终提交编译任务的，AdvancedThresholdPolicy改写了父类SimpleThresholdPolicy的实现
void SimpleThresholdPolicy::submit_compile(methodHandle mh, int bci, CompLevel level, JavaThread* thread) {
  int hot_count = (bci == InvocationEntryBci) ? mh->invocation_count() : mh->backedge_count();
  CompileBroker::compile_method(mh, bci, level, mh, hot_count, "tiered", thread);
}

// Call and loop predicates determine whether a transition to a higher
// compilation level should be performed (pointers to predicate functions
// are passed to common() transition function).
bool SimpleThresholdPolicy::loop_predicate(int i, int b, CompLevel cur_level) {
  switch(cur_level) {
  case CompLevel_none:
  case CompLevel_limited_profile: {
    return loop_predicate_helper<CompLevel_none>(i, b, 1.0);
  }
  case CompLevel_full_profile: {
    return loop_predicate_helper<CompLevel_full_profile>(i, b, 1.0);
  }
  default:
    return true;
  }
}

bool SimpleThresholdPolicy::call_predicate(int i, int b, CompLevel cur_level) {
  switch(cur_level) {
  case CompLevel_none:
  case CompLevel_limited_profile: {
    return call_predicate_helper<CompLevel_none>(i, b, 1.0);
  }
  case CompLevel_full_profile: {
    return call_predicate_helper<CompLevel_full_profile>(i, b, 1.0);
  }
  default:
    return true;
  }
}

// Determine is a method is mature.
bool SimpleThresholdPolicy::is_mature(Method* method) {
  if (is_trivial(method)) return true;
  MethodData* mdo = method->method_data();
  if (mdo != NULL) {
    int i = mdo->invocation_count();
    int b = mdo->backedge_count();
    double k = ProfileMaturityPercentage / 100.0;
    return call_predicate_helper<CompLevel_full_profile>(i, b, k) ||
           loop_predicate_helper<CompLevel_full_profile>(i, b, k);
  }
  return false;
}

// Common transition function. Given a predicate determines if a method should transition to another level.
CompLevel SimpleThresholdPolicy::common(Predicate p, Method* method, CompLevel cur_level) {
  CompLevel next_level = cur_level;
  int i = method->invocation_count();
  int b = method->backedge_count();

  if (is_trivial(method)) {
    next_level = CompLevel_simple;
  } else {
    switch(cur_level) {
    case CompLevel_none:
      // If we were at full profile level, would we switch to full opt?
      if (common(p, method, CompLevel_full_profile) == CompLevel_full_optimization) {
        next_level = CompLevel_full_optimization;
      } else if ((this->*p)(i, b, cur_level)) {
        next_level = CompLevel_full_profile;
      }
      break;
    case CompLevel_limited_profile:
    case CompLevel_full_profile:
      {
        MethodData* mdo = method->method_data();
        if (mdo != NULL) {
          if (mdo->would_profile()) {
            int mdo_i = mdo->invocation_count_delta();
            int mdo_b = mdo->backedge_count_delta();
            if ((this->*p)(mdo_i, mdo_b, cur_level)) {
              next_level = CompLevel_full_optimization;
            }
          } else {
            next_level = CompLevel_full_optimization;
          }
        }
      }
      break;
    }
  }
  return MIN2(next_level, (CompLevel)TieredStopAtLevel);
}

// Determine if a method should be compiled with a normal entry point at a different level.
//  call_event方法用于计算方法调用超过阈值时触发的编译的编译级别，loop_event方法用于计算循环调用超过阈值时触发的编译的编译级别，分层编译下如何在多个编译级别之前转换的逻辑都封装在这两个方法中，\
//  AdvancedThresholdPolicy覆写了这两个方法的实现。
CompLevel SimpleThresholdPolicy::call_event(Method* method,  CompLevel cur_level) {
  CompLevel osr_level = MIN2((CompLevel) method->highest_osr_comp_level(),
                             common(&SimpleThresholdPolicy::loop_predicate, method, cur_level));
  CompLevel next_level = common(&SimpleThresholdPolicy::call_predicate, method, cur_level);

  // If OSR method level is greater than the regular method level, the levels should be
  // equalized by raising the regular method level in order to avoid OSRs during each
  // invocation of the method.
  if (osr_level == CompLevel_full_optimization && cur_level == CompLevel_full_profile) {
    MethodData* mdo = method->method_data();
    guarantee(mdo != NULL, "MDO should not be NULL");
    if (mdo->invocation_count() >= 1) {
      next_level = CompLevel_full_optimization;
    }
  } else {
    next_level = MAX2(osr_level, next_level);
  }

  return next_level;
}

// Determine if we should do an OSR compilation of a given method.
CompLevel SimpleThresholdPolicy::loop_event(Method* method, CompLevel cur_level) {
  CompLevel next_level = common(&SimpleThresholdPolicy::loop_predicate, method, cur_level);
  if (cur_level == CompLevel_none) {
    // If there is a live OSR method that means that we deopted to the interpreter
    // for the transition.
    CompLevel osr_level = MIN2((CompLevel)method->highest_osr_comp_level(), next_level);
    if (osr_level > CompLevel_none) {
      return osr_level;
    }
  }
  return next_level;
}


// Handle the invocation event.
void SimpleThresholdPolicy::method_invocation_event(methodHandle mh, methodHandle imh,
                                              CompLevel level, nmethod* nm, JavaThread* thread) {
    //如果允许编译，且该方法不在编译队列中
  if (is_compilation_enabled() && !CompileBroker::compilation_is_in_queue(mh)) {
      //计算编译级别
    CompLevel next_level = call_event(mh(), level);
    if (next_level != level) {
        //编译方法
      compile(mh, InvocationEntryBci, next_level, thread);
    }
  }
}

// Handle the back branch event. Notice that we can compile the method
// with a regular entry from here.
void SimpleThresholdPolicy::method_back_branch_event(methodHandle mh, methodHandle imh,
                                                     int bci, CompLevel level, nmethod* nm, JavaThread* thread) {
  // If the method is already compiling, quickly bail out.
  if (is_compilation_enabled() && !CompileBroker::compilation_is_in_queue(mh)) {
    // Use loop event as an opportunity to also check there's been
    // enough calls.
      //当前编译级别
    CompLevel cur_level = comp_level(mh());
      //方法调用触发的热点编译的编译级别
    CompLevel next_level = call_event(mh(), cur_level);
      //循环调用触发的热点编译的编译级别
    CompLevel next_osr_level = loop_event(mh(), level);

      //如果next_osr_level已经是最高编译级别CompLevel_full_optimization，则取next_level和cur_level的最大值
    next_level = MAX2(next_level,
                      next_osr_level < CompLevel_full_optimization ? next_osr_level : cur_level);
    bool is_compiling = false;
      //即优先编译循环代码，编译至最高级别后才考虑编译循环代码所属的本地方法
    if (next_level != cur_level) {
        //编译目标方法
      compile(mh, InvocationEntryBci, next_level, thread);
      is_compiling = true;
    }

    // Do the OSR version
      //is_compiling为false，即不需要编译目标方式时，编译循环代码
    if (!is_compiling && next_osr_level != level) {
      compile(mh, bci, next_osr_level, thread);
    }
  }
}
