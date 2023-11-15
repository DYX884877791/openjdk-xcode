/*
 * Copyright (c) 2001, 2015, Oracle and/or its affiliates. All rights reserved.
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
#include "classfile/systemDictionary.hpp"
#include "gc_implementation/concurrentMarkSweep/concurrentMarkSweepGeneration.inline.hpp"
#include "gc_implementation/concurrentMarkSweep/concurrentMarkSweepThread.hpp"
#include "memory/genCollectedHeap.hpp"
#include "oops/instanceRefKlass.hpp"
#include "oops/oop.inline.hpp"
#include "runtime/init.hpp"
#include "runtime/interfaceSupport.hpp"
#include "runtime/java.hpp"
#include "runtime/javaCalls.hpp"
#include "runtime/mutexLocker.hpp"
#include "runtime/os.hpp"
#include "runtime/vmThread.hpp"

// ======= Concurrent Mark Sweep Thread ========

// The CMS thread is created when Concurrent Mark Sweep is used in the
// older of two generations in a generational memory system.

ConcurrentMarkSweepThread*
     ConcurrentMarkSweepThread::_cmst     = NULL;
CMSCollector* ConcurrentMarkSweepThread::_collector = NULL;
bool ConcurrentMarkSweepThread::_should_terminate = false;
int  ConcurrentMarkSweepThread::_CMS_flag         = CMS_nil;

volatile jint ConcurrentMarkSweepThread::_pending_yields      = 0;
volatile jint ConcurrentMarkSweepThread::_pending_decrements  = 0;

volatile jint ConcurrentMarkSweepThread::_icms_disabled   = 0;
volatile bool ConcurrentMarkSweepThread::_should_run     = false;
// When icms is enabled, the icms thread is stopped until explicitly
// started.
volatile bool ConcurrentMarkSweepThread::_should_stop    = true;

SurrogateLockerThread*
     ConcurrentMarkSweepThread::_slt = NULL;
SurrogateLockerThread::SLT_msg_type
     ConcurrentMarkSweepThread::_sltBuffer = SurrogateLockerThread::empty;
Monitor*
     ConcurrentMarkSweepThread::_sltMonitor = NULL;

// 构造方法执行时会创建一个与之关联的本地线程并启动线程的执行
ConcurrentMarkSweepThread::ConcurrentMarkSweepThread(CMSCollector* collector)
  : ConcurrentGCThread() {
  assert(UseConcMarkSweepGC,  "UseConcMarkSweepGC should be set");
  assert(_cmst == NULL, "CMS thread already created");
  _cmst = this;
  assert(_collector == NULL, "Collector already set");
  _collector = collector;

    //设置线程名
  set_name("Concurrent Mark-Sweep GC Thread");

    //创建关联的线程
  if (os::create_thread(this, os::cgc_thread)) {
    // An old comment here said: "Priority should be just less
    // than that of VMThread".  Since the VMThread runs at
    // NearMaxPriority, the old comment was inaccurate, but
    // changing the default priority to NearMaxPriority-1
    // could change current behavior, so the default of
    // NearMaxPriority stays in place.
    //
    // Note that there's a possibility of the VMThread
    // starving if UseCriticalCMSThreadPriority is on.
    // That won't happen on Solaris for various reasons,
    // but may well happen on non-Solaris platforms.
    int native_prio;
      //UseCriticalCMSThreadPriority表示使用一个特殊的优先级，默认为false
    if (UseCriticalCMSThreadPriority) {
      native_prio = os::java_to_os_priority[CriticalPriority];
    } else {
      native_prio = os::java_to_os_priority[NearMaxPriority];
    }
      //设置线程优先级
    os::set_native_priority(this, native_prio);

      //DisableStartThread默认为false
    if (!DisableStartThread) {
        //启动线程，调用其run方法
      os::start_thread(this);
    }
  }
  _sltMonitor = SLT_lock;
    //CMSIncrementalMode表示是否开启增量收集模式，适用于单核CPU场景，默认为false
  assert(!CMSIncrementalMode || icms_is_enabled(), "Error");
}

// run方法就是ConcurrentMarkSweepThread的执行逻辑，会不断循环等待，如果需要GC了则调用CMSCollector::collect_in_background方法执行GC，然后继续下一次循环，直到_should_terminate属性为true
void ConcurrentMarkSweepThread::run() {
  assert(this == cmst(), "just checking");

  initialize_in_thread();
  // From this time Thread::current() should be working.
  assert(this == Thread::current(), "just checking");
    //BindCMSThreadToCPU表示是否将CMSThread绑定到指定的CPU核上执行，默认为false
    //CPUForCMSThread表示绑定的CPU核，默认是0，即第一个CPU核
  if (BindCMSThreadToCPU && !os::bind_to_processor(CPUForCMSThread)) {
    warning("Couldn't bind CMS thread to processor " UINTX_FORMAT, CPUForCMSThread);
  }
  // Wait until Universe::is_fully_initialized()
  {
    CMSLoopCountWarn loopX("CMS::run", "waiting for "
                           "Universe::is_fully_initialized()", 2);
      //获取锁
    MutexLockerEx x(CGC_lock, true);
    set_CMS_flag(CMS_cms_wants_token);
    // Wait until Universe is initialized and all initialization is completed.
      //不断循环等待，一次200ms，直到Universe完全初始化完毕
    while (!is_init_completed() && !Universe::is_fully_initialized() &&
           !_should_terminate) {
      CGC_lock->wait(true, 200);
        //增加循环次数，达到阈值了会打印warn日志
      loopX.tick();
    }
    // Wait until the surrogate locker thread that will do
    // pending list locking on our behalf has been created.
    // We cannot start the SLT thread ourselves since we need
    // to be a JavaThread to do so.
      //不断循环等待SurrogateLockerThread初始化完成，由Threads::create_vm方法负责初始化
    CMSLoopCountWarn loopY("CMS::run", "waiting for SLT installation", 2);
    while (_slt == NULL && !_should_terminate) {
      CGC_lock->wait(true, 200);
      loopY.tick();
    }
    clear_CMS_flag(CMS_cms_wants_token);
  }

  while (!_should_terminate) {
      //不断等待直到需要执行GC
    sleepBeforeNextCycle();
      //如果_should_terminate为true则终止循环
    if (_should_terminate) break;
      //获取GC的原因
    GCCause::Cause cause = _collector->_full_gc_requested ?
      _collector->_full_gc_cause : GCCause::_cms_concurrent_mark;
      //在后台执行GC
    _collector->collect_in_background(false, cause);
  }
    //退出循环了，校验_should_terminate为true
  assert(_should_terminate, "just checking");
  // Check that the state of any protocol for synchronization
  // between background (CMS) and foreground collector is "clean"
  // (i.e. will not potentially block the foreground collector,
  // requiring action by us).
    //校验是否可以终止
  verify_ok_to_terminate();
  // Signal that it is terminated
  {
      //获取锁Terminator_lock
    MutexLockerEx mu(Terminator_lock,
                     Mutex::_no_safepoint_check_flag);
    assert(_cmst == this, "Weird!");
      //cmst置为NULL
    _cmst = NULL;
    Terminator_lock->notify();
  }

  // Thread destructor usually does this..
    //去掉CMSThread同本地线程的绑定
  ThreadLocalStorage::set_thread(NULL);
}

#ifndef PRODUCT
void ConcurrentMarkSweepThread::verify_ok_to_terminate() const {
  assert(!(CGC_lock->owned_by_self() || cms_thread_has_cms_token() ||
           cms_thread_wants_cms_token()),
         "Must renounce all worldly possessions and desires for nirvana");
  _collector->verify_ok_to_terminate();
}
#endif

// create and start a new ConcurrentMarkSweep Thread for given CMS generation
ConcurrentMarkSweepThread* ConcurrentMarkSweepThread::start(CMSCollector* collector) {
  if (!_should_terminate) {
    assert(cmst() == NULL, "start() called twice?");
      //创建一个新的ConcurrentMarkSweepThread实例
    ConcurrentMarkSweepThread* th = new ConcurrentMarkSweepThread(collector);
    assert(cmst() == th, "Where did the just-created CMS thread go?");
    return th;
  }
  return NULL;
}

void ConcurrentMarkSweepThread::stop() {
  if (CMSIncrementalMode) {
    // Disable incremental mode and wake up the thread so it notices the change.
    disable_icms();
    start_icms();
  }
  // it is ok to take late safepoints here, if needed
  {
    MutexLockerEx x(Terminator_lock);
    _should_terminate = true;
  }
  { // Now post a notify on CGC_lock so as to nudge
    // CMS thread(s) that might be slumbering in
    // sleepBeforeNextCycle.
    MutexLockerEx x(CGC_lock, Mutex::_no_safepoint_check_flag);
    CGC_lock->notify_all();
  }
  { // Now wait until (all) CMS thread(s) have exited
    MutexLockerEx x(Terminator_lock);
    while(cmst() != NULL) {
      Terminator_lock->wait();
    }
  }
}

void ConcurrentMarkSweepThread::threads_do(ThreadClosure* tc) {
  assert(tc != NULL, "Null ThreadClosure");
  if (_cmst != NULL) {
    tc->do_thread(_cmst);
  }
  assert(Universe::is_fully_initialized(),
         "Called too early, make sure heap is fully initialized");
  if (_collector != NULL) {
    AbstractWorkGang* gang = _collector->conc_workers();
    if (gang != NULL) {
      gang->threads_do(tc);
    }
  }
}

void ConcurrentMarkSweepThread::print_all_on(outputStream* st) {
  if (_cmst != NULL) {
    _cmst->print_on(st);
    st->cr();
  }
  if (_collector != NULL) {
    AbstractWorkGang* gang = _collector->conc_workers();
    if (gang != NULL) {
      gang->print_worker_threads_on(st);
    }
  }
}

void ConcurrentMarkSweepThread::synchronize(bool is_cms_thread) {
  assert(UseConcMarkSweepGC, "just checking");

  MutexLockerEx x(CGC_lock,
                  Mutex::_no_safepoint_check_flag);
  if (!is_cms_thread) {
    assert(Thread::current()->is_VM_thread(), "Not a VM thread");
    CMSSynchronousYieldRequest yr;
    while (CMS_flag_is_set(CMS_cms_has_token)) {
      // indicate that we want to get the token
      set_CMS_flag(CMS_vm_wants_token);
      CGC_lock->wait(true);
    }
    // claim the token and proceed
    clear_CMS_flag(CMS_vm_wants_token);
    set_CMS_flag(CMS_vm_has_token);
  } else {
    assert(Thread::current()->is_ConcurrentGC_thread(),
           "Not a CMS thread");
    // The following barrier assumes there's only one CMS thread.
    // This will need to be modified is there are more CMS threads than one.
    while (CMS_flag_is_set(CMS_vm_has_token | CMS_vm_wants_token)) {
      set_CMS_flag(CMS_cms_wants_token);
      CGC_lock->wait(true);
    }
    // claim the token
    clear_CMS_flag(CMS_cms_wants_token);
    set_CMS_flag(CMS_cms_has_token);
  }
}

void ConcurrentMarkSweepThread::desynchronize(bool is_cms_thread) {
  assert(UseConcMarkSweepGC, "just checking");

  MutexLockerEx x(CGC_lock,
                  Mutex::_no_safepoint_check_flag);
  if (!is_cms_thread) {
    assert(Thread::current()->is_VM_thread(), "Not a VM thread");
    assert(CMS_flag_is_set(CMS_vm_has_token), "just checking");
    clear_CMS_flag(CMS_vm_has_token);
    if (CMS_flag_is_set(CMS_cms_wants_token)) {
      // wake-up a waiting CMS thread
      CGC_lock->notify();
    }
    assert(!CMS_flag_is_set(CMS_vm_has_token | CMS_vm_wants_token),
           "Should have been cleared");
  } else {
    assert(Thread::current()->is_ConcurrentGC_thread(),
           "Not a CMS thread");
    assert(CMS_flag_is_set(CMS_cms_has_token), "just checking");
    clear_CMS_flag(CMS_cms_has_token);
    if (CMS_flag_is_set(CMS_vm_wants_token)) {
      // wake-up a waiting VM thread
      CGC_lock->notify();
    }
    assert(!CMS_flag_is_set(CMS_cms_has_token | CMS_cms_wants_token),
           "Should have been cleared");
  }
}

// Wait until any cms_lock event
void ConcurrentMarkSweepThread::wait_on_cms_lock(long t_millis) {
    //获取锁CGC_lock
  MutexLockerEx x(CGC_lock,
                  Mutex::_no_safepoint_check_flag);
  if (_should_terminate || _collector->_full_gc_requested) {
    return;
  }
  set_CMS_flag(CMS_cms_wants_token);   // to provoke notifies
    //等待最长t_millis，期间可能被唤醒
  CGC_lock->wait(Mutex::_no_safepoint_check_flag, t_millis);
  clear_CMS_flag(CMS_cms_wants_token);
  assert(!CMS_flag_is_set(CMS_cms_has_token | CMS_cms_wants_token),
         "Should not be set");
}

// Wait until the next synchronous GC, a concurrent full gc request,
// or a timeout, whichever is earlier.
void ConcurrentMarkSweepThread::wait_on_cms_lock_for_scavenge(long t_millis) {
  // Wait time in millis or 0 value representing infinite wait for a scavenge
  assert(t_millis >= 0, "Wait time for scavenge should be 0 or positive");

  GenCollectedHeap* gch = GenCollectedHeap::heap();
  double start_time_secs = os::elapsedTime();
    //计算结束时间
  double end_time_secs = start_time_secs + (t_millis / ((double) MILLIUNITS));

  // Total collections count before waiting loop
    //获取当前的垃圾收集次数
  unsigned int before_count;
  {
    MutexLockerEx hl(Heap_lock, Mutex::_no_safepoint_check_flag);
    before_count = gch->total_collections();
  }

  unsigned int loop_count = 0;

  while(!_should_terminate) {
    double now_time = os::elapsedTime();
    long wait_time_millis;

    if(t_millis != 0) {
      // New wait limit
        //计算需要等待的时间
      wait_time_millis = (long) ((end_time_secs - now_time) * MILLIUNITS);
      if(wait_time_millis <= 0) {
        // Wait time is over
          //超时了退出循环
        break;
      }
    } else {
      // No wait limit, wait if necessary forever
      wait_time_millis = 0;
    }

    // Wait until the next event or the remaining timeout
    {
      MutexLockerEx x(CGC_lock, Mutex::_no_safepoint_check_flag);

        //如果需要终止CMSThread或者执行full GC则退出循环
      if (_should_terminate || _collector->_full_gc_requested) {
        return;
      }
      set_CMS_flag(CMS_cms_wants_token);   // to provoke notifies
      assert(t_millis == 0 || wait_time_millis > 0, "Sanity");
        //CGC_lock上等待最多wait_time_millis秒
      CGC_lock->wait(Mutex::_no_safepoint_check_flag, wait_time_millis);
      clear_CMS_flag(CMS_cms_wants_token);
      assert(!CMS_flag_is_set(CMS_cms_has_token | CMS_cms_wants_token),
             "Should not be set");
    }

    // Extra wait time check before entering the heap lock to get the collection count
      //CGC_lock上等待的线程被唤醒了，如果当前时间超过结束时间了
    if(t_millis != 0 && os::elapsedTime() >= end_time_secs) {
      // Wait time is over
        //超时终止循环
      break;
    }

    // Total collections count after the event
      //获取此时的垃圾回收次数
    unsigned int after_count;
    {
      MutexLockerEx hl(Heap_lock, Mutex::_no_safepoint_check_flag);
      after_count = gch->total_collections();
    }

    if(before_count != after_count) {
      // There was a collection - success
        //如果垃圾回收次数变了，说明已经执行过一遍GC了，终止循环
      break;
    }

    // Too many loops warning
      //增加循环次数，一直不断循环可能变成负值，等于0时说明循环的次数很大了，打印warning日志
    if(++loop_count == 0) {
      warning("wait_on_cms_lock_for_scavenge() has looped %u times", loop_count - 1);
    }
  }
}

void ConcurrentMarkSweepThread::sleepBeforeNextCycle() {
  while (!_should_terminate) {
    if (CMSIncrementalMode) {
        //如果开启增量模式
        //等待重新获取CPU
      icms_wait();
      if(CMSWaitDuration >= 0) {
        // Wait until the next synchronous GC, a concurrent full gc
        // request or a timeout, whichever is earlier.
        wait_on_cms_lock_for_scavenge(CMSWaitDuration);
      }
      return;
    } else {
        //CMSWaitDuration表示CMSThread等待young gc的时间，默认值是2000
      if(CMSWaitDuration >= 0) {
        // Wait until the next synchronous GC, a concurrent full gc
        // request or a timeout, whichever is earlier.
          //等待直到超时或者需要一次Full GC或者已经完成了一次Full GC
        wait_on_cms_lock_for_scavenge(CMSWaitDuration);
      } else {
        // Wait until any cms_lock event or check interval not to call shouldConcurrentCollect permanently
          //CMSCheckInterval表示CMSThread检查是否应该执行GC的间隔时间，默认是1000ms
          //跟wait_on_cms_lock_for_scavenge相比，就是不会循环等待，只等待一次
        wait_on_cms_lock(CMSCheckInterval);
      }
    }
    // Check if we should start a CMS collection cycle
      //如果需要开始GC则终止循环
    if (_collector->shouldConcurrentCollect()) {
      return;
    }
    // .. collection criterion not yet met, let's go back
    // and wait some more
  }
}

// Incremental CMS
// start_icms方法用于通知CMS Thread重新开始执行
void ConcurrentMarkSweepThread::start_icms() {
  assert(UseConcMarkSweepGC && CMSIncrementalMode, "just checking");
    //获取锁iCMS_lock
  MutexLockerEx x(iCMS_lock, Mutex::_no_safepoint_check_flag);
  trace_state("start_icms");
    //将_should_run置为true
  _should_run = true;
    //唤醒所有等待的线程，判断should_run变成true了重新开始执行
  iCMS_lock->notify_all();
}

// stop_icms方法用于通知CMS Thread暂停执行，在iCMS_lock上等待
void ConcurrentMarkSweepThread::stop_icms() {
  assert(UseConcMarkSweepGC && CMSIncrementalMode, "just checking");
    //获取锁iCMS_lock
  MutexLockerEx x(iCMS_lock, Mutex::_no_safepoint_check_flag);
    //_should_stop正常情况为false
  if (!_should_stop) {
    trace_state("stop_icms");
      //将_should_stop置为true
    _should_stop = true;
    _should_run = false;
      //增加计数器
    asynchronous_yield_request();
      //唤醒所有在iCMS_lock上等待的线程
    iCMS_lock->notify_all();
  }
}

// icms_wait是CMS Thread使用的，会检查是否需要暂停执行
void ConcurrentMarkSweepThread::icms_wait() {
  assert(UseConcMarkSweepGC && CMSIncrementalMode, "just checking");
    //如果调用了stop_icms则_should_stop变成true
  if (_should_stop && icms_is_enabled()) {
      //获取锁iCMS_lock
    MutexLockerEx x(iCMS_lock, Mutex::_no_safepoint_check_flag);
    trace_state("pause_icms");
      //停止CMS的计时器
    _collector->stats().stop_cms_timer();
    while(!_should_run && icms_is_enabled()) {
        //不断循环等待，直到_should_run变成true
      iCMS_lock->wait(Mutex::_no_safepoint_check_flag);
    }
      //重新开启计时器
    _collector->stats().start_cms_timer();
      //重置_should_stop
    _should_stop = false;
    trace_state("pause_icms end");
  }
}

// Note: this method, although exported by the ConcurrentMarkSweepThread,
// which is a non-JavaThread, can only be called by a JavaThread.
// Currently this is done at vm creation time (post-vm-init) by the
// main/Primordial (Java)Thread.
// XXX Consider changing this in the future to allow the CMS thread
// itself to create this thread?
void ConcurrentMarkSweepThread::makeSurrogateLockerThread(TRAPS) {
  assert(UseConcMarkSweepGC, "SLT thread needed only for CMS GC");
  assert(_slt == NULL, "SLT already created");
  _slt = SurrogateLockerThread::make(THREAD);
}
