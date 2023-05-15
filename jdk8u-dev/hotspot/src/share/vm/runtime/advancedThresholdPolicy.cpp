/*
 * Copyright (c) 2010, 2019, Oracle and/or its affiliates. All rights reserved.
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
#include "runtime/advancedThresholdPolicy.hpp"
#include "runtime/simpleThresholdPolicy.inline.hpp"

#ifdef TIERED
// Print an event.
void AdvancedThresholdPolicy::print_specific(EventType type, methodHandle mh, methodHandle imh,
                                             int bci, CompLevel level) {
  tty->print(" rate=");
  if (mh->prev_time() == 0) tty->print("n/a");
  else tty->print("%f", mh->rate());

  tty->print(" k=%.2lf,%.2lf", threshold_scale(CompLevel_full_profile, Tier3LoadFeedback),
                               threshold_scale(CompLevel_full_optimization, Tier4LoadFeedback));

}

// AdvancedThresholdPolicy::initialize就是AdvancedThresholdPolicy的初始化方法
void AdvancedThresholdPolicy::initialize() {
  // Turn on ergonomic compiler count selection
    //CICompilerCountPerCPU默认值为false，表示是否每个CPU对应1个后台编译线程，CICompilerCount表示后台编译线程的数量，默认值3
    //当这两个都是默认值时，将CICompilerCountPerCPU置为true
  if (FLAG_IS_DEFAULT(CICompilerCountPerCPU) && FLAG_IS_DEFAULT(CICompilerCount)) {
    FLAG_SET_DEFAULT(CICompilerCountPerCPU, true);
  }
  int count = CICompilerCount;
  if (CICompilerCountPerCPU) {
    // Simple log n seems to grow too slowly for tiered, try something faster: log n * log log n
      //考虑机器的CPU数量，计算总的后台编译线程数
    int log_cpu = log2_int(os::active_processor_count());
    int loglog_cpu = log2_int(MAX2(log_cpu, 1));
    count = MAX2(log_cpu * loglog_cpu, 1) * 3 / 2;
  }
    //计算C1,C2编译线程的数量
  set_c1_count(MAX2(count / 3, 1));
  set_c2_count(MAX2(count - c1_count(), 1));
    //重置CICompilerCount
  FLAG_SET_ERGO(intx, CICompilerCount, c1_count() + c2_count());

  // Some inlining tuning
#if defined(X86) || defined(AARCH64)
    //InlineSmallCode表示只有当方法代码大小小于该值时才会使用内联编译的方式，x86下默认是1000
    //这里是将其调整为2000
  if (FLAG_IS_DEFAULT(InlineSmallCode)) {
    FLAG_SET_DEFAULT(InlineSmallCode, 2000);
  }
#endif

#ifdef SPARC
  if (FLAG_IS_DEFAULT(InlineSmallCode)) {
    FLAG_SET_DEFAULT(InlineSmallCode, 2500);
  }
#endif

  set_increase_threshold_at_ratio();
  set_start_time(os::javaTimeMillis());
}

// update_rate() is called from select_task() while holding a compile queue lock.
void AdvancedThresholdPolicy::update_rate(jlong t, Method* m) {
  // Skip update if counters are absent.
  // Can't allocate them since we are holding compile queue lock.
    // 如果没有MethodCounters
  if (m->method_counters() == NULL)  return;

    //运行时间长，调用次数多的方法
  if (is_old(m)) {
    // We don't remove old methods from the queue,
    // so we can just zero the rate.
      //rate是MethodCounters的一个属性，用来记录每毫秒调用次数的增量，编译线程选择编译任务时优先选择rate最大的方法
    m->set_rate(0);
    return;
  }

  // We don't update the rate if we've just came out of a safepoint.
  // delta_s is the time since last safepoint in milliseconds.
    //delta_s表示自上一次safepoint的时间
  jlong delta_s = t - SafepointSynchronize::end_of_last_safepoint();
    //delta_t表示自上一次measurement即计算rate的时间
  jlong delta_t = t - (m->prev_time() != 0 ? m->prev_time() : start_time()); // milliseconds since the last measurement
  // How many events were there since the last time?
  int event_count = m->invocation_count() + m->backedge_count();
    //获取调用次数的增量
  int delta_e = event_count - m->prev_event_count();

  // We should be running for at least 1ms.
    //更新时间间隔大于最低间隔，默认1ms，即rate 1ms内更新一次
  if (delta_s >= TieredRateUpdateMinTime) {
    // And we must've taken the previous point at least 1ms before.
    if (delta_t >= TieredRateUpdateMinTime && delta_e > 0) {
        //计算rate，更新prev_time和prev_event_count
      m->set_prev_time(t);
      m->set_prev_event_count(event_count);
      m->set_rate((float)delta_e / (float)delta_t); // Rate is events per millisecond
    } else {
        //如果没有新增调用，则rate置为0，TieredRateUpdateMaxTime是最大时间间隔，默认25ms
      if (delta_t > TieredRateUpdateMaxTime && delta_e == 0) {
        // If nothing happened for 25ms, zero the rate. Don't modify prev values.
        m->set_rate(0);
      }
    }
  }
}

// Check if this method has been stale from a given number of milliseconds.
// See select_task().
// 在一段时间timeout内，调用次数没有变化，即该方法不再被调用，被认为是stale
bool AdvancedThresholdPolicy::is_stale(jlong t, jlong timeout, Method* m) {
  jlong delta_s = t - SafepointSynchronize::end_of_last_safepoint();
  jlong delta_t = t - m->prev_time();
  if (delta_t > timeout && delta_s > timeout) {
    int event_count = m->invocation_count() + m->backedge_count();
      //计算调用次数的增量
    int delta_e = event_count - m->prev_event_count();
    // Return true if there were no events.
    return delta_e == 0;
  }
  return false;
}

// We don't remove old methods from the compile queue even if they have
// very low activity. See select_task().
bool AdvancedThresholdPolicy::is_old(Method* method) {
  return method->invocation_count() > 50000 || method->backedge_count() > 500000;
}

double AdvancedThresholdPolicy::weight(Method* method) {
  return (double)(method->rate() + 1) *
    (method->invocation_count() + 1) * (method->backedge_count() + 1);
}

// Apply heuristics and return true if x should be compiled before y
bool AdvancedThresholdPolicy::compare_methods(Method* x, Method* y) {
  if (x->highest_comp_level() > y->highest_comp_level()) {
    // recompilation after deopt
    return true;
  } else
    if (x->highest_comp_level() == y->highest_comp_level()) {
      if (weight(x) > weight(y)) {
        return true;
      }
    }
  return false;
}

// Is method profiled enough?
//判断某个方法是否已经full profile
bool AdvancedThresholdPolicy::is_method_profiled(Method* method) {
  MethodData* mdo = method->method_data();
  if (mdo != NULL) {
    int i = mdo->invocation_count_delta();
    int b = mdo->backedge_count_delta();
    return call_predicate_helper<CompLevel_full_profile>(i, b, 1);
  }
  return false;
}

// Called with the queue locked and with at least one element
// select_task方法用于从编译任务队列中挑选一个rate最大的任务执行，在遍历任务队列的过程中如果CompileTask对应的方法在一段时间内不再被调用则将其添加到_first_stale链表中，并对对应的方法标记成已从编译队列移除。
CompileTask* AdvancedThresholdPolicy::select_task(CompileQueue* compile_queue) {
  CompileTask *max_task = NULL;
  Method* max_method = NULL;
    //获取当前系统时间
  jlong t = os::javaTimeMillis();
  // Iterate through the queue and find a method with a maximum rate.
    //遍历整个队列找到rate即单位时间内调用次数最大的一个CompileTask
  for (CompileTask* task = compile_queue->first(); task != NULL;) {
    CompileTask* next_task = task->next();
    Method* method = task->method();
      //重新计算rate
    update_rate(t, method);
    if (max_task == NULL) {
        //初始化
      max_task = task;
      max_method = method;
    } else {
      // If a method has been stale for some time, remove it from the queue.
        //如果该方法在一段时间内不再被调用，且不是old方法
      if (is_stale(t, TieredCompileTaskTimeout, method) && !is_old(method)) {
        if (PrintTieredEvents) {
          print_event(REMOVE_FROM_QUEUE, method, method, task->osr_bci(), (CompLevel)task->comp_level());
        }
          //将其从编译队列移除，并标记成stale
        compile_queue->remove_and_mark_stale(task);
          //method的_access_flags打标，该方法已从编译队列移除
        method->clear_queued_for_compilation();
          //遍历下一个方法
        task = next_task;
        continue;
      }

      // Select a method with a higher rate
        //选择一个rate更大的method
      if (compare_methods(method, max_method)) {
        max_task = task;
        max_method = method;
      }
    }
    task = next_task;
  }
    //如果已经收集了足够的profile信息，将编译级别置为有限的profile
  if (max_task->comp_level() == CompLevel_full_profile && TieredStopAtLevel > CompLevel_full_profile
      && is_method_profiled(max_method)) {

    if (CompileBroker::compilation_is_complete(max_method, max_task->osr_bci(), CompLevel_limited_profile)) {
      if (PrintTieredEvents) {
        print_event(REMOVE_FROM_QUEUE, max_method, max_method, max_task->osr_bci(), (CompLevel)max_task->comp_level());
      }
      compile_queue->remove_and_mark_stale(max_task);
      max_method->clear_queued_for_compilation();
      return NULL;
    }

    max_task->set_comp_level(CompLevel_limited_profile);
    if (PrintTieredEvents) {
      print_event(UPDATE_IN_QUEUE, max_method, max_method, max_task->osr_bci(), (CompLevel)max_task->comp_level());
    }
  }

  return max_task;
}

double AdvancedThresholdPolicy::threshold_scale(CompLevel level, int feedback_k) {
    //获取队列长度
  double queue_size = CompileBroker::queue_size(level);
    //获取编译线程的数量
  int comp_count = compiler_count(level);
  double k = queue_size / (feedback_k * comp_count) + 1;

  // Increase C1 compile threshold when the code cache is filled more
  // than specified by IncreaseFirstTierCompileThresholdAt percentage.
  // The main intention is to keep enough free space for C2 compiled code
  // to achieve peak performance if the code cache is under stress.
  if ((TieredStopAtLevel == CompLevel_full_optimization) && (level != CompLevel_full_optimization))  {
      //获取CodeCache的剩余可用空间，如果不足，则增加阈值，从而为C2编译保留足够的空间
    double current_reverse_free_ratio = CodeCache::reverse_free_ratio();
    if (current_reverse_free_ratio > _increase_threshold_at_ratio) {
      k *= exp(current_reverse_free_ratio - _increase_threshold_at_ratio);
    }
  }
  return k;
}

// Call and loop predicates determine whether a transition to a higher
// compilation level should be performed (pointers to predicate functions
// are passed to common()).
// Tier?LoadFeedback is basically a coefficient that determines of
// how many methods per compiler thread can be in the queue before
// the threshold values double.
//计算是否超过调整编译级别的阈值
bool AdvancedThresholdPolicy::loop_predicate(int i, int b, CompLevel cur_level) {
  switch(cur_level) {
  case CompLevel_none:
  case CompLevel_limited_profile: {
    double k = threshold_scale(CompLevel_full_profile, Tier3LoadFeedback);
    return loop_predicate_helper<CompLevel_none>(i, b, k);
  }
  case CompLevel_full_profile: {
    double k = threshold_scale(CompLevel_full_optimization, Tier4LoadFeedback);
    return loop_predicate_helper<CompLevel_full_profile>(i, b, k);
  }
  default:
    return true;
  }
}

//计算是否超过调整编译级别的阈值
bool AdvancedThresholdPolicy::call_predicate(int i, int b, CompLevel cur_level) {
  switch(cur_level) {
  case CompLevel_none:
  case CompLevel_limited_profile: {
    double k = threshold_scale(CompLevel_full_profile, Tier3LoadFeedback);
    return call_predicate_helper<CompLevel_none>(i, b, k);
  }
  case CompLevel_full_profile: {
    double k = threshold_scale(CompLevel_full_optimization, Tier4LoadFeedback);
    return call_predicate_helper<CompLevel_full_profile>(i, b, k);
  }
  default:
    return true;
  }
}

// If a method is old enough and is still in the interpreter we would want to
// start profiling without waiting for the compiled method to arrive.
// We also take the load on compilers into the account.
//如果一个方法运行很久了依然是解释执行，则需要开启profile，而不等待其触发编译了
bool AdvancedThresholdPolicy::should_create_mdo(Method* method, CompLevel cur_level) {
  if (cur_level == CompLevel_none &&
      CompileBroker::queue_size(CompLevel_full_optimization) <=
      Tier3DelayOn * compiler_count(CompLevel_full_optimization)) {
    int i = method->invocation_count();
    int b = method->backedge_count();
    double k = Tier0ProfilingStartPercentage / 100.0;
    return call_predicate_helper<CompLevel_none>(i, b, k) || loop_predicate_helper<CompLevel_none>(i, b, k);
  }
  return false;
}

// Inlining control: if we're compiling a profiled method with C1 and the callee
// is known to have OSRed in a C2 version, don't inline it.
bool AdvancedThresholdPolicy::should_not_inline(ciEnv* env, ciMethod* callee) {
  CompLevel comp_level = (CompLevel)env->comp_level();
  if (comp_level == CompLevel_full_profile ||
      comp_level == CompLevel_limited_profile) {
    return callee->highest_osr_comp_level() == CompLevel_full_optimization;
  }
  return false;
}

// Create MDO if necessary.
void AdvancedThresholdPolicy::create_mdo(methodHandle mh, JavaThread* THREAD) {
  if (mh->is_native() || mh->is_abstract() || mh->is_accessor()) return;
  if (mh->method_data() == NULL) {
      //初始化Method的MethodData
    Method::build_interpreter_method_data(mh, CHECK_AND_CLEAR);
  }
}


/*
 *  AdvancedThresholdPolicy支持5个级别的编译:
 *
 * Method states:
 *   0 - interpreter (CompLevel_none)
 *   1 - pure C1 (CompLevel_simple)
 *   2 - C1 with invocation and backedge counting (CompLevel_limited_profile)
 *   3 - C1 with full profiling (CompLevel_full_profile)
 *   4 - C2 (CompLevel_full_optimization)
 *
 * 其中0,2,3三个级别下都会周期性的通知 AdvancedThresholdPolicy某个方法的方法调用计数即invocation counters and循环调用计数即backedge counters，不同级别下通知的频率不同。这些通知用来决定如何调整编译级别。
 *
 *     某个方法刚开始执行时是通过解释器解释执行的，即level 0，然后AdvancedThresholdPolicy会综合如下两个因素和调用计数将编译级别调整为2或者3：
 *
 * C2编译任务队列的长度决定了下一个编译级别。据观察，level 2级别下的编译比level 3级别下的编译快30%，因此我们应该在只有已经收集了充分的profile信息后才采用level 3编译，从而尽可能缩短level 3级别下编译的耗时。
 *  当C2的编译任务队列太长的时候，如果选择level 3则编译会被卡住直到C2将之前的编译任务处理完成，这时如果选择Level 2编译则很快完成编译。当C2的编译压力逐步减小，就可以重新在level 3下编译并且开始收集profile信息。
 * 根据C1编译任务队列的长度用来动态的调整阈值，在编译器过载时，会将已经编译完成但是不在使用的方法从编译队列中移除。
 *      当level 3下profile信息收集完成后就会转换成level 4了，可根据C2编译任务队列的长度来动态调整转换的阈值。当经过C1编译完成后，基于方法的代码块的数量，循环的数量等信息可以判断一个方法是否是琐碎（trivial）的，
 *      这类方法在C2编译下会产生和C1编译一样的代码，因此这时会用level 1的编译代替level 4的编译。
 *
 *      当C1和C2的编译任务队列的长度足够短，也可在level 0下开启profile信息收集。编译队列通过优先级队列实现，每个添加到编译任务队列的方法都会周期的计算其在单位时间内增加的调用次数，每次从编译队列中获取任务时，
 *      都选择单位时间内调用次数最大的一个。基于此，我们也可以将那些编译完成后不再使用，调用次数不再增加的方法从编译任务队列中移除。跟编译级别转换相关的命令行参数及其转换逻辑可以参考AdvancedThresholdPolicy的注释。
 *
 *       常见的各级别转换路径如下：
 *
 * 0 -> 3 -> 4，最常见的转换路径，需要注意这种情况下profile可以从level 0开始，level3结束。
 * 0 -> 2 -> 3 -> 4，当C2的编译压力较大会发生这种情形，本来应该从0直接转换成3，为了减少编译耗时就从0转换成2，等C2的编译压力降低再转换成3
 * 0 -> (3->2) -> 4，这种情形下已经把方法加入到3的编译队列中了，但是C1队列太长了导致在0的时候就开启了profile，然后就调整了编译策略按level 2来编译，即时还是3的队列中，这样编译更快
 * 0 -> 3 -> 1 or 0 -> 2 -> 1，当一个方法被C1编译后，被标记成trivial的，因为这类方法C2编译的代码和C1编译的代码是一样的，所以不使用4，改成1
 * 0 -> 4，一个方法C1编译失败且一直在收集profile信息，就从0切换到4
 *
 *
 * Common state transition patterns:
 * a. 0 -> 3 -> 4.
 *    The most common path. But note that even in this straightforward case
 *    profiling can start at level 0 and finish at level 3.
 *
 * b. 0 -> 2 -> 3 -> 4.
 *    This case occures when the load on C2 is deemed too high. So, instead of transitioning
 *    into state 3 directly and over-profiling while a method is in the C2 queue we transition to
 *    level 2 and wait until the load on C2 decreases. This path is disabled for OSRs.
 *
 * c. 0 -> (3->2) -> 4.
 *    In this case we enqueue a method for compilation at level 3, but the C1 queue is long enough
 *    to enable the profiling to fully occur at level 0. In this case we change the compilation level
 *    of the method to 2 while the request is still in-queue, because it'll allow it to run much faster
 *    without full profiling while c2 is compiling.
 *
 * d. 0 -> 3 -> 1 or 0 -> 2 -> 1.
 *    After a method was once compiled with C1 it can be identified as trivial and be compiled to
 *    level 1. These transition can also occur if a method can't be compiled with C2 but can with C1.
 *
 * e. 0 -> 4.
 *    This can happen if a method fails C1 compilation (it will still be profiled in the interpreter)
 *    or because of a deopt that didn't require reprofiling (compilation won't happen in this case because
 *    the compiled version already exists).
 *
 * Note that since state 0 can be reached from any other state via deoptimization different loops
 * are possible.
 *
 */

// Common transition function. Given a predicate determines if a method should transition to another level.
CompLevel AdvancedThresholdPolicy::common(Predicate p, Method* method, CompLevel cur_level, bool disable_feedback) {
  CompLevel next_level = cur_level;
  int i = method->invocation_count();
  int b = method->backedge_count();
    //trivial方法切换成CompLevel_simple
  if (is_trivial(method)) {
    next_level = CompLevel_simple;
  } else {
      //非trivial方法
    switch(cur_level) {
        //根据当前级别的不同处理不同
    case CompLevel_none:
      // If we were at full profile level, would we switch to full opt?
        //如果处于CompLevel_full_profile并且超过阈值则切换成CompLevel_full_optimization
      if (common(p, method, CompLevel_full_profile, disable_feedback) == CompLevel_full_optimization) {
        next_level = CompLevel_full_optimization;
      } else if ((this->*p)(i, b, cur_level)) {
        // C1-generated fully profiled code is about 30% slower than the limited profile
        // code that has only invocation and backedge counters. The observation is that
        // if C2 queue is large enough we can spend too much time in the fully profiled code
        // while waiting for C2 to pick the method from the queue. To alleviate this problem
        // we introduce a feedback on the C2 queue size. If the C2 queue is sufficiently long
        // we choose to compile a limited profiled version and then recompile with full profiling
        // when the load on C2 goes down.
          // 如果C2的队列过长
        if (!disable_feedback && CompileBroker::queue_size(CompLevel_full_optimization) >
                                 Tier3DelayOn * compiler_count(CompLevel_full_optimization)) {
          next_level = CompLevel_limited_profile;
        } else {
          next_level = CompLevel_full_profile;
        }
      }
      break;
    case CompLevel_limited_profile:
        //如果已经full profile了
      if (is_method_profiled(method)) {
        // Special case: we got here because this method was fully profiled in the interpreter.
        next_level = CompLevel_full_optimization;
      } else {
        MethodData* mdo = method->method_data();
        if (mdo != NULL) {
            //该方法能够开启profile
          if (mdo->would_profile()) {
              //如果C2队列的长度比较小且超过阈值
            if (disable_feedback || (CompileBroker::queue_size(CompLevel_full_optimization) <=
                                     Tier3DelayOff * compiler_count(CompLevel_full_optimization) &&
                                     (this->*p)(i, b, cur_level))) {
              next_level = CompLevel_full_profile;
            }
          } else {
            next_level = CompLevel_full_optimization;
          }
        }
      }
      break;
    case CompLevel_full_profile:
      {
        MethodData* mdo = method->method_data();
        if (mdo != NULL) {
          if (mdo->would_profile()) {
            int mdo_i = mdo->invocation_count_delta();
            int mdo_b = mdo->backedge_count_delta();
              //判断MethodData中profile超过阈值了
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
CompLevel AdvancedThresholdPolicy::call_event(Method* method, CompLevel cur_level) {
    //获取osr循环代码的编译level
  CompLevel osr_level = MIN2((CompLevel) method->highest_osr_comp_level(),
                             common(&AdvancedThresholdPolicy::loop_predicate, method, cur_level, true));
    //获取普通方法的编译level
  CompLevel next_level = common(&AdvancedThresholdPolicy::call_predicate, method, cur_level);

  // If OSR method level is greater than the regular method level, the levels should be
  // equalized by raising the regular method level in order to avoid OSRs during each
  // invocation of the method.
    //取osr_level和cur_level的最大值，从而避免osr循环的编译级别高于外层方法的编译级别
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
CompLevel AdvancedThresholdPolicy::loop_event(Method* method, CompLevel cur_level) {
  CompLevel next_level = common(&AdvancedThresholdPolicy::loop_predicate, method, cur_level, true);
  if (cur_level == CompLevel_none) {
    // If there is a live OSR method that means that we deopted to the interpreter
    // for the transition.
      // 该方法已经编译过了
    CompLevel osr_level = MIN2((CompLevel)method->highest_osr_comp_level(), next_level);
    if (osr_level > CompLevel_none) {
      return osr_level;
    }
  }
  return next_level;
}

// Update the rate and submit compile
void AdvancedThresholdPolicy::submit_compile(methodHandle mh, int bci, CompLevel level, JavaThread* thread) {
  int hot_count = (bci == InvocationEntryBci) ? mh->invocation_count() : mh->backedge_count();
  update_rate(os::javaTimeMillis(), mh());
  CompileBroker::compile_method(mh, bci, level, mh, hot_count, "tiered", thread);
}

// Handle the invocation event.
void AdvancedThresholdPolicy::method_invocation_event(methodHandle mh, methodHandle imh,
                                                      CompLevel level, nmethod* nm, JavaThread* thread) {
    //如果需要开启profile，则初始化方法的mdo
  if (should_create_mdo(mh(), level)) {
    create_mdo(mh, thread);
  }
    //如果开启方法编译，且该方法不再编译队列中
  if (is_compilation_enabled() && !CompileBroker::compilation_is_in_queue(mh)) {
    CompLevel next_level = call_event(mh(), level);
    if (next_level != level) {
        //执行方法编译
      compile(mh, InvocationEntryBci, next_level, thread);
    }
  }
}

// Handle the back branch event. Notice that we can compile the method
// with a regular entry from here.
void AdvancedThresholdPolicy::method_back_branch_event(methodHandle mh, methodHandle imh,
                                                       int bci, CompLevel level, nmethod* nm, JavaThread* thread) {
    // 如果需要开启profile，则初始化方法的mdo
  if (should_create_mdo(mh(), level)) {
    create_mdo(mh, thread);
  }
  // Check if MDO should be created for the inlined method
  if (should_create_mdo(imh(), level)) {
    create_mdo(imh, thread);
  }

  if (is_compilation_enabled()) {
      //获取下一个编译级别
    CompLevel next_osr_level = loop_event(imh(), level);
      //获取曾经有的最高编译级别
    CompLevel max_osr_level = (CompLevel)imh->highest_osr_comp_level();
    // At the very least compile the OSR version
      //如果该方法不再编译队列中且编译级别变了
    if (!CompileBroker::compilation_is_in_queue(imh) && (next_osr_level != level)) {
      compile(imh, bci, next_osr_level, thread);
    }

    // Use loop event as an opportunity to also check if there's been
    // enough calls.
    CompLevel cur_level, next_level;
      //如果有内联方法
    if (mh() != imh()) { // If there is an enclosing method
      guarantee(nm != NULL, "Should have nmethod here");
        //外层方法的当前编译级别和下一个编译级别
      cur_level = comp_level(mh());
      next_level = call_event(mh(), cur_level);
        //如果已经是最高编译级别了
      if (max_osr_level == CompLevel_full_optimization) {
        // The inlinee OSRed to full opt, we need to modify the enclosing method to avoid deopts
          //如果内联的osr方法已经是最高优化级别，为了避免逆向优化需要去除nmethod
        bool make_not_entrant = false;
        if (nm->is_osr_method()) {
          // This is an osr method, just make it not entrant and recompile later if needed
          make_not_entrant = true;
        } else {
          if (next_level != CompLevel_full_optimization) {
            // next_level is not full opt, so we need to recompile the
            // enclosing method without the inlinee
            cur_level = CompLevel_none;
            make_not_entrant = true;
          }
        }
        if (make_not_entrant) {
          if (PrintTieredEvents) {
            int osr_bci = nm->is_osr_method() ? nm->osr_entry_bci() : InvocationEntryBci;
            print_event(MAKE_NOT_ENTRANT, mh(), mh(), osr_bci, level);
          }
            //去掉nmethod
          nm->make_not_entrant();
        }
      }
      if (!CompileBroker::compilation_is_in_queue(mh)) {
        // Fix up next_level if necessary to avoid deopts
          //编译外层方法
        if (next_level == CompLevel_limited_profile && max_osr_level == CompLevel_full_profile) {
          next_level = CompLevel_full_profile;
        }
        if (cur_level != next_level) {
          compile(mh, InvocationEntryBci, next_level, thread);
        }
      }
    } else {
        //imh()和mh()一样则只编译imh即可
      cur_level = comp_level(imh());
      next_level = call_event(imh(), cur_level);
      if (!CompileBroker::compilation_is_in_queue(imh) && (next_level != cur_level)) {
        compile(imh, InvocationEntryBci, next_level, thread);
      }
    }
  }
}

#endif // TIERED
