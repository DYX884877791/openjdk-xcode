/*
 * Copyright (c) 1998, 2014, Oracle and/or its affiliates. All rights reserved.
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
#include "classfile/vmSymbols.hpp"
#include "jfr/jfrEvents.hpp"
#include "memory/resourceArea.hpp"
#include "oops/markOop.hpp"
#include "oops/oop.inline.hpp"
#include "runtime/biasedLocking.hpp"
#include "runtime/handles.inline.hpp"
#include "runtime/interfaceSupport.hpp"
#include "runtime/mutexLocker.hpp"
#include "runtime/objectMonitor.hpp"
#include "runtime/objectMonitor.inline.hpp"
#include "runtime/osThread.hpp"
#include "runtime/stubRoutines.hpp"
#include "runtime/synchronizer.hpp"
#include "runtime/thread.inline.hpp"
#include "utilities/dtrace.hpp"
#include "utilities/events.hpp"
#include "utilities/preserveException.hpp"
#ifdef TARGET_OS_FAMILY_linux
# include "os_linux.inline.hpp"
#endif
#ifdef TARGET_OS_FAMILY_solaris
# include "os_solaris.inline.hpp"
#endif
#ifdef TARGET_OS_FAMILY_windows
# include "os_windows.inline.hpp"
#endif
#ifdef TARGET_OS_FAMILY_bsd
# include "os_bsd.inline.hpp"
#endif

#if defined(__GNUC__) && !defined(PPC64)
  // Need to inhibit inlining for older versions of GCC to avoid build-time failures
  #define ATTR __attribute__((noinline))
#else
  #define ATTR
#endif

PRAGMA_FORMAT_MUTE_WARNINGS_FOR_GCC

// The "core" versions of monitor enter and exit reside in this file.
// The interpreter and compilers contain specialized transliterated
// variants of the enter-exit fast-path operations.  See i486.ad fast_lock(),
// for instance.  If you make changes here, make sure to modify the
// interpreter, and both C1 and C2 fast-path inline locking code emission.
//
//
// -----------------------------------------------------------------------------

#ifdef DTRACE_ENABLED

// Only bother with this argument setup if dtrace is available
// TODO-FIXME: probes should not fire when caller is _blocked.  assert() accordingly.

#define DTRACE_MONITOR_PROBE_COMMON(obj, thread)                           \
  char* bytes = NULL;                                                      \
  int len = 0;                                                             \
  jlong jtid = SharedRuntime::get_java_tid(thread);                        \
  Symbol* klassname = ((oop)(obj))->klass()->name();                       \
  if (klassname != NULL) {                                                 \
    bytes = (char*)klassname->bytes();                                     \
    len = klassname->utf8_length();                                        \
  }

#ifndef USDT2
HS_DTRACE_PROBE_DECL5(hotspot, monitor__wait,
  jlong, uintptr_t, char*, int, long);
HS_DTRACE_PROBE_DECL4(hotspot, monitor__waited,
  jlong, uintptr_t, char*, int);

#define DTRACE_MONITOR_WAIT_PROBE(monitor, obj, thread, millis)            \
  {                                                                        \
    if (DTraceMonitorProbes) {                                            \
      DTRACE_MONITOR_PROBE_COMMON(obj, thread);                            \
      HS_DTRACE_PROBE5(hotspot, monitor__wait, jtid,                       \
                       (monitor), bytes, len, (millis));                   \
    }                                                                      \
  }

#define DTRACE_MONITOR_PROBE(probe, monitor, obj, thread)                  \
  {                                                                        \
    if (DTraceMonitorProbes) {                                            \
      DTRACE_MONITOR_PROBE_COMMON(obj, thread);                            \
      HS_DTRACE_PROBE4(hotspot, monitor__##probe, jtid,                    \
                       (uintptr_t)(monitor), bytes, len);                  \
    }                                                                      \
  }

#else /* USDT2 */

#define DTRACE_MONITOR_WAIT_PROBE(monitor, obj, thread, millis)            \
  {                                                                        \
    if (DTraceMonitorProbes) {                                            \
      DTRACE_MONITOR_PROBE_COMMON(obj, thread);                            \
      HOTSPOT_MONITOR_WAIT(jtid,                                           \
                           (uintptr_t)(monitor), bytes, len, (millis));  \
    }                                                                      \
  }

#define HOTSPOT_MONITOR_PROBE_waited HOTSPOT_MONITOR_WAITED

#define DTRACE_MONITOR_PROBE(probe, monitor, obj, thread)                  \
  {                                                                        \
    if (DTraceMonitorProbes) {                                            \
      DTRACE_MONITOR_PROBE_COMMON(obj, thread);                            \
      HOTSPOT_MONITOR_PROBE_##probe(jtid, /* probe = waited */             \
                       (uintptr_t)(monitor), bytes, len);                  \
    }                                                                      \
  }

#endif /* USDT2 */
#else //  ndef DTRACE_ENABLED

#define DTRACE_MONITOR_WAIT_PROBE(obj, thread, millis, mon)    {;}
#define DTRACE_MONITOR_PROBE(probe, obj, thread, mon)          {;}

#endif // ndef DTRACE_ENABLED

// This exists only as a workaround of dtrace bug 6254741
int dtrace_waited_probe(ObjectMonitor* monitor, Handle obj, Thread* thr) {
  DTRACE_MONITOR_PROBE(waited, monitor, obj(), thr);
  return 0;
}

//指针数组，通过原子的修改数组元素的值来实现膨胀锁的功能
#define NINFLATIONLOCKS 256
static volatile intptr_t InflationLocks [NINFLATIONLOCKS] ;

ObjectMonitor * volatile ObjectSynchronizer::gBlockList = NULL;
ObjectMonitor * volatile ObjectSynchronizer::gFreeList  = NULL ;
ObjectMonitor * volatile ObjectSynchronizer::gOmInUseList  = NULL ;
int ObjectSynchronizer::gOmInUseCount = 0;
// 操作gFreeList的锁
static volatile intptr_t ListLock = 0 ;      // protects global monitor free-list cache
//gFreeList中包含的元素的个数
static volatile int MonitorFreeCount  = 0 ;      // # on gFreeList
// 已经创建的ObjectMonitor的总数
static volatile int MonitorPopulation = 0 ;      // # Extant -- in circulation
#define CHAINMARKER (cast_to_oop<intptr_t>(-1))

// -----------------------------------------------------------------------------
//  Fast Monitor Enter/Exit
// This the fast monitor enter. The interpreter and compiler use
// some assembly copies of this code. Make sure update those code
// if the following function is changed. The implementation is
// extremely sensitive to race condition. Be careful.

/**
 * fast_enter实现简单流程：
 *
 * 1. 再次检查偏向锁是否已开启
 * 2. 当处于不安全点时，通过 revoke_and_rebias尝试获取偏向锁，如果成功则直接返回，如果失败则进入轻量级锁获取过程
 * 3. revoke_and_rebias这个偏向锁的获取逻辑在 biasedLocking.cpp中
 * 4. 如果偏向锁未开启，则进入 slow_enter获取轻量级锁的流程
 *
 * 偏向锁顾名思义会偏向于一个线程，如果有其他线程来抢占这个偏向锁则会导致偏向锁被撤销，恢复成无锁状态或者直接膨胀成轻量级锁。
 * 当偏向锁没有被某个线程占用，即对象头中用于保存线程指针的位都是0的时候，这个偏向锁就是匿名偏向锁。
 *
 * 撤销偏向锁就是将锁对象oop的对象头恢复成无锁状态或者膨胀成轻量级锁状态，执行撤销动作的前提是锁对象oop的对象头处于偏向锁状态。具体而言有以下几种情形：
 *
 * 1、执行Object类的hashcode方法，会将其恢复成无锁状态。
 * 2、执行Object类的wait/notify/notifyall方法，会将其恢复成无锁状态，直接膨胀成重量级锁。
 * 3、执行jni_MonitorEnter或者jni_MonitorExit方法，会将其恢复成无锁状态，直接膨胀成重量级锁。
 * 4、执行Unsafe类的monitorenter/trymonitorenter/monitorexit方法，会将其恢复成无锁状态，直接膨胀成重量级锁。
 * 5、尝试获取某个偏向锁，如果该偏向锁被某个线程占用了，但是没有关联的BasicObjectLock，即实际占用该偏向锁的方法已经退出了，则会将其恢复成无锁状态，
 *      然后膨胀成轻量级锁，但是在撤销一定次数后触发批量重偏向（rebasic）的情形下也可能重新获取该偏向锁。如果该偏向锁正在被某个方法所使用，
 *      即存在对应的BasicObjectLock，则直接将该偏向锁膨胀成轻量级锁。
 *
 *  注意偏向锁的撤销大部分情形下都是需要在安全点下执行，因为需要遍历其他线程的所有调用栈帧，判断是否存在与之关联的BasicObjectLock。在以下情形不需要在安全点下执行：
 *
 * 1、目标对象的对象头是匿名偏向锁状态
 * 2、目标对象的Klass的prototype_header变成无锁状态
 * 3、目标对象的Klass的prototype_header中的epoch值和目标对象对象头中的epoch值不一样
 * 4、目标对象的偏向锁由当前线程持有
 */
void ObjectSynchronizer::fast_enter(Handle obj, BasicLock* lock, bool attempt_rebias, TRAPS) {
 slog_debug("进入hotspot/src/share/vm/runtime/synchronizer.cpp中的ObjectSynchronizer::fast_enter函数...");
    //判断是否开启了偏向锁
 if (UseBiasedLocking) {
     //如果不处于全局安全点
    if (!SafepointSynchronize::is_at_safepoint()) {
      slog_debug("当前不处于全局安全点...");
        //通过`revoke_and_rebias`这个函数尝试获取偏向锁
      BiasedLocking::Condition cond = BiasedLocking::revoke_and_rebias(obj, attempt_rebias, THREAD);
        //如果是撤销与重偏向直接返回
      if (cond == BiasedLocking::BIAS_REVOKED_AND_REBIASED) {
        slog_debug("获取到的偏向锁的状态为重偏向,直接返回...");
        return;
      }
    } else {
      slog_debug("当前处于全局安全点,撤销偏向锁...");
        //如果在安全点，撤销偏向锁
      assert(!attempt_rebias, "can not rebias toward VM thread");
        //当到达一个全局安全点时，这时会根据偏向锁的状态来判断是否需要撤销偏向锁，调用 revoke_at_safepoint方法
      BiasedLocking::revoke_at_safepoint(obj);
    }
    assert(!obj->mark()->has_bias_pattern(), "biases should be revoked by now");
 }

 slow_enter (obj, lock, THREAD) ;
}

// 轻量级锁的释放也比较简单，就是将当前线程栈帧中锁记录空间中的Mark Word替换到锁对象的对象头中，如果成功表示锁释放成功。否则，锁膨胀成重量级锁，实现重量级锁的释放锁逻辑。
void ObjectSynchronizer::fast_exit(oop object, BasicLock* lock, TRAPS) {
  assert(!object->mark()->has_bias_pattern(), "should not see bias pattern here");
  // if displaced header is null, the previous enter is recursive enter, no-op
    //获取锁对象中的对象头
  markOop dhw = lock->displaced_header();
  markOop mark ;
  if (dhw == NULL) {
     // Recursive stack-lock.
     // Diagnostics -- Could be: stack-locked, inflating, inflated.
     mark = object->mark() ;
     assert (!mark->is_neutral(), "invariant") ;
     if (mark->has_locker() && mark != markOopDesc::INFLATING()) {
        assert(THREAD->is_lock_owned((address)mark->locker()), "invariant") ;
     }
     if (mark->has_monitor()) {
        ObjectMonitor * m = mark->monitor() ;
        assert(((oop)(m->object()))->mark() == mark, "invariant") ;
        assert(m->is_entered(THREAD), "invariant") ;
     }
     return ;
  }

    //获取线程栈帧中锁记录(LockRecord)中的markword
  mark = object->mark() ;

  // If the object is stack-locked by the current thread, try to
  // swing the displaced header from the box back to the mark.
  if (mark == (markOop) lock) {
     assert (dhw->is_neutral(), "invariant") ;
      //通过CAS尝试将Displaced Mark Word替换回对象头，如果成功，表示锁释放成功。
     if ((markOop) Atomic::cmpxchg_ptr (dhw, object->mark_addr(), mark) == mark) {
        TEVENT (fast_exit: release stacklock) ;
        return;
     }
  }

    //锁膨胀，调用重量级锁的释放锁方法
  ObjectSynchronizer::inflate(THREAD,
                              object,
                              inflate_cause_vm_internal)->exit(true, THREAD);
}

// -----------------------------------------------------------------------------
// Interpreter/Compiler Slow Case
// This routine is used to handle interpreter/compiler slow case
// We don't need to use fast path here, because it must have been
// failed in the interpreter/compiler code.
// 简单整理轻量级锁的获取逻辑：
//  1. mark->is_neutral()方法, is_neutral这个方法是在 markOop.hpp中定义，如果 biased_lock:0且lock:01表示无锁状态
//  2. 如果mark处于无锁状态，则进入下一步骤，否则执行最后一个步骤
//  3. 把mark保存到BasicLock对象的_displaced_header字段
//  4. 通过CAS尝试将markword更新为指向BasicLock对象的指针lock，如果更新成功，表示竞争到锁，则执行同步代码，否则执行下一步骤
//  5. 如果当前mark处于加锁状态，且mark中的ptr指针指向当前线程的栈帧，则执行同步代码，否则说明有多个线程竞争轻量级锁，轻量级锁需要膨胀升级为重量级锁
void ObjectSynchronizer::slow_enter(Handle obj, BasicLock* lock, TRAPS) {
  slog_debug("进入hotspot/src/share/vm/runtime/synchronizer.cpp中的ObjectSynchronizer::slow_enter函数...");
  markOop mark = obj->mark();
  assert(!mark->has_bias_pattern(), "should not see bias pattern here");

    //如果当前是无锁状态, markword的biase_lock:0，lock:01
  if (mark->is_neutral()) {
    // Anticipate successful CAS -- the ST of the displaced mark must
    // be visible <= the ST performed by the CAS.
      //直接把mark保存到BasicLock对象的_displaced_header字段
    lock->set_displaced_header(mark);
      //通过CAS将mark word更新为指向BasicLock对象的指针lock，更新成功表示获得了轻量级锁
    if (mark == (markOop) Atomic::cmpxchg_ptr(lock, obj()->mark_addr(), mark)) {
      TEVENT (slow_enter: release stacklock) ;
      return ;
    }
    // Fall through to inflate() ...
  } else
  if (mark->has_locker() && THREAD->is_lock_owned((address)mark->locker())) {
      //如果markword处于加锁状态、且markword中的ptr指针指向当前线程的栈帧，表示为重入操作，不需要争抢锁
    assert(lock != mark->locker(), "must not re-lock the same lock");
    assert(lock != (BasicLock*)obj->mark(), "don't relock with same BasicLock");
    lock->set_displaced_header(NULL);
    return;
  }

#if 0
  // The following optimization isn't particularly useful.
  if (mark->has_monitor() && mark->monitor()->is_entered(THREAD)) {
    lock->set_displaced_header (NULL) ;
    return ;
  }
#endif

  // The object header will never be displaced to this lock,
  // so it does not matter what the value is, except that it
  // must be non-zero to avoid looking like a re-entrant lock,
  // and must not look locked either.
    //代码执行到这里，说明有多个线程竞争轻量级锁，轻量级锁通过`inflate`进行膨胀升级为重量级锁
  lock->set_displaced_header(markOopDesc::unused_mark());
  ObjectSynchronizer::inflate(THREAD,
                              obj(),
                              inflate_cause_monitor_enter)->enter(THREAD);
}

// This routine is used to handle interpreter/compiler slow case
// We don't need to use fast path here, because it must have
// failed in the interpreter/compiler code. Simply use the heavy
// weight monitor should be ok, unless someone find otherwise.
void ObjectSynchronizer::slow_exit(oop object, BasicLock* lock, TRAPS) {
  fast_exit (object, lock, THREAD) ;
}

// -----------------------------------------------------------------------------
// Class Loader  support to workaround deadlocks on the class loader lock objects
// Also used by GC
// complete_exit()/reenter() are used to wait on a nested lock
// i.e. to give up an outer lock completely and then re-enter
// Used when holding nested locks - lock acquisition order: lock1 then lock2
//  1) complete_exit lock1 - saving recursion count
//  2) wait on lock2
//  3) when notified on lock2, unlock lock2
//  4) reenter lock1 with original recursion count
//  5) lock lock2
// NOTE: must use heavy weight monitor to handle complete_exit/reenter()
intptr_t ObjectSynchronizer::complete_exit(Handle obj, TRAPS) {
  TEVENT (complete_exit) ;
  if (UseBiasedLocking) {
    BiasedLocking::revoke_and_rebias(obj, false, THREAD);
    assert(!obj->mark()->has_bias_pattern(), "biases should be revoked by now");
  }

  ObjectMonitor* monitor = ObjectSynchronizer::inflate(THREAD,
                                                       obj(),
                                                       inflate_cause_vm_internal);

  return monitor->complete_exit(THREAD);
}

// NOTE: must use heavy weight monitor to handle complete_exit/reenter()
void ObjectSynchronizer::reenter(Handle obj, intptr_t recursion, TRAPS) {
  TEVENT (reenter) ;
  if (UseBiasedLocking) {
    BiasedLocking::revoke_and_rebias(obj, false, THREAD);
    assert(!obj->mark()->has_bias_pattern(), "biases should be revoked by now");
  }

  ObjectMonitor* monitor = ObjectSynchronizer::inflate(THREAD,
                                                       obj(),
                                                       inflate_cause_vm_internal);

  monitor->reenter(recursion, THREAD);
}
// -----------------------------------------------------------------------------
// JNI locks on java objects
// NOTE: must use heavy weight monitor to handle jni monitor enter
void ObjectSynchronizer::jni_enter(Handle obj, TRAPS) { // possible entry from jni enter
  // the current locking is from JNI instead of Java code
  TEVENT (jni_enter) ;
  if (UseBiasedLocking) {
    BiasedLocking::revoke_and_rebias(obj, false, THREAD);
    assert(!obj->mark()->has_bias_pattern(), "biases should be revoked by now");
  }
  THREAD->set_current_pending_monitor_is_from_java(false);
  ObjectSynchronizer::inflate(THREAD, obj(), inflate_cause_jni_enter)->enter(THREAD);
  THREAD->set_current_pending_monitor_is_from_java(true);
}

// NOTE: must use heavy weight monitor to handle jni monitor enter
bool ObjectSynchronizer::jni_try_enter(Handle obj, Thread* THREAD) {
  if (UseBiasedLocking) {
    BiasedLocking::revoke_and_rebias(obj, false, THREAD);
    assert(!obj->mark()->has_bias_pattern(), "biases should be revoked by now");
  }

  ObjectMonitor* monitor = ObjectSynchronizer::inflate_helper(obj());
  return monitor->try_enter(THREAD);
}


// NOTE: must use heavy weight monitor to handle jni monitor exit
void ObjectSynchronizer::jni_exit(oop obj, Thread* THREAD) {
  TEVENT (jni_exit) ;
  if (UseBiasedLocking) {
    Handle h_obj(THREAD, obj);
    BiasedLocking::revoke_and_rebias(h_obj, false, THREAD);
    obj = h_obj();
  }
  assert(!obj->mark()->has_bias_pattern(), "biases should be revoked by now");

  ObjectMonitor* monitor = ObjectSynchronizer::inflate(THREAD,
                                                       obj,
                                                       inflate_cause_jni_exit);
  // If this thread has locked the object, exit the monitor.  Note:  can't use
  // monitor->check(CHECK); must exit even if an exception is pending.
  if (monitor->check(THREAD)) {
     monitor->exit(true, THREAD);
  }
}

// -----------------------------------------------------------------------------
// Internal VM locks on java objects
// standard constructor, allows locking failures
ObjectLocker::ObjectLocker(Handle obj, Thread* thread, bool doLock) {
  slog_debug("进入hotspot/src/share/vm/runtime/synchronizer.cpp中的ObjectLocker::ObjectLocker构造函数...");
  _dolock = doLock;
  _thread = thread;
  debug_only(if (StrictSafepointChecks) _thread->check_for_valid_safepoint_state(false);)
  _obj = obj;

  if (_dolock) {
    TEVENT (ObjectLocker) ;

    slog_debug("在ObjectLocker::ObjectLocker构造函数中即将调用ObjectSynchronizer::fast_enter函数...");
    ObjectSynchronizer::fast_enter(_obj, &_lock, false, _thread);
  }
}

ObjectLocker::~ObjectLocker() {
  slog_debug("进入hotspot/src/share/vm/runtime/synchronizer.cpp中的ObjectLocker::~ObjectLocker析构函数...");
  if (_dolock) {
    slog_debug("在ObjectLocker::~ObjectLocker析构函数中即将调用ObjectSynchronizer::fast_exit函数...");
    ObjectSynchronizer::fast_exit(_obj(), &_lock, _thread);
  }
}


// -----------------------------------------------------------------------------
//  Wait/Notify/NotifyAll
// NOTE: must use heavy weight monitor to handle wait()
// 这三个方法就是Object的wait，notify，notifyAll方法的核心实现，wait方法会分配一个与该对象关联的ObjectMonitor然后调用其wait方法；
// notify和notifyall会校验对象是否持有轻量级锁，如果有则直接返回，因为如果持有轻量级锁说明其未调用wait方法，没有与之关联的ObjectMonitor实例，
// 如果没有则获取关联的ObjectMonitor实例，调用其notify和notifyall方法，在执行具体的逻辑前会检查获取的ObjectMonitor实例是否是当前线程持有的，
// 如果不是会抛出异常ObjectMonitor实例，如果先调用wait方法再调用notify或者notifyall方法则不报错。
void ObjectSynchronizer::wait(Handle obj, jlong millis, TRAPS) {
    //UseBiasedLocking默认为true
  if (UseBiasedLocking) {
      //撤销对象头中包含的偏向锁
    BiasedLocking::revoke_and_rebias(obj, false, THREAD);
    assert(!obj->mark()->has_bias_pattern(), "biases should be revoked by now");
  }
  if (millis < 0) {
    TEVENT (wait - throw IAX) ;
      //参数非法抛出异常
    THROW_MSG(vmSymbols::java_lang_IllegalArgumentException(), "timeout value is negative");
  }
    //分配一个关联的ObjectMonitor实例
  ObjectMonitor* monitor = ObjectSynchronizer::inflate(THREAD,
                                                       obj(),
                                                       inflate_cause_wait);

  DTRACE_MONITOR_WAIT_PROBE(monitor, obj(), THREAD, millis);
    //调用其wait方法
  monitor->wait(millis, true, THREAD);

  /* This dummy call is in place to get around dtrace bug 6254741.  Once
     that's fixed we can uncomment the following line and remove the call */
  // DTRACE_MONITOR_PROBE(waited, monitor, obj(), THREAD);
  dtrace_waited_probe(monitor, obj, THREAD);
}

void ObjectSynchronizer::waitUninterruptibly (Handle obj, jlong millis, TRAPS) {
  if (UseBiasedLocking) {
    BiasedLocking::revoke_and_rebias(obj, false, THREAD);
    assert(!obj->mark()->has_bias_pattern(), "biases should be revoked by now");
  }
  if (millis < 0) {
    TEVENT (wait - throw IAX) ;
    THROW_MSG(vmSymbols::java_lang_IllegalArgumentException(), "timeout value is negative");
  }
  ObjectSynchronizer::inflate(THREAD,
                              obj(),
                              inflate_cause_wait)->wait(millis, false, THREAD) ;
}

void ObjectSynchronizer::notify(Handle obj, TRAPS) {
 if (UseBiasedLocking) {
     //撤销对象头中包含的偏向锁
    BiasedLocking::revoke_and_rebias(obj, false, THREAD);
    assert(!obj->mark()->has_bias_pattern(), "biases should be revoked by now");
  }

  markOop mark = obj->mark();
  if (mark->has_locker() && THREAD->is_lock_owned((address)mark->locker())) {
      //如果该对象持有轻量级锁则直接返回，持有轻量级锁说明其对象头中没有包含ObjectMonitor指针
    return;
  }
    //获取与之关联的ObjectMonitor实例，调用其notify方法
  ObjectSynchronizer::inflate(THREAD,
                              obj(),
                              inflate_cause_notify)->notify(THREAD);
}

// NOTE: see comment of notify()
void ObjectSynchronizer::notifyall(Handle obj, TRAPS) {
  if (UseBiasedLocking) {
      //撤销对象头中包含的偏向锁
    BiasedLocking::revoke_and_rebias(obj, false, THREAD);
    assert(!obj->mark()->has_bias_pattern(), "biases should be revoked by now");
  }

  markOop mark = obj->mark();
  if (mark->has_locker() && THREAD->is_lock_owned((address)mark->locker())) {
      //如果该对象持有轻量级锁则直接返回
    return;
  }
    //获取与之关联的ObjectMonitor实例，调用其notify方法
  ObjectSynchronizer::inflate(THREAD,
                              obj(),
                              inflate_cause_notify)->notifyAll(THREAD);
}

// -----------------------------------------------------------------------------
// Hash Code handling
//
// Performance concern:
// OrderAccess::storestore() calls release() which at one time stored 0
// into the global volatile OrderAccess::dummy variable. This store was
// unnecessary for correctness. Many threads storing into a common location
// causes considerable cache migration or "sloshing" on large SMP systems.
// As such, I avoided using OrderAccess::storestore(). In some cases
// OrderAccess::fence() -- which incurs local latency on the executing
// processor -- is a better choice as it scales on SMP systems.
//
// See http://blogs.oracle.com/dave/entry/biased_locking_in_hotspot for
// a discussion of coherency costs. Note that all our current reference
// platforms provide strong ST-ST order, so the issue is moot on IA32,
// x64, and SPARC.
//
// As a general policy we use "volatile" to control compiler-based reordering
// and explicit fences (barriers) to control for architectural reordering
// performed by the CPU(s) or platform.

struct SharedGlobals {
    // These are highly shared mostly-read variables.
    // To avoid false-sharing they need to be the sole occupants of a $ line.
    double padPrefix [8];
    volatile int stwRandom ;
    volatile int stwCycle ;

    // Hot RW variables -- Sequester to avoid false-sharing
    double padSuffix [16];
    volatile int hcSequence ;
    double padFinal [8] ;
} ;

static SharedGlobals GVars ;
static int MonitorScavengeThreshold = 1000000 ;
static volatile int ForceMonitorScavenge = 0 ; // Scavenge required and pending

static markOop ReadStableMark (oop obj) {
  markOop mark = obj->mark() ;
  if (!mark->is_being_inflated()) {
      //如果没有处于锁膨胀的过程中，则直接返回，锁膨胀是一个非常短暂的中间状态
    return mark ;       // normal fast-path return
  }

  int its = 0 ;
    //相当于while(true)死循环
  for (;;) {
    markOop mark = obj->mark() ;
    if (!mark->is_being_inflated()) {
        //如果没有处于锁膨胀的过程中，则直接返回
      return mark ;    // normal fast-path return
    }

    // The object is being inflated by some other thread.
    // The caller of ReadStableMark() must wait for inflation to complete.
    // Avoid live-lock
    // TODO: consider calling SafepointSynchronize::do_call_back() while
    // spinning to see if there's a safepoint pending.  If so, immediately
    // yielding or blocking would be appropriate.  Avoid spinning while
    // there is a safepoint pending.
    // TODO: add inflation contention performance counters.
    // TODO: restrict the aggregate number of spinners.

      //如果该对象处于锁膨胀的过程中，则调用方必须等待锁膨胀完成
    ++its ;
    if (its > 10000 || !os::is_MP()) {
        //循环超过1000此或者是单核系统
       if (its & 1) {
           //如果是奇数次循环，让当前线程yeild
         os::NakedYield() ;
         TEVENT (Inflate: INFLATING - yield) ;
       } else {
         // Note that the following code attenuates the livelock problem but is not
         // a complete remedy.  A more complete solution would require that the inflating
         // thread hold the associated inflation lock.  The following code simply restricts
         // the number of spinners to at most one.  We'll have N-2 threads blocked
         // on the inflationlock, 1 thread holding the inflation lock and using
         // a yield/park strategy, and 1 thread in the midst of inflation.
         // A more refined approach would be to change the encoding of INFLATING
         // to allow encapsulation of a native thread pointer.  Threads waiting for
         // inflation to complete would use CAS to push themselves onto a singly linked
         // list rooted at the markword.  Once enqueued, they'd loop, checking a per-thread flag
         // and calling park().  When inflation was complete the thread that accomplished inflation
         // would detach the list and set the markword to inflated with a single CAS and
         // then for each thread on the list, set the flag and unpark() the thread.
         // This is conceptually similar to muxAcquire-muxRelease, except that muxRelease
         // wakes at most one thread whereas we need to wake the entire list.
           //根据对象地址算出用于表示膨胀锁的指针数组的索引
         int ix = (cast_from_oop<intptr_t>(obj) >> 5) & (NINFLATIONLOCKS-1) ;
         int YieldThenBlock = 0 ;
         assert (ix >= 0 && ix < NINFLATIONLOCKS, "invariant") ;
         assert ((NINFLATIONLOCKS & (NINFLATIONLOCKS-1)) == 0, "invariant") ;
           //获取对应的膨胀锁
         Thread::muxAcquire (InflationLocks + ix, "InflationLock") ;
         while (obj->mark() == markOopDesc::INFLATING()) {
           // Beware: NakedYield() is advisory and has almost no effect on some platforms
           // so we periodically call Self->_ParkEvent->park(1).
           // We use a mixed spin/yield/block mechanism.
             // 如果还是处于膨胀过程中
           if ((YieldThenBlock++) >= 16) {
               //让当前线程park
              Thread::current()->_ParkEvent->park(1) ;
           } else {
              os::NakedYield() ;
           }
         }
           //释放膨胀锁
         Thread::muxRelease (InflationLocks + ix ) ;
         TEVENT (Inflate: INFLATING - yield/park) ;
       }
    } else {
        //执行自旋，实际就是直接返回0
       SpinPause() ;       // SMP-polite spinning
    }
  }
}

// hashCode() generation :
//
// Possibilities:
// * MD5Digest of {obj,stwRandom}
// * CRC32 of {obj,stwRandom} or any linear-feedback shift register function.
// * A DES- or AES-style SBox[] mechanism
// * One of the Phi-based schemes, such as:
//   2654435761 = 2^32 * Phi (golden ratio)
//   HashCodeValue = ((uintptr_t(obj) >> 3) * 2654435761) ^ GVars.stwRandom ;
// * A variation of Marsaglia's shift-xor RNG scheme.
// * (obj ^ stwRandom) is appealing, but can result
//   in undesirable regularity in the hashCode values of adjacent objects
//   (objects allocated back-to-back, in particular).  This could potentially
//   result in hashtable collisions and reduced hashtable efficiency.
//   There are simple ways to "diffuse" the middle address bits over the
//   generated hashCode values:
//

// get_next_hash方法用于生成一个新的hash码，根据配置参数hashCode有不同的实现，从JDK6开始，该参数的默认值是5，此时采用Marsaglia's xor-shift随机算法给对象生成一个hash码，跟对象的内存地址没有任何关系
static inline intptr_t get_next_hash(Thread * Self, oop obj) {
  intptr_t value = 0 ;
    //hashCode是一个配置选项，从JDK6开始，其默认值是5
  if (hashCode == 0) {
     // This form uses an unguarded global Park-Miller RNG,
     // so it's possible for two threads to race and generate the same RNG.
     // On MP system we'll have lots of RW access to a global, so the
     // mechanism induces lots of coherency traffic.
      //获取随机数
     value = os::random() ;
  } else
  if (hashCode == 1) {
     // This variation has the property of being stable (idempotent)
     // between STW operations.  This can be useful in some of the 1-0
     // synchronization schemes.
      //根据对象地址计算
     intptr_t addrBits = cast_from_oop<intptr_t>(obj) >> 3 ;
     value = addrBits ^ (addrBits >> 5) ^ GVars.stwRandom ;
  } else
  if (hashCode == 2) {
     value = 1 ;            // for sensitivity testing
  } else
  if (hashCode == 3) {
      //返回一个序列值
     value = ++GVars.hcSequence ;
  } else
  if (hashCode == 4) {
      //直接返回对象地址
     value = cast_from_oop<intptr_t>(obj) ;
  } else {
     // Marsaglia's xor-shift scheme with thread-specific state
     // This is probably the best overall implementation -- we'll
     // likely make this the default in future releases.
      // Marsaglia's xor-shift 算法，默认实现
      //_hashStateX是线程创建时生成的随机数，
     unsigned t = Self->_hashStateX ;
     t ^= (t << 11) ;
      //_hashStateY,_hashStateZ,_hashStateW初始化时是固定值
      //通过重置_hashStateW，来动态改变_hashStateX，_hashStateY，_hashStateZ的属性
     Self->_hashStateX = Self->_hashStateY ;
     Self->_hashStateY = Self->_hashStateZ ;
     Self->_hashStateZ = Self->_hashStateW ;
     unsigned v = Self->_hashStateW ;
     v = (v ^ (v >> 19)) ^ (t ^ (t >> 8)) ;
     Self->_hashStateW = v ;
     value = v ;
  }

    //因为hash值是保存在对象头的特定位上，此处且运算，检查对应位的hash值是否是0
  value &= markOopDesc::hash_mask;
  if (value == 0) value = 0xBAD ;
  assert (value != markOopDesc::no_hash, "invariant") ;
  TEVENT (hashCode: GENERATE) ;
  return value;
}
//
// FastHashCode是Object类的hashCode方法和System类的identityHashCode方法的底层实现，identity_hash_value_for方法是JVM内部使用的获取某个oop的hash码，
// 用于获取匿名类的类名或者将某个oop放入Map时据此计算key，该方法也是基于FastHashCode实现的。
//
// 该方法会根据对象是否持有锁以及锁的类型做不同的处理，如果是无锁的，则直接返回对象头中包含的hash码，如果对象头中不存在hash码则创建一个并在对象头中保存；
// 如果持有轻量级锁，则根据对象头中保存的指向当前线程调用栈帧的指针找到该对象原有的对象头，如果对象头中包含有hash码则返回，如果没有则需要将当前对象的锁膨胀成监视器锁，
// 并生成一个新的hash码保存到对象头中；如果持有监视器锁，即重量级锁，则根据对象头中保存的代表监视器锁的ObjectMonitor的指针获取关联的ObjectMonitor，
// 从中获取保存的对象的原有对象头，如果包含hash码则返回，如果不包含则创建一个新的hash码并保存到对象头中。
intptr_t ObjectSynchronizer::FastHashCode (Thread * Self, oop obj) {
    //UseBiasedLocking表示是否启用偏向锁
  if (UseBiasedLocking) {
    // NOTE: many places throughout the JVM do not expect a safepoint
    // to be taken here, in particular most operations on perm gen
    // objects. However, we only ever bias Java instances and all of
    // the call sites of identity_hash that might revoke biases have
    // been checked to make sure they can handle a safepoint. The
    // added check of the bias pattern is to avoid useless calls to
    // thread-local storage.
    if (obj->mark()->has_bias_pattern()) {
      // Box and unbox the raw reference just in case we cause a STW safepoint.
      Handle hobj (Self, obj) ;
      // Relaxing assertion for bug 6320749.
      assert (Universe::verify_in_progress() ||
              !SafepointSynchronize::is_at_safepoint(),
             "biases should not be seen by VM thread here");
        //撤销偏向锁
      BiasedLocking::revoke_and_rebias(hobj, false, JavaThread::current());
      obj = hobj() ;
      assert(!obj->mark()->has_bias_pattern(), "biases should be revoked by now");
    }
  }

  // hashCode() is a heap mutator ...
  // Relaxing assertion for bug 6320749.
  assert (Universe::verify_in_progress() ||
          !SafepointSynchronize::is_at_safepoint(), "invariant") ;
  assert (Universe::verify_in_progress() ||
          Self->is_Java_thread() , "invariant") ;
  assert (Universe::verify_in_progress() ||
         ((JavaThread *)Self)->thread_state() != _thread_blocked, "invariant") ;

  ObjectMonitor* monitor = NULL;
  markOop temp, test;
  intptr_t hash;
    //如果该对象处于锁膨胀的过程中则通过自旋或者阻塞的方式等待锁膨胀完成
  markOop mark = ReadStableMark (obj);

  // object should remain ineligible for biased locking
  assert (!mark->has_bias_pattern(), "invariant") ;

  if (mark->is_neutral()) {
      //如果该对象没有持有锁
    hash = mark->hash();              // this is a normal header
    if (hash) {                       // if it has hash, just return it
        //如果对象头中保存有hash码则直接返回从对象头中获取的hash码
      return hash;
    }
      //对象头中没有hash码，计算一个新的hash码
    hash = get_next_hash(Self, obj);  // allocate a new hash code
      //将hash码保存到对象头中
    temp = mark->copy_set_hash(hash); // merge the hash code into header
    // use (machine word version) atomic operation to install the hash
      //原子地重置对象头
    test = (markOop) Atomic::cmpxchg_ptr(temp, obj->mark_addr(), mark);
    if (test == mark) {
        //重置成功
      return hash;
    }
    // If atomic operation failed, we must inflate the header
    // into heavy weight monitor. We could add more code here
    // for fast path, but it does not worth the complexity.
      //重置失败，有其他线程修改了该对象的对象头
  } else if (mark->has_monitor()) {
      //如果持有监视器锁即重量级锁，从对象头中获取监视器的指针
    monitor = mark->monitor();
      //获取ObjectMonitor保存的对象原来的对象头
    temp = monitor->header();
    assert (temp->is_neutral(), "invariant") ;
    hash = temp->hash();
    if (hash) {
        //如果对象头中有hash码，则直接返回
      return hash;
    }
    // Skip to the following code to reduce code size
      // 对象头中无hash码
  } else if (Self->is_lock_owned((address)mark->locker())) {
      //如果持有轻量级锁，去掉对象头中的锁表示，此时对象头包含的指针是指向线程栈的某个地址
      //该地址保存了该对象原来的对象头，displaced_mark_helper就是获取原来的对象头
    temp = mark->displaced_mark_helper(); // this is a lightweight monitor owned
    assert (temp->is_neutral(), "invariant") ;
    hash = temp->hash();              // by current thread, check if the displaced
    if (hash) {                       // header contains hash code
      return hash;
    }
    // WARNING:
    //   The displaced header is strictly immutable.
    // It can NOT be changed in ANY cases. So we have
    // to inflate the header into heavyweight monitor
    // even the current thread owns the lock. The reason
    // is the BasicLock (stack slot) will be asynchronously
    // read by other threads during the inflate() function.
    // Any change to stack may not propagate to other threads
    // correctly.
  }

  // Inflate the monitor to set hash code
    //进行锁膨胀，获取该对象关联的ObjectMonitor
  monitor = ObjectSynchronizer::inflate(Self, obj, inflate_cause_hash_code);
  // Load displaced header and check it has hash code
    //获取该对象的对象头
  mark = monitor->header();
  assert (mark->is_neutral(), "invariant") ;
  hash = mark->hash();
  if (hash == 0) {
      //如果没有hash码则生成一个
    hash = get_next_hash(Self, obj);
      //保存hash码
    temp = mark->copy_set_hash(hash); // merge hash code into header
    assert (temp->is_neutral(), "invariant") ;
    test = (markOop) Atomic::cmpxchg_ptr(temp, monitor, mark);
    if (test != mark) {
      // The only update to the header in the monitor (outside GC)
      // is install the hash code. If someone add new usage of
      // displaced header, please update this code
        //如果原子地更新失败，则返回原来的hash码
      hash = test->hash();
      assert (test->is_neutral(), "invariant") ;
      assert (hash != 0, "Trivial unexpected object/monitor header usage.");
    }
  }
  // We finally get the hash
  return hash;
}

// Deprecated -- use FastHashCode() instead.

intptr_t ObjectSynchronizer::identity_hash_value_for(Handle obj) {
  return FastHashCode (Thread::current(), obj()) ;
}


bool ObjectSynchronizer::current_thread_holds_lock(JavaThread* thread,
                                                   Handle h_obj) {
  if (UseBiasedLocking) {
    BiasedLocking::revoke_and_rebias(h_obj, false, thread);
    assert(!h_obj->mark()->has_bias_pattern(), "biases should be revoked by now");
  }

  assert(thread == JavaThread::current(), "Can only be called on current thread");
  oop obj = h_obj();

  markOop mark = ReadStableMark (obj) ;

  // Uncontended case, header points to stack
  if (mark->has_locker()) {
    return thread->is_lock_owned((address)mark->locker());
  }
  // Contended case, header points to ObjectMonitor (tagged pointer)
  if (mark->has_monitor()) {
    ObjectMonitor* monitor = mark->monitor();
    return monitor->is_entered(thread) != 0 ;
  }
  // Unlocked case, header in place
  assert(mark->is_neutral(), "sanity check");
  return false;
}

// Be aware of this method could revoke bias of the lock object.
// This method querys the ownership of the lock handle specified by 'h_obj'.
// If the current thread owns the lock, it returns owner_self. If no
// thread owns the lock, it returns owner_none. Otherwise, it will return
// ower_other.
ObjectSynchronizer::LockOwnership ObjectSynchronizer::query_lock_ownership
(JavaThread *self, Handle h_obj) {
  // The caller must beware this method can revoke bias, and
  // revocation can result in a safepoint.
  assert (!SafepointSynchronize::is_at_safepoint(), "invariant") ;
  assert (self->thread_state() != _thread_blocked , "invariant") ;

  // Possible mark states: neutral, biased, stack-locked, inflated

  if (UseBiasedLocking && h_obj()->mark()->has_bias_pattern()) {
    // CASE: biased
    BiasedLocking::revoke_and_rebias(h_obj, false, self);
    assert(!h_obj->mark()->has_bias_pattern(),
           "biases should be revoked by now");
  }

  assert(self == JavaThread::current(), "Can only be called on current thread");
  oop obj = h_obj();
  markOop mark = ReadStableMark (obj) ;

  // CASE: stack-locked.  Mark points to a BasicLock on the owner's stack.
  if (mark->has_locker()) {
    return self->is_lock_owned((address)mark->locker()) ?
      owner_self : owner_other;
  }

  // CASE: inflated. Mark (tagged pointer) points to an objectMonitor.
  // The Object:ObjectMonitor relationship is stable as long as we're
  // not at a safepoint.
  if (mark->has_monitor()) {
    void * owner = mark->monitor()->_owner ;
    if (owner == NULL) return owner_none ;
    return (owner == self ||
            self->is_lock_owned((address)owner)) ? owner_self : owner_other;
  }

  // CASE: neutral
  assert(mark->is_neutral(), "sanity check");
  return owner_none ;           // it's unlocked
}

// FIXME: jvmti should call this
JavaThread* ObjectSynchronizer::get_lock_owner(Handle h_obj, bool doLock) {
  if (UseBiasedLocking) {
    if (SafepointSynchronize::is_at_safepoint()) {
      BiasedLocking::revoke_at_safepoint(h_obj);
    } else {
      BiasedLocking::revoke_and_rebias(h_obj, false, JavaThread::current());
    }
    assert(!h_obj->mark()->has_bias_pattern(), "biases should be revoked by now");
  }

  oop obj = h_obj();
  address owner = NULL;

  markOop mark = ReadStableMark (obj) ;

  // Uncontended case, header points to stack
  if (mark->has_locker()) {
    owner = (address) mark->locker();
  }

  // Contended case, header points to ObjectMonitor (tagged pointer)
  if (mark->has_monitor()) {
    ObjectMonitor* monitor = mark->monitor();
    assert(monitor != NULL, "monitor should be non-null");
    owner = (address) monitor->owner();
  }

  if (owner != NULL) {
    // owning_thread_from_monitor_owner() may also return NULL here
    return Threads::owning_thread_from_monitor_owner(owner, doLock);
  }

  // Unlocked case, header in place
  // Cannot have assertion since this object may have been
  // locked by another thread when reaching here.
  // assert(mark->is_neutral(), "sanity check");

  return NULL;
}
// Visitors ...

void ObjectSynchronizer::monitors_iterate(MonitorClosure* closure) {
  ObjectMonitor* block =
    (ObjectMonitor*)OrderAccess::load_ptr_acquire(&gBlockList);
  while (block != NULL) {
    assert(block->object() == CHAINMARKER, "must be a block header");
    for (int i = _BLOCKSIZE - 1; i > 0; i--) {
      ObjectMonitor* mid = (ObjectMonitor *)(block + i);
      oop object = (oop)mid->object();
      if (object != NULL) {
        closure->do_monitor(mid);
      }
    }
    block = (ObjectMonitor*)block->FreeNext;
  }
}

// Get the next block in the block list.
static inline ObjectMonitor* next(ObjectMonitor* block) {
  assert(block->object() == CHAINMARKER, "must be a block header");
  block = block->FreeNext ;
  assert(block == NULL || block->object() == CHAINMARKER, "must be a block header");
  return block;
}


void ObjectSynchronizer::oops_do(OopClosure* f) {
  assert(SafepointSynchronize::is_at_safepoint(), "must be at safepoint");
  ObjectMonitor* block =
    (ObjectMonitor*)OrderAccess::load_ptr_acquire(&gBlockList);
  for (; block != NULL; block = (ObjectMonitor *)next(block)) {
    assert(block->object() == CHAINMARKER, "must be a block header");
    for (int i = 1; i < _BLOCKSIZE; i++) {
      ObjectMonitor* mid = &block[i];
      if (mid->object() != NULL) {
        f->do_oop((oop*)mid->object_addr());
      }
    }
  }
}


// -----------------------------------------------------------------------------
// ObjectMonitor Lifecycle
// -----------------------
// Inflation unlinks monitors from the global gFreeList and
// associates them with objects.  Deflation -- which occurs at
// STW-time -- disassociates idle monitors from objects.  Such
// scavenged monitors are returned to the gFreeList.
//
// The global list is protected by ListLock.  All the critical sections
// are short and operate in constant-time.
//
// ObjectMonitors reside in type-stable memory (TSM) and are immortal.
//
// Lifecycle:
// --   unassigned and on the global free list
// --   unassigned and on a thread's private omFreeList
// --   assigned to an object.  The object is inflated and the mark refers
//      to the objectmonitor.
//


// Constraining monitor pool growth via MonitorBound ...
//
// The monitor pool is grow-only.  We scavenge at STW safepoint-time, but the
// the rate of scavenging is driven primarily by GC.  As such,  we can find
// an inordinate number of monitors in circulation.
// To avoid that scenario we can artificially induce a STW safepoint
// if the pool appears to be growing past some reasonable bound.
// Generally we favor time in space-time tradeoffs, but as there's no
// natural back-pressure on the # of extant monitors we need to impose some
// type of limit.  Beware that if MonitorBound is set to too low a value
// we could just loop. In addition, if MonitorBound is set to a low value
// we'll incur more safepoints, which are harmful to performance.
// See also: GuaranteedSafepointInterval
//
// The current implementation uses asynchronous VM operations.
//

// 当设置了MonitorBound属性时，如果当前已使用的ObjectMonitor超过该属性时就会通过InduceScavenge强制JVM进入安全点，
// 进入安全点后会执行deflate_idle_monitors方法来清理掉已经从本地或者全局空闲链表中分配出去了但是不被使用的ObjectMonitor实例，
// 将其归还到全局的空闲链表中。该属性默认为0，即默认情况下只有在执行GC等需要在安全点下执行的操作时才会触发ObjectMonitor的回收，
// 除此之外ObjectMonitor的数量会一直增长。
//
static void InduceScavenge (Thread * Self, const char * Whence) {
  // Induce STW safepoint to trim monitors
  // Ultimately, this results in a call to deflate_idle_monitors() in the near future.
  // More precisely, trigger an asynchronous STW safepoint as the number
  // of active monitors passes the specified threshold.
  // TODO: assert thread state is reasonable

    //ForceMonitorScavenge是一个static volatile变量，初始为0，用来控制InduceScavenge不会重复执行
  if (ForceMonitorScavenge == 0 && Atomic::xchg (1, &ForceMonitorScavenge) == 0) {
    if (ObjectMonitor::Knob_Verbose) {
      ::printf ("Monitor scavenge - Induced STW @%s (%d)\n", Whence, ForceMonitorScavenge) ;
      ::fflush(stdout) ;
    }
    // Induce a 'null' safepoint to scavenge monitors
    // Must VM_Operation instance be heap allocated as the op will be enqueue and posted
    // to the VMthread and have a lifespan longer than that of this activation record.
    // The VMThread will delete the op when completed.
      //执行VM_ForceAsyncSafepoint
    VMThread::execute (new VM_ForceAsyncSafepoint()) ;

    if (ObjectMonitor::Knob_Verbose) {
      ::printf ("Monitor scavenge - STW posted @%s (%d)\n", Whence, ForceMonitorScavenge) ;
      ::fflush(stdout) ;
    }
  }
}
/* Too slow for general assert or debug
void ObjectSynchronizer::verifyInUse (Thread *Self) {
   ObjectMonitor* mid;
   int inusetally = 0;
   for (mid = Self->omInUseList; mid != NULL; mid = mid->FreeNext) {
     inusetally ++;
   }
   assert(inusetally == Self->omInUseCount, "inuse count off");

   int freetally = 0;
   for (mid = Self->omFreeList; mid != NULL; mid = mid->FreeNext) {
     freetally ++;
   }
   assert(freetally == Self->omFreeCount, "free count off");
}
*/
/**
 * omAlloc方法用于分配ObjectMonitor，具体分配时首先从当前线程保存的本地ObjectMonitor空闲列表中分配，如果本地ObjectMonitor空闲列表为空，
 * 则尝试从全局的ObjectMonitor空闲列表中分配最多32个ObjectMonitor实例将其存入线程本地的ObjectMonitor空闲链表中，然后从中分配ObjectMonitor；
 * 如果本地ObjectMonitor空闲列表和全局的ObjectMonitor空闲列表都为空，则一次创建128个ObjectMonitor实例，将其加入到全局的ObjectMonitor空闲列表中。
 * 使用本地的ObjectMonitor空闲列表可以减少从全局的ObjectMonitor空闲列表中分配时的锁竞争问题和一致性问题，提高分配效率，但是会增加GC时根节点扫描的开销。
 */
ObjectMonitor * ATTR ObjectSynchronizer::omAlloc (Thread * Self) {
    // A large MAXPRIVATE value reduces both list lock contention
    // and list coherency traffic, but also tends to increase the
    // number of objectMonitors in circulation as well as the STW
    // scavenge costs.  As usual, we lean toward time in space-time
    // tradeoffs.
    const int MAXPRIVATE = 1024 ;
    for (;;) {
        ObjectMonitor * m ;

        // 1: try to allocate from the thread's local omFreeList.
        // Threads will attempt to allocate first from their local list, then
        // from the global list, and only after those attempts fail will the thread
        // attempt to instantiate new monitors.   Thread-local free lists take
        // heat off the ListLock and improve allocation latency, as well as reducing
        // coherency traffic on the shared global list.
        m = Self->omFreeList ;
        if (m != NULL) {
            //如果线程本地的空闲链表非空，从空闲链表中分配一个
           Self->omFreeList = m->FreeNext ;
           Self->omFreeCount -- ;
           // CONSIDER: set m->FreeNext = BAD -- diagnostic hygiene
           guarantee (m->object() == NULL, "invariant") ;
            //MonitorInUseLists表示是否记录使用中的ObjectMonitor，默认为false
           if (MonitorInUseLists) {
               //如果为true则将其加入到omInUseList中
             m->FreeNext = Self->omInUseList;
             Self->omInUseList = m;
             Self->omInUseCount ++;
             // verifyInUse(Self);
           } else {
               //如果为false，将FreeNext置为NULL
             m->FreeNext = NULL;
           }
           return m ;
        }

        // 2: try to allocate from the global gFreeList
        // CONSIDER: use muxTry() instead of muxAcquire().
        // If the muxTry() fails then drop immediately into case 3.
        // If we're using thread-local free lists then try
        // to reprovision the caller's free list.
        //线程本地的空闲链表为空，尝试从全局的空闲链表中分配
        if (gFreeList != NULL) {
            // Reprovision the thread's omFreeList.
            // Use bulk transfers to reduce the allocation rate and heat
            // on various locks.
            //获取锁ListLock
            Thread::muxAcquire (&ListLock, "omAlloc") ;
            for (int i = Self->omFreeProvision; --i >= 0 && gFreeList != NULL; ) {
                //从全局链表中取出一个
                MonitorFreeCount --;
                ObjectMonitor * take = gFreeList ;
                gFreeList = take->FreeNext ;
                guarantee (take->object() == NULL, "invariant") ;
                guarantee (!take->is_busy(), "invariant") ;
                //将取出的ObjectMonitor重置
                take->Recycle() ;
                //将ObjectMonitor加入到线程本地的空闲链表中
                omRelease (Self, take, false) ;
            }
            //释放锁
            Thread::muxRelease (&ListLock) ;
            //将omFreeProvision增加一半，如果超过MAXPRIVATE则置为MAXPRIVATE
            Self->omFreeProvision += 1 + (Self->omFreeProvision/2) ;
            if (Self->omFreeProvision > MAXPRIVATE ) Self->omFreeProvision = MAXPRIVATE ;
            TEVENT (omFirst - reprovision) ;

            //MonitorBound默认为0，表示允许使用的ObjectMonitor的最大个数
            const int mx = MonitorBound ;
            if (mx > 0 && (MonitorPopulation-MonitorFreeCount) > mx) {
              // We can't safely induce a STW safepoint from omAlloc() as our thread
              // state may not be appropriate for such activities and callers may hold
              // naked oops, so instead we defer the action.
                //通过VM_Operation强制JVM进入安全点并调用deflate_idle_monitors方法回收部分空闲的ObjectMonitor
              InduceScavenge (Self, "omAlloc") ;
            }
            //从全局的空闲链表中分配指定数量的空闲ObjectMonitor后就开始下一次循环，在本地空闲链表中分配
            continue;
        }

        // 3: allocate a block of new ObjectMonitors
        // Both the local and global free lists are empty -- resort to malloc().
        // In the current implementation objectMonitors are TSM - immortal.
        //本地空闲链表和全局的空闲链表都是空的，需要重新分配
        //_BLOCKSIZE是一个枚举值，默认128
        assert (_BLOCKSIZE > 1, "invariant") ;
        //创建一个ObjectMonitor数组，即一次创建128个ObjectMonitor
        ObjectMonitor * temp = new ObjectMonitor[_BLOCKSIZE];

        // NOTE: (almost) no way to recover if allocation failed.
        // We might be able to induce a STW safepoint and scavenge enough
        // objectMonitors to permit progress.
        if (temp == NULL) {
            //分配失败，抛出异常
            vm_exit_out_of_memory (sizeof (ObjectMonitor[_BLOCKSIZE]), OOM_MALLOC_ERROR,
                                   "Allocate ObjectMonitors");
        }

        // Format the block.
        // initialize the linked list, each monitor points to its next
        // forming the single linked free list, the very first monitor
        // will points to next block, which forms the block list.
        // The trick of using the 1st element in the block as gBlockList
        // linkage should be reconsidered.  A better implementation would
        // look like: class Block { Block * next; int N; ObjectMonitor Body [N] ; }

        //将数组中的ObjectMonitor构成链表
        for (int i = 1; i < _BLOCKSIZE ; i++) {
           temp[i].FreeNext = &temp[i+1];
        }

        // terminate the last monitor as the end of list
        //数组中最后一个ObjectMonitor的FreeNext置为NULL
        temp[_BLOCKSIZE - 1].FreeNext = NULL ;

        // Element [0] is reserved for global list linkage
        //第一个元素标记为正在构建链表中
        temp[0].set_object(CHAINMARKER);

        // Consider carving out this thread's current request from the
        // block in hand.  This avoids some lock traffic and redundant
        // list activity.

        // Acquire the ListLock to manipulate BlockList and FreeList.
        // An Oyama-Taura-Yonezawa scheme might be more efficient.
        //获取锁
        Thread::muxAcquire (&ListLock, "omAlloc [2]") ;
        //增加计数
        MonitorPopulation += _BLOCKSIZE-1;
        MonitorFreeCount += _BLOCKSIZE-1;

        // Add the new block to the list of extant blocks (gBlockList).
        // The very first objectMonitor in a block is reserved and dedicated.
        // It serves as blocklist "next" linkage.
        //将第一个数组元素加入到gBlockList中，gBlockList用于遍历所有的ObjectMonitor实例，无论其是空闲还是正在使用
        temp[0].FreeNext = gBlockList;
        // There are lock-free uses of gBlockList so make sure that
        // the previous stores happen before we update gBlockList.
        OrderAccess::release_store_ptr(&gBlockList, temp);

        // Add the new string of objectMonitors to the global free list
        //将第二个元素加入gFreeList中
        temp[_BLOCKSIZE - 1].FreeNext = gFreeList ;
        gFreeList = temp + 1;
        //释放锁
        Thread::muxRelease (&ListLock) ;
        TEVENT (Allocate block of monitors) ;
    }
}

// Place "m" on the caller's private per-thread omFreeList.
// In practice there's no need to clamp or limit the number of
// monitors on a thread's omFreeList as the only time we'll call
// omRelease is to return a monitor to the free list after a CAS
// attempt failed.  This doesn't allow unbounded #s of monitors to
// accumulate on a thread's free list.
//

/**
 * omRelease用于将某个ObjectMonitor归还到本地的ObjectMonitor空闲链表中，如果MonitorInUseLists为true还需要将其从全局的正在使用的ObjectMonitor链表中移除。
 */
void ObjectSynchronizer::omRelease (Thread * Self, ObjectMonitor * m, bool fromPerThreadAlloc) {
    guarantee (m->object() == NULL, "invariant") ;

    // Remove from omInUseList
    //MonitorInUseLists默认为false
    if (MonitorInUseLists && fromPerThreadAlloc) {
      ObjectMonitor* curmidinuse = NULL;
        //遍历omInUseList，如果存在目标m则将其移除
      for (ObjectMonitor* mid = Self->omInUseList; mid != NULL; ) {
       if (m == mid) {
         // extract from per-thread in-use-list
         if (mid == Self->omInUseList) {
             //如果位于链表头
           Self->omInUseList = mid->FreeNext;
         } else if (curmidinuse != NULL) {
             //如果位于链表中间
           curmidinuse->FreeNext = mid->FreeNext; // maintain the current thread inuselist
         }
         Self->omInUseCount --;
         // verifyInUse(Self);
         break;
       } else {
           //curmidinuse表示当前mid的上一个ObjectMonitor
         curmidinuse = mid;
         mid = mid->FreeNext;
      }
    }
  }

  // FreeNext is used for both onInUseList and omFreeList, so clear old before setting new
    //将m加入到当前线程的空闲链表中
  m->FreeNext = Self->omFreeList ;
  Self->omFreeList = m ;
  Self->omFreeCount ++ ;
}

// Return the monitors of a moribund thread's local free list to
// the global free list.  Typically a thread calls omFlush() when
// it's dying.  We could also consider having the VM thread steal
// monitors from threads that have not run java code over a few
// consecutive STW safepoints.  Relatedly, we might decay
// omFreeProvision at STW safepoints.
//
// Also return the monitors of a moribund thread"s omInUseList to
// a global gOmInUseList under the global list lock so these
// will continue to be scanned.
//
// We currently call omFlush() from the Thread:: dtor _after the thread
// has been excised from the thread list and is no longer a mutator.
// That means that omFlush() can run concurrently with a safepoint and
// the scavenge operator.  Calling omFlush() from JavaThread::exit() might
// be a better choice as we could safely reason that that the JVM is
// not at a safepoint at the time of the call, and thus there could
// be not inopportune interleavings between omFlush() and the scavenge
// operator.

/**
 * omFlush是在线程退出时调用，将该线程的本地ObjectMonitor空闲链表中的元素归还到全局空闲链表中，将本地正在使用的ObjectMonitor链表中的元素归还到全局正在使用的链表，
 * 为啥这是不能将本地正在使用的ObjectMonitor链表中的元素归还到全局空闲链表中了？因为其中的某个ObjectMonitor实例也可能被其他线程所使用
 */
void ObjectSynchronizer::omFlush (Thread * Self) {
    ObjectMonitor * List = Self->omFreeList ;  // Null-terminated SLL
    Self->omFreeList = NULL ;
    ObjectMonitor * Tail = NULL ;
    int Tally = 0;
    if (List != NULL) {
      ObjectMonitor * s ;
        //遍历本地空闲链表中的ObjectMonitor，找到最后一个元素
      for (s = List ; s != NULL ; s = s->FreeNext) {
          Tally ++ ;
          Tail = s ;
          guarantee (s->object() == NULL, "invariant") ;
          guarantee (!s->is_busy(), "invariant") ;
          s->set_owner (NULL) ;   // redundant but good hygiene
          TEVENT (omFlush - Move one) ;
      }
      guarantee (Tail != NULL && List != NULL, "invariant") ;
    }

    ObjectMonitor * InUseList = Self->omInUseList;
    ObjectMonitor * InUseTail = NULL ;
    int InUseTally = 0;
    if (InUseList != NULL) {
      Self->omInUseList = NULL;
      ObjectMonitor *curom;
        //遍历本地正在使用中的ObjectMonitor链表，找到最后一个
      for (curom = InUseList; curom != NULL; curom = curom->FreeNext) {
        InUseTail = curom;
        InUseTally++;
      }
// TODO debug
      assert(Self->omInUseCount == InUseTally, "inuse count off");
      Self->omInUseCount = 0;
      guarantee (InUseTail != NULL && InUseList != NULL, "invariant");
    }

    //获取锁
    Thread::muxAcquire (&ListLock, "omFlush") ;
    if (Tail != NULL) {
        //将本地空闲ObjectMonitor链表归还到全局空闲链表中
      Tail->FreeNext = gFreeList ;
      gFreeList = List ;
      MonitorFreeCount += Tally;
    }

    if (InUseTail != NULL) {
        //将本地正在使用中的ObjectMonitor链表归还到全局正在使用链表
      InUseTail->FreeNext = gOmInUseList;
      gOmInUseList = InUseList;
      gOmInUseCount += InUseTally;
    }

    //释放锁
    Thread::muxRelease (&ListLock) ;
    TEVENT (omFlush) ;
}

const char* ObjectSynchronizer::inflate_cause_name(const InflateCause cause) {
  switch (cause) {
    case inflate_cause_vm_internal:    return "VM Internal";
    case inflate_cause_monitor_enter:  return "Monitor Enter";
    case inflate_cause_wait:           return "Monitor Wait";
    case inflate_cause_notify:         return "Monitor Notify";
    case inflate_cause_hash_code:      return "Monitor Hash Code";
    case inflate_cause_jni_enter:      return "JNI Monitor Enter";
    case inflate_cause_jni_exit:       return "JNI Monitor Exit";
    default:
      ShouldNotReachHere();
  }
  return "Unknown";
}

static void post_monitor_inflate_event(EventJavaMonitorInflate* event,
                                       const oop obj,
                                       const ObjectSynchronizer::InflateCause cause) {
  assert(event != NULL, "invariant");
  assert(event->should_commit(), "invariant");
  event->set_monitorClass(obj->klass());
  event->set_address((uintptr_t)(void*)obj);
  event->set_cause((u1)cause);
  event->commit();
}

// Fast path code shared by multiple functions
// inflate_helper是给JVM内部其他类使用的，也是基于inflate方法实现的
ObjectMonitor* ObjectSynchronizer::inflate_helper(oop obj) {
  markOop mark = obj->mark();
  if (mark->has_monitor()) {
      //如果该对象本身就是持有重量级锁则直接返回对象头中包含的ObjectMonitor指针
    assert(ObjectSynchronizer::verify_objmon_isinpool(mark->monitor()), "monitor is invalid");
    assert(mark->monitor()->header()->is_neutral(), "monitor must record a good object header");
    return mark->monitor();
  }
    //如果没有持有监视器锁，则创建一个并保存在对象头中
  return ObjectSynchronizer::inflate(Thread::current(),
                                     obj,
                                     inflate_cause_vm_internal);
}


// Note that we could encounter some performance loss through false-sharing as
// multiple locks occupy the same $ line.  Padding might be appropriate.


// inflate方法用于将某个对象持有的轻量级锁膨胀成重量级锁即监视器锁，inflate_helper是给JVM内部其他类使用的，也是基于inflate方法实现的
// 该方法会根据对象的锁状态来做不同的处理，如果已经持有监视器锁则直接返回对象头中包含的ObjectMonitor指针；
// 如果处于锁膨胀的过程中，则通过自旋、yeild或者park的方式等待锁膨胀完成；如果对象持有轻量级锁或者无锁，
// 则从线程本地的空闲ObjectMonitor链表或者全局的空闲ObjectMonitor链表中分配一个ObjectMonitor，将其重置并保存当前对象的对象头等属性，
// 最后原子的修改对象的对象头，如果有其他线程也在尝试修改同一对象的对象头导致修改失败则将之前分配的ObjectMonitor归还到线程本地的空闲ObjectMonitor链表中，
// 开始下一次循环，如果修改成功则返回ObjectMonitor实例。
//
//
// 重量级锁是通过对象内部的监视器(monitor)来实现，而monitor的本质是依赖操作系统底层的MutexLock实现的。
// 先来看锁的膨胀过程，从前面的分析中已经知道了所膨胀的过程是通过 ObjectSynchronizer::inflate方法实现的
// 锁膨胀的过程实际上是获得一个ObjectMonitor对象监视器，而真正抢占锁的逻辑，在 ObjectMonitor::enter方法里面。
//
// 锁膨胀的过程稍微有点复杂，整个锁膨胀的过程是通过自旋来完成的，具体的实现逻辑简答总结以下几点：
//  1. mark->has_monitor() 判断如果当前锁对象为重量级锁，也就是lock:10，则执行第二步骤,否则执行第三步骤。
//  2. 通过 mark->monitor获得重量级锁的对象监视器ObjectMonitor并返回，锁膨胀过程结束。
//  3. 如果当前锁处于 INFLATING,说明有其他线程在执行锁膨胀，那么当前线程通过自旋等待其他线程锁膨胀完成。
//  4. 如果当前是轻量级锁状态 mark->has_locker(),则进行锁膨胀。首先，通过omAlloc方法获得一个可用的ObjectMonitor，并设置初始数据；
//      然后通过CAS将对象头设置为`markOopDesc:INFLATING，表示当前锁正在膨胀，如果CAS失败，继续自旋。
//  5. 如果是无锁状态，逻辑类似第四步骤。
//
ObjectMonitor * ATTR ObjectSynchronizer::inflate (Thread * Self,
                                                  oop object,
                                                  const InflateCause cause) {
  slog_debug("进入hotspot/src/share/vm/runtime/synchronizer.cpp中的ObjectSynchronizer::inflate函数...");
  // Inflate mutates the heap ...
  // Relaxing assertion for bug 6320749.
  assert (Universe::verify_in_progress() ||
          !SafepointSynchronize::is_at_safepoint(), "invariant") ;

  EventJavaMonitorInflate event;

    //通过无意义的循环实现自旋操作
  for (;;) {
      const markOop mark = object->mark() ;
      assert (!mark->has_bias_pattern(), "invariant") ;

      // The mark can be in one of the following states:
      // *  Inflated     - just return      已经持有监视器锁
      // *  Stack-locked - coerce it to inflated    持有轻量级锁，准备执行锁膨胀
      // *  INFLATING    - busy wait for conversion to complete     正在执行锁膨胀过程
      // *  Neutral      - aggressively inflate the object.     未持有锁
      // *  BIASED       - Illegal.  We should never see this   持有偏向锁，不可能出现此情形，因为偏向锁只有膨胀成轻量级锁才会膨胀成重量级锁

      // CASE: inflated
      //has_monitor是markOop.hpp中的方法，如果为true表示当前锁已经是重量级锁了
      if (mark->has_monitor()) {
          slog_debug("当前锁标记位(mark word最后2位)为10,即重量级锁...");
          //如果已经持有监视器锁，则返回对象头中保存的ObjectMonitor指针
          //获得重量级锁的对象监视器直接返回
          ObjectMonitor * inf = mark->monitor() ;
          assert (inf->header()->is_neutral(), "invariant");
          assert (inf->object() == object, "invariant") ;
          assert (ObjectSynchronizer::verify_objmon_isinpool(inf), "monitor is invalid");
          return inf ;
      }

      // CASE: inflation in progress - inflating over a stack-lock.
      // Some other thread is converting from stack-locked to inflated.
      // Only that thread can complete inflation -- other threads must wait.
      // The INFLATING value is transient.
      // Currently, we spin/yield/park and poll the markword, waiting for inflation to finish.
      // We could always eliminate polling by parking the thread on some auxiliary list.
      //膨胀等待，表示存在线程正在膨胀，通过continue进行下一轮的膨胀
      if (mark == markOopDesc::INFLATING()) {
         slog_debug("当前正处于升级为重量级锁的过程中...");
         TEVENT (Inflate: spin while INFLATING) ;
          //其他某个线程正在将该对象的锁膨胀成监视器锁，在此等待膨胀完成，会先自旋，超过1000次后yeild，yield 一定次数后执行park
         ReadStableMark(object) ;
         continue ;
      }

      // CASE: stack-locked
      // Could be stack-locked either by this thread or by some other thread.
      //
      // Note that we allocate the objectmonitor speculatively, _before_ attempting
      // to install INFLATING into the mark word.  We originally installed INFLATING,
      // allocated the objectmonitor, and then finally STed the address of the
      // objectmonitor into the mark.  This was correct, but artificially lengthened
      // the interval in which INFLATED appeared in the mark, thus increasing
      // the odds of inflation contention.
      //
      // We now use per-thread private objectmonitor free lists.
      // These list are reprovisioned from the global free list outside the
      // critical INFLATING...ST interval.  A thread can transfer
      // multiple objectmonitors en-mass from the global free list to its local free list.
      // This reduces coherency traffic and lock contention on the global free list.
      // Using such local free lists, it doesn't matter if the omAlloc() call appears
      // before or after the CAS(INFLATING) operation.
      // See the comments in omAlloc().

      //表示当前锁为轻量级锁，以下是轻量级锁的膨胀逻辑
      if (mark->has_locker()) {
          slog_debug("当前锁标记位(mark word最后2位)为00,即轻量级锁,进行轻量级锁的膨胀...");
          //获取一个可用的ObjectMonitor
          //从当前线程的本地空闲链表或者全局空闲链表中分配一个空闲的ObjectMonitor
          ObjectMonitor * m = omAlloc (Self) ;
          // Optimistically prepare the objectmonitor - anticipate successful CAS
          // We do this before the CAS in order to minimize the length of time
          // in which INFLATING appears in the mark.
          // 将其重置
          m->Recycle();
          m->_Responsible  = NULL ;
          m->OwnerIsThread = 0 ;
          m->_recursions   = 0 ;
          m->_SpinDuration = ObjectMonitor::Knob_SpinLimit ;   // Consider: maintain by type/class

          //将其对象头原子的修改为INFLATING
          // 将object->mark_addr()和mark比较，如果这两个值相等，则将object->mark_addr()
          //    改成markOopDesc::INFLATING()，相等返回是mark，不相等返回的是object->mark_addr()
          slog_debug("使用CAS操作尝试将对象头修改为INFLATING...");
          markOop cmp = (markOop) Atomic::cmpxchg_ptr (markOopDesc::INFLATING(), object->mark_addr(), mark) ;
          //CAS失败
          //如果修改失败则将m归还到本地线程的空闲链表，修改失败说明其他某个线程也在尝试执行锁膨胀
          if (cmp != mark) {
              //释放监视器
             omRelease (Self, m, true) ;
              // 重试
              //开始下一次循环
             slog_debug("CAS修改失败,继续下一次循环...");
             continue ;       // Interference -- just retry
          }

          // We've successfully installed INFLATING (0) into the mark-word.
          // This is the only case where 0 will appear in a mark-work.
          // Only the singular thread that successfully swings the mark-word
          // to 0 can perform (or more precisely, complete) inflation.
          //
          // Why do we CAS a 0 into the mark-word instead of just CASing the
          // mark-word from the stack-locked value directly to the new inflated state?
          // Consider what happens when a thread unlocks a stack-locked object.
          // It attempts to use CAS to swing the displaced header value from the
          // on-stack basiclock back into the object header.  Recall also that the
          // header value (hashcode, etc) can reside in (a) the object header, or
          // (b) a displaced header associated with the stack-lock, or (c) a displaced
          // header in an objectMonitor.  The inflate() routine must copy the header
          // value from the basiclock on the owner's stack to the objectMonitor, all
          // the while preserving the hashCode stability invariants.  If the owner
          // decides to release the lock while the value is 0, the unlock will fail
          // and control will eventually pass from slow_exit() to inflate.  The owner
          // will then spin, waiting for the 0 value to disappear.   Put another way,
          // the 0 causes the owner to stall if the owner happens to try to
          // drop the lock (restoring the header from the basiclock to the object)
          // while inflation is in-progress.  This protocol avoids races that might
          // would otherwise permit hashCode values to change or "flicker" for an object.
          // Critically, while object->mark is 0 mark->displaced_mark_helper() is stable.
          // 0 serves as a "BUSY" inflate-in-progress indicator.


          // fetch the displaced mark from the owner's stack.
          // The owner can't die or unwind past the lock while our INFLATING
          // object is in the mark.  Furthermore the owner can't complete
          // an unlock on the object, either.
          //根据对象头中的指针获取保存在当前线程栈帧中的对象原来的对象头
          markOop dmw = mark->displaced_mark_helper() ;
          assert (dmw->is_neutral(), "invariant") ;

          // Setup monitor fields to proper values -- prepare the monitor
          //CAS成功以后，设置ObjectMonitor相关属性
          //将原来的对象头保存
          m->set_header(dmw) ;

          // Optimization: if the mark->locker stack address is associated
          // with this thread we could simply set m->_owner = Self and
          // m->OwnerIsThread = 1. Note that a thread can inflate an object
          // that it has stack-locked -- as might happen in wait() -- directly
          // with CAS.  That is, we can avoid the xchg-NULL .... ST idiom.
          m->set_owner(mark->locker());
          m->set_object(object);
          // TODO-FIXME: assert BasicLock->dhw != 0.

          // Must preserve store ordering. The monitor state must
          // be stable at the time of publishing the monitor address.
          guarantee (object->mark() == markOopDesc::INFLATING(), "invariant") ;
          //重置对象头
          object->release_set_mark(markOopDesc::encode(m));

          // Hopefully the performance counters are allocated on distinct cache lines
          // to avoid false sharing on MP systems ...
          //增加计数
          if (ObjectMonitor::_sync_Inflations != NULL) {
              ObjectMonitor::_sync_Inflations->inc() ;
          }
          TEVENT(Inflate: overwrite stacklock) ;
          if (TraceMonitorInflation) {
            if (object->is_instance()) {
              ResourceMark rm;
              tty->print_cr("Inflating object " INTPTR_FORMAT " , mark " INTPTR_FORMAT " , type %s",
                (void *) object, (intptr_t) object->mark(),
                object->klass()->external_name());
            }
          }
          if (event.should_commit()) {
            post_monitor_inflate_event(&event, object, cause);
          }
          //返回ObjectMonitor
          return m ;
      }

      // CASE: neutral
      // TODO-FIXME: for entry we currently inflate and then try to CAS _owner.
      // If we know we're inflating for entry it's better to inflate by swinging a
      // pre-locked objectMonitor pointer into the object header.   A successful
      // CAS inflates the object *and* confers ownership to the inflating thread.
      // In the current implementation we use a 2-step mechanism where we CAS()
      // to inflate and then CAS() again to try to swing _owner from NULL to Self.
      // An inflateTry() method that we could call from fast_enter() and slow_enter()
      // would be useful.

      slog_debug("当前锁标记位(mark word最后3位)为001,即无锁状态...");
      //如果是无锁状态
      assert (mark->is_neutral(), "invariant");
      //获取一个可用的ObjectMonitor
      //分配一个新的ObjectMonitor
      ObjectMonitor * m = omAlloc (Self) ;
      // prepare m for installation - set monitor to initial state
      //设置ObjectMonitor相关属性
      //将其初始化
      m->Recycle();
      //保存原来的对象头，设置相关属性
      m->set_header(mark);
      m->set_owner(NULL);
      m->set_object(object);
      m->OwnerIsThread = 1 ;
      m->_recursions   = 0 ;
      m->_Responsible  = NULL ;
      m->_SpinDuration = ObjectMonitor::Knob_SpinLimit ;       // consider: keep metastats by type/class

      // 将object->mark_addr()和mark比较，如果这两个值相等，则将object->mark_addr()
      //          改成markOopDesc::encode(m)，相等返回是mark，不相等返回的是object->mark_addr()
      if (Atomic::cmpxchg_ptr (markOopDesc::encode(m), object->mark_addr(), mark) != mark) {
          //CAS失败，说明出现了锁竞争，则释放监视器重新竞争锁
          //原子的修改对象头，如果修改失败将m恢复成初始状态
          m->set_object (NULL) ;
          m->set_owner  (NULL) ;
          m->OwnerIsThread = 0 ;
          m->Recycle() ;
          //归还到本地的空闲链表中
          omRelease (Self, m, true) ;
          m = NULL ;
          //开始下一次循环
          continue ;
          // interference - the markword changed - just retry.
          // The state-transitions are one-way, so there's no chance of
          // live-lock -- "Inflated" is an absorbing state.
      }

      // Hopefully the performance counters are allocated on distinct
      // cache lines to avoid false sharing on MP systems ...
      //对象头修改成功，增加计数
      if (ObjectMonitor::_sync_Inflations != NULL) {
          ObjectMonitor::_sync_Inflations->inc() ;
      }
      TEVENT(Inflate: overwrite neutral) ;
      if (TraceMonitorInflation) {
        if (object->is_instance()) {
          ResourceMark rm;
          tty->print_cr("Inflating object " INTPTR_FORMAT " , mark " INTPTR_FORMAT " , type %s",
            (void *) object, (intptr_t) object->mark(),
            object->klass()->external_name());
        }
      }
      if (event.should_commit()) {
        post_monitor_inflate_event(&event, object, cause);
      }
      //返回ObjectMonitor对象
      return m ;
  }
}

// Note that we could encounter some performance loss through false-sharing as
// multiple locks occupy the same $ line.  Padding might be appropriate.


// Deflate_idle_monitors() is called at all safepoints, immediately
// after all mutators are stopped, but before any objects have moved.
// It traverses the list of known monitors, deflating where possible.
// The scavenged monitor are returned to the monitor free list.
//
// Beware that we scavenge at *every* stop-the-world point.
// Having a large number of monitors in-circulation negatively
// impacts the performance of some applications (e.g., PointBase).
// Broadly, we want to minimize the # of monitors in circulation.
//
// We have added a flag, MonitorInUseLists, which creates a list
// of active monitors for each thread. deflate_idle_monitors()
// only scans the per-thread inuse lists. omAlloc() puts all
// assigned monitors on the per-thread list. deflate_idle_monitors()
// returns the non-busy monitors to the global free list.
// When a thread dies, omFlush() adds the list of active monitors for
// that thread to a global gOmInUseList acquiring the
// global list lock. deflate_idle_monitors() acquires the global
// list lock to scan for non-busy monitors to the global free list.
// An alternative could have used a single global inuse list. The
// downside would have been the additional cost of acquiring the global list lock
// for every omAlloc().
//
// Perversely, the heap size -- and thus the STW safepoint rate --
// typically drives the scavenge rate.  Large heaps can mean infrequent GC,
// which in turn can mean large(r) numbers of objectmonitors in circulation.
// This is an unfortunate aspect of this design.
//

enum ManifestConstants {
    ClearResponsibleAtSTW   = 0,
    MaximumRecheckInterval  = 1000
} ;

// Deflate a single monitor if not in use
// Return true if deflated, false if in use
bool ObjectSynchronizer::deflate_monitor(ObjectMonitor* mid, oop obj,
                                         ObjectMonitor** FreeHeadp, ObjectMonitor** FreeTailp) {
  bool deflated;
  // Normal case ... The monitor is associated with obj.
  guarantee (obj->mark() == markOopDesc::encode(mid), "invariant") ;
  guarantee (mid == obj->mark()->monitor(), "invariant");
  guarantee (mid->header()->is_neutral(), "invariant");

  if (mid->is_busy()) {
      //如果ObjectMonitor正在使用中，返回false
     if (ClearResponsibleAtSTW) mid->_Responsible = NULL ;
     deflated = false;
  } else {
     // Deflate the monitor if it is no longer being used
     // It's idle - scavenge and return to the global free list
     // plain old deflation ...
      //如果ObjectMonitor未使用则回收，将其归还FreeTailp空闲链表中，返回true
     TEVENT (deflate_idle_monitors - scavenge1) ;
     if (TraceMonitorInflation) {
       if (obj->is_instance()) {
         ResourceMark rm;
           tty->print_cr("Deflating object " INTPTR_FORMAT " , mark " INTPTR_FORMAT " , type %s",
                (void *) obj, (intptr_t) obj->mark(), obj->klass()->external_name());
       }
     }

     // Restore the header back to obj
      //恢复原来的对象头
     obj->release_set_mark(mid->header());
      //重置
     mid->clear();

     assert (mid->object() == NULL, "invariant") ;

     // Move the object to the working free list defined by FreeHead,FreeTail.
      //将其插入到空闲链表中
     if (*FreeHeadp == NULL) *FreeHeadp = mid;
     if (*FreeTailp != NULL) {
       ObjectMonitor * prevtail = *FreeTailp;
       assert(prevtail->FreeNext == NULL, "cleaned up deflated?"); // TODO KK
       prevtail->FreeNext = mid;
      }
     *FreeTailp = mid;
     deflated = true;
  }
  return deflated;
}

// Caller acquires ListLock
int ObjectSynchronizer::walk_monitor_list(ObjectMonitor** listheadp,
                                          ObjectMonitor** FreeHeadp, ObjectMonitor** FreeTailp) {
  ObjectMonitor* mid;
  ObjectMonitor* next;
  ObjectMonitor* curmidinuse = NULL;
  int deflatedcount = 0;

  for (mid = *listheadp; mid != NULL; ) {
     oop obj = (oop) mid->object();
     bool deflated = false;
     if (obj != NULL) {
       deflated = deflate_monitor(mid, obj, FreeHeadp, FreeTailp);
     }
     if (deflated) {
       // extract from per-thread in-use-list
         //如果未使用被回收了，则将其从listheadp中移除
       if (mid == *listheadp) {
         *listheadp = mid->FreeNext;
       } else if (curmidinuse != NULL) {
         curmidinuse->FreeNext = mid->FreeNext; // maintain the current thread inuselist
       }
       next = mid->FreeNext;
       mid->FreeNext = NULL;  // This mid is current tail in the FreeHead list
       mid = next;
         //增加计数
       deflatedcount++;
     } else {
       curmidinuse = mid;
       mid = mid->FreeNext;
    }
  }
  return deflatedcount;
}

/**
 * 如果MonitorInUseLists为true，deflate_idle_monitors依赖walk_monitor_list遍历所有线程的本地omInUseList链表和全局的gOmInUseList，
 * 如果链表中的ObjectMonitor未被使用则将其归还到一个链表中；如果MonitorInUseLists为false，deflate_idle_monitors则依赖deflate_monitor，
 * 通过gBlockList遍历所有的已创建的ObjectMonitor实例，如果其Object为非空即已经被分配出去了但是未被使用则将其回收，归还到某个链表中；
 * walk_monitor_list处理链表中的ObjectMonitor实例时，也是调用deflate_monitor方法；最后将所有归还的ObjectMonitor实例构成的链表插入到全局空闲链表gFreeList的前面。
 */
void ObjectSynchronizer::deflate_idle_monitors() {
  assert(SafepointSynchronize::is_at_safepoint(), "must be at safepoint");
  int nInuse = 0 ;              // currently associated with objects
  int nInCirculation = 0 ;      // extant
  int nScavenged = 0 ;          // reclaimed
  bool deflated = false;

  ObjectMonitor * FreeHead = NULL ;  // Local SLL of scavenged monitors
  ObjectMonitor * FreeTail = NULL ;

  TEVENT (deflate_idle_monitors) ;
  // Prevent omFlush from changing mids in Thread dtor's during deflation
  // And in case the vm thread is acquiring a lock during a safepoint
  // See e.g. 6320749
    //获取锁
  Thread::muxAcquire (&ListLock, "scavenge - return") ;

    //MonitorInUseLists默认为false
  if (MonitorInUseLists) {
    int inUse = 0;
    for (JavaThread* cur = Threads::first(); cur != NULL; cur = cur->next()) {
      nInCirculation+= cur->omInUseCount;
        //遍历线程本地的omInUseList链表中的ObjectMonitor，如果其未使用则将其回收，返回该链表中被回收掉的ObjectMonitor个数
      int deflatedcount = walk_monitor_list(cur->omInUseList_addr(), &FreeHead, &FreeTail);
      cur->omInUseCount-= deflatedcount;
      // verifyInUse(cur);
        //nScavenged表示累计回收掉的ObjectMonitor数量
      nScavenged += deflatedcount;
        //nInuse表示累计的正在使用中的ObjectMonitor数量
      nInuse += cur->omInUseCount;
     }

   // For moribund threads, scan gOmInUseList
   if (gOmInUseList) {
     nInCirculation += gOmInUseCount;
       //遍历全局的gOmInUseList链表中的ObjectMonitor
     int deflatedcount = walk_monitor_list((ObjectMonitor **)&gOmInUseList, &FreeHead, &FreeTail);
     gOmInUseCount-= deflatedcount;
     nScavenged += deflatedcount;
     nInuse += gOmInUseCount;
    }

  } else {
      //通过gBlockList链表遍历所有已创建的ObjectMonitor，其中每个元素都是一个ObjectMonitor数组的头元素
    ObjectMonitor* block =
      (ObjectMonitor*)OrderAccess::load_ptr_acquire(&gBlockList);
    for (; block != NULL; block = (ObjectMonitor*)next(block)) {
      // Iterate over all extant monitors - Scavenge all idle monitors.
      assert(block->object() == CHAINMARKER, "must be a block header");
      nInCirculation += _BLOCKSIZE;
      for (int i = 1; i < _BLOCKSIZE; i++) {
        ObjectMonitor* mid = (ObjectMonitor*)&block[i];
        oop obj = (oop)mid->object();

        if (obj == NULL) {
          // The monitor is not associated with an object.
          // The monitor should either be a thread-specific private
          // free list or the global free list.
          // obj == NULL IMPLIES mid->is_busy() == 0
            //obj为空说明其未被分配出去，还在本地或者全局空闲链表中
          guarantee(!mid->is_busy(), "invariant");
          continue;
        }
          //如果该ObjectMonitor未使用则将其插入到FreeTail链表中
        deflated = deflate_monitor(mid, obj, &FreeHead, &FreeTail);

        if (deflated) {
            //未被使用，将其FreeNext置为null，因为在分配该ObjectMonitor时已经将该ObjectMonitor从空闲链表中移除了，所以此处不需要再次将其从空闲链表中移除
          mid->FreeNext = NULL;
          nScavenged++;
        } else {
          nInuse++;
        }
      }
    }
  }

    //增加空闲计数
  MonitorFreeCount += nScavenged;

  // Consider: audit gFreeList to ensure that MonitorFreeCount and list agree.

  if (ObjectMonitor::Knob_Verbose) {
    ::printf ("Deflate: InCirc=%d InUse=%d Scavenged=%d ForceMonitorScavenge=%d : pop=%d free=%d\n",
        nInCirculation, nInuse, nScavenged, ForceMonitorScavenge,
        MonitorPopulation, MonitorFreeCount) ;
    ::fflush(stdout) ;
  }

  ForceMonitorScavenge = 0;    // Reset

  // Move the scavenged monitors back to the global free list.
    //如果回收掉了ObjectMonitor实例
  if (FreeHead != NULL) {
     guarantee (FreeTail != NULL && nScavenged > 0, "invariant") ;
     assert (FreeTail->FreeNext == NULL, "invariant") ;
     // constant-time list splice - prepend scavenged segment to gFreeList
      //将FreeTail插入到gFreeList的后面
     FreeTail->FreeNext = gFreeList ;
     gFreeList = FreeHead ;
  }
    //释放锁
  Thread::muxRelease (&ListLock) ;

    // 增加计数
  if (ObjectMonitor::_sync_Deflations != NULL) ObjectMonitor::_sync_Deflations->inc(nScavenged) ;
  if (ObjectMonitor::_sync_MonExtant  != NULL) ObjectMonitor::_sync_MonExtant ->set_value(nInCirculation);

  // TODO: Add objectMonitor leak detection.
  // Audit/inventory the objectMonitors -- make sure they're all accounted for.
    // 增加计数
  GVars.stwRandom = os::random() ;
  GVars.stwCycle ++ ;
}

// Monitor cleanup on JavaThread::exit

// Iterate through monitor cache and attempt to release thread's monitors
// Gives up on a particular monitor if an exception occurs, but continues
// the overall iteration, swallowing the exception.
class ReleaseJavaMonitorsClosure: public MonitorClosure {
private:
  TRAPS;

public:
  ReleaseJavaMonitorsClosure(Thread* thread) : THREAD(thread) {}
  void do_monitor(ObjectMonitor* mid) {
    if (mid->owner() == THREAD) {
      (void)mid->complete_exit(CHECK);
    }
  }
};

// Release all inflated monitors owned by THREAD.  Lightweight monitors are
// ignored.  This is meant to be called during JNI thread detach which assumes
// all remaining monitors are heavyweight.  All exceptions are swallowed.
// Scanning the extant monitor list can be time consuming.
// A simple optimization is to add a per-thread flag that indicates a thread
// called jni_monitorenter() during its lifetime.
//
// Instead of No_Savepoint_Verifier it might be cheaper to
// use an idiom of the form:
//   auto int tmp = SafepointSynchronize::_safepoint_counter ;
//   <code that must not run at safepoint>
//   guarantee (((tmp ^ _safepoint_counter) | (tmp & 1)) == 0) ;
// Since the tests are extremely cheap we could leave them enabled
// for normal product builds.

void ObjectSynchronizer::release_monitors_owned_by_thread(TRAPS) {
  assert(THREAD == JavaThread::current(), "must be current Java thread");
  No_Safepoint_Verifier nsv ;
  ReleaseJavaMonitorsClosure rjmc(THREAD);
  Thread::muxAcquire(&ListLock, "release_monitors_owned_by_thread");
  ObjectSynchronizer::monitors_iterate(&rjmc);
  Thread::muxRelease(&ListLock);
  THREAD->clear_pending_exception();
}

//------------------------------------------------------------------------------
// Debugging code

void ObjectSynchronizer::sanity_checks(const bool verbose,
                                       const uint cache_line_size,
                                       int *error_cnt_ptr,
                                       int *warning_cnt_ptr) {
  u_char *addr_begin      = (u_char*)&GVars;
  u_char *addr_stwRandom  = (u_char*)&GVars.stwRandom;
  u_char *addr_hcSequence = (u_char*)&GVars.hcSequence;

  if (verbose) {
    tty->print_cr("INFO: sizeof(SharedGlobals)=" SIZE_FORMAT,
                  sizeof(SharedGlobals));
  }

  uint offset_stwRandom = (uint)(addr_stwRandom - addr_begin);
  if (verbose) tty->print_cr("INFO: offset(stwRandom)=%u", offset_stwRandom);

  uint offset_hcSequence = (uint)(addr_hcSequence - addr_begin);
  if (verbose) {
    tty->print_cr("INFO: offset(_hcSequence)=%u", offset_hcSequence);
  }

  if (cache_line_size != 0) {
    // We were able to determine the L1 data cache line size so
    // do some cache line specific sanity checks

    if (offset_stwRandom < cache_line_size) {
      tty->print_cr("WARNING: the SharedGlobals.stwRandom field is closer "
                    "to the struct beginning than a cache line which permits "
                    "false sharing.");
      (*warning_cnt_ptr)++;
    }

    if ((offset_hcSequence - offset_stwRandom) < cache_line_size) {
      tty->print_cr("WARNING: the SharedGlobals.stwRandom and "
                    "SharedGlobals.hcSequence fields are closer than a cache "
                    "line which permits false sharing.");
      (*warning_cnt_ptr)++;
    }

    if ((sizeof(SharedGlobals) - offset_hcSequence) < cache_line_size) {
      tty->print_cr("WARNING: the SharedGlobals.hcSequence field is closer "
                    "to the struct end than a cache line which permits false "
                    "sharing.");
      (*warning_cnt_ptr)++;
    }
  }
}

#ifndef PRODUCT

// Verify all monitors in the monitor cache, the verification is weak.
void ObjectSynchronizer::verify() {
  ObjectMonitor* block =
    (ObjectMonitor *)OrderAccess::load_ptr_acquire(&gBlockList);
  while (block != NULL) {
    assert(block->object() == CHAINMARKER, "must be a block header");
    for (int i = 1; i < _BLOCKSIZE; i++) {
      ObjectMonitor* mid = (ObjectMonitor *)(block + i);
      oop object = (oop)mid->object();
      if (object != NULL) {
        mid->verify();
      }
    }
    block = (ObjectMonitor*) block->FreeNext;
  }
}

// Check if monitor belongs to the monitor cache
// The list is grow-only so it's *relatively* safe to traverse
// the list of extant blocks without taking a lock.

int ObjectSynchronizer::verify_objmon_isinpool(ObjectMonitor *monitor) {
  ObjectMonitor* block =
    (ObjectMonitor*)OrderAccess::load_ptr_acquire(&gBlockList);
  while (block != NULL) {
    assert(block->object() == CHAINMARKER, "must be a block header");
    if (monitor > &block[0] && monitor < &block[_BLOCKSIZE]) {
      address mon = (address)monitor;
      address blk = (address)block;
      size_t diff = mon - blk;
      assert((diff % sizeof(ObjectMonitor)) == 0, "must be aligned");
      return 1;
    }
    block = (ObjectMonitor*)block->FreeNext;
  }
  return 0;
}

#endif
