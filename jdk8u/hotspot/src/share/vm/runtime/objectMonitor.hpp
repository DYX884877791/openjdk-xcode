/*
 * Copyright (c) 1998, 2013, Oracle and/or its affiliates. All rights reserved.
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

#ifndef SHARE_VM_RUNTIME_OBJECTMONITOR_HPP
#define SHARE_VM_RUNTIME_OBJECTMONITOR_HPP

#include "runtime/os.hpp"
#include "runtime/park.hpp"
#include "runtime/perfData.hpp"

// ObjectWaiter serves as a "proxy" or surrogate thread.
// TODO-FIXME: Eliminate ObjectWaiter and use the thread-specific
// ParkEvent instead.  Beware, however, that the JVMTI code
// knows about ObjectWaiters, so we'll have to reconcile that code.
// See next_waiter(), first_waiter(), etc.

// ObjectWaiter 顾名思义对象等待者
// ObjectWaiter 一个线程尝试获取monitor锁失败后，最终会被封装成一个ObjectWaiter对象 封装的是一个线程
// ObjectWaiter 类似一个双向链表
class ObjectWaiter : public StackObj {
 public:
  enum TStates { TS_UNDEF, TS_READY, TS_RUN, TS_WAIT, TS_ENTER, TS_CXQ } ;
  enum Sorted  { PREPEND, APPEND, SORTED } ;
    // 下一个 ObjectWaiter
  ObjectWaiter * volatile _next;
    // 上一个 ObjectWaiter
  ObjectWaiter * volatile _prev;
    // 线程
  Thread*       _thread;
    // _notifier_tid用于记录执行唤醒动作的线程指针
  jlong         _notifier_tid;
    // 关联的线程属性
  ParkEvent *   _event;
    // _notified属性用于记录该ObjectWaiter是否被某个线程唤醒了而不是因为线程中断唤醒的
  volatile int  _notified ;
    // TState用于描述当前ObjectWaiter的状态，刚创建时的状态是TS_RUN，加入到cxq链表中状态是TS_CXQ，加入到EntryList链表后变成TS_ENTER，加入到WaitSet链表中的状态就是TS_WAIT，另外两个状态枚举没有调用。
  volatile TStates TState ;
  Sorted        _Sorted ;           // List placement disposition
    // _active用于记录当前线程是否开启了线程监控，如果开启了可以通过jmm接口获取线程运行的统计数据，比如锁抢占的次数和累计耗时。
  bool          _active ;           // Contention monitoring is enabled
 public:
  ObjectWaiter(Thread* thread);

  void wait_reenter_begin(ObjectMonitor *mon);
  void wait_reenter_end(ObjectMonitor *mon);
};

// WARNING:
//   This is a very sensitive and fragile class. DO NOT make any
// change unless you are fully aware of the underlying semantics.

//   This class can not inherit from any other class, because I have
// to let the displaced header be the very first word. Otherwise I
// have to let markOop include this file, which would export the
// monitor data structure to everywhere.
//
// The ObjectMonitor class is used to implement JavaMonitors which have
// transformed from the lightweight structure of the thread stack to a
// heavy weight lock due to contention

// It is also used as RawMonitor by the JVMTI

/**
 * 就是我们常说的monitor对象
 * ObjectMonitor不仅是重量级锁的实现，还是Object的wait/notify/notifyAll方法的底层核心实现。
 *
 * ObjectMonitor维护了三个ObjectWaiter链表，分别是cxq链表、EntryList链表和WaitSet链表，对应链表中ObjectWaiter的状态分别是TS_CXQ，TS_ENTER和TS_WAIT。
 * 调用enter方法时，如果自旋获取锁失败就会创建一个ObjectWaiter并加入到cxq链表中，某个已经获取锁的线程调用wait方法会创建一个ObjectWaiter并加入到WaitSet链表中，
 * 当某个线程调用notify/notifyAll方法“唤醒”该线程时，会将该ObjectWaiter从WaitSet链表中移除然后加入到cxq链表头。
 * 当某个获取锁的线程释放锁时，就会唤醒EntryList链表头对应的线程，如果EntryList链表为空，则将此时的cxq链表中的元素整体转移到EntryList链表中，
 * 然后同样的唤醒EntryList链表头对应的线程，被唤醒后该线程一样调用enter方法抢占锁。
 */
class ObjectMonitor {
 public:
    // 异常枚举
  enum {
    OM_OK,                    // no error
    OM_SYSTEM_ERROR,          // operating system error
    OM_ILLEGAL_MONITOR_STATE, // IllegalMonitorStateException
    OM_INTERRUPTED,           // Thread.interrupt()
    OM_TIMED_OUT              // Object.wait() timed out
  };

 public:
  // TODO-FIXME: the "offset" routines should return a type of off_t instead of int ...
  // ByteSize would also be an appropriate type.
  static int header_offset_in_bytes()      { return offset_of(ObjectMonitor, _header);     }
  static int object_offset_in_bytes()      { return offset_of(ObjectMonitor, _object);     }
  static int owner_offset_in_bytes()       { return offset_of(ObjectMonitor, _owner);      }
  static int count_offset_in_bytes()       { return offset_of(ObjectMonitor, _count);      }
  static int recursions_offset_in_bytes()  { return offset_of(ObjectMonitor, _recursions); }
  static int cxq_offset_in_bytes()         { return offset_of(ObjectMonitor, _cxq) ;       }
  static int succ_offset_in_bytes()        { return offset_of(ObjectMonitor, _succ) ;      }
  static int EntryList_offset_in_bytes()   { return offset_of(ObjectMonitor, _EntryList);  }
  static int FreeNext_offset_in_bytes()    { return offset_of(ObjectMonitor, FreeNext);    }
  static int WaitSet_offset_in_bytes()     { return offset_of(ObjectMonitor, _WaitSet) ;   }
  static int Responsible_offset_in_bytes() { return offset_of(ObjectMonitor, _Responsible);}
  static int Spinner_offset_in_bytes()     { return offset_of(ObjectMonitor, _Spinner);    }

 public:
  // Eventaully we'll make provisions for multiple callbacks, but
  // now one will suffice.
  static int (*SpinCallbackFunction)(intptr_t, int) ;
  static intptr_t SpinCallbackArgument ;


 public:
  markOop   header() const;
  void      set_header(markOop hdr);

  intptr_t is_busy() const {
    // TODO-FIXME: merge _count and _waiters.
    // TODO-FIXME: assert _owner == null implies _recursions = 0
    // TODO-FIXME: assert _WaitSet != null implies _count > 0
    return _count|_waiters|intptr_t(_owner)|intptr_t(_cxq)|intptr_t(_EntryList ) ;
  }

  intptr_t  is_entered(Thread* current) const;

  void*     owner() const;
  void      set_owner(void* owner);

  intptr_t  waiters() const;

  intptr_t  count() const;
  void      set_count(intptr_t count);
  intptr_t  contentions() const ;
  intptr_t  recursions() const                                         { return _recursions; }

  // JVM/DI GetMonitorInfo() needs this
  ObjectWaiter* first_waiter()                                         { return _WaitSet; }
  ObjectWaiter* next_waiter(ObjectWaiter* o)                           { return o->_next; }
  Thread* thread_of_waiter(ObjectWaiter* o)                            { return o->_thread; }

  // initialize the monitor, exception the semaphore, all other fields
  // are simple integers or pointers
  ObjectMonitor() {
    _header       = NULL;
    _count        = 0;
    _waiters      = 0,
    _recursions   = 0;
    _object       = NULL;
    _owner        = NULL;
    _WaitSet      = NULL;
    _WaitSetLock  = 0 ;
    _Responsible  = NULL ;
    _succ         = NULL ;
    _cxq          = NULL ;
    FreeNext      = NULL ;
    _EntryList    = NULL ;
    _SpinFreq     = 0 ;
    _SpinClock    = 0 ;
    OwnerIsThread = 0 ;
    _previous_owner_tid = 0;
  }

  ~ObjectMonitor() {
   // TODO: Add asserts ...
   // _cxq == 0 _succ == NULL _owner == NULL _waiters == 0
   // _count == 0 _EntryList  == NULL etc
  }

private:
  void Recycle () {
    // TODO: add stronger asserts ...
    // _cxq == 0 _succ == NULL _owner == NULL _waiters == 0
    // _count == 0 EntryList  == NULL
    // _recursions == 0 _WaitSet == NULL
    // TODO: assert (is_busy()|_recursions) == 0
    _succ          = NULL ;
    _EntryList     = NULL ;
    _cxq           = NULL ;
    _WaitSet       = NULL ;
    _recursions    = 0 ;
    _SpinFreq      = 0 ;
    _SpinClock     = 0 ;
    OwnerIsThread  = 0 ;
  }

public:

  void*     object() const;
  void*     object_addr();
  void      set_object(void* obj);

  bool      check(TRAPS);       // true if the thread owns the monitor.
  void      check_slow(TRAPS);
  void      clear();
  static void sanity_checks();  // public for -XX:+ExecuteInternalVMTests
                                // in PRODUCT for -XX:SyncKnobs=Verbose=1
#ifndef PRODUCT
  void      verify();
  void      print();
#endif

  bool      try_enter (TRAPS) ;
  void      enter(TRAPS);
  void      exit(bool not_suspended, TRAPS);
  void      wait(jlong millis, bool interruptable, TRAPS);
  void      notify(TRAPS);
  void      notifyAll(TRAPS);

// Use the following at your own risk
  intptr_t  complete_exit(TRAPS);
  void      reenter(intptr_t recursions, TRAPS);

 private:
  void      AddWaiter (ObjectWaiter * waiter) ;
  static    void DeferredInitialize();

  ObjectWaiter * DequeueWaiter () ;
  void      DequeueSpecificWaiter (ObjectWaiter * waiter) ;
  void      EnterI (TRAPS) ;
  void      ReenterI (Thread * Self, ObjectWaiter * SelfNode) ;
  void      UnlinkAfterAcquire (Thread * Self, ObjectWaiter * SelfNode) ;
  int       TryLock (Thread * Self) ;
  int       NotRunnable (Thread * Self, Thread * Owner) ;
  int       TrySpin_Fixed (Thread * Self) ;
  int       TrySpin_VaryFrequency (Thread * Self) ;
  int       TrySpin_VaryDuration  (Thread * Self) ;
  void      ctAsserts () ;
  void      ExitEpilog (Thread * Self, ObjectWaiter * Wakee) ;
  bool      ExitSuspendEquivalent (JavaThread * Self) ;

 private:
  friend class ObjectSynchronizer;
  friend class ObjectWaiter;
  friend class VMStructs;

  // WARNING: this must be the very first word of ObjectMonitor
  // This means this class can't use any virtual member functions.

    // 锁对象oop的原始对象头
  volatile markOop   _header;       // displaced object header word - mark
    // 关联的锁对象oop
  void*     volatile _object;       // backward object pointer - strong root

  double SharingPad [1] ;           // temp to reduce false sharing

  // All the following fields must be machine word aligned
  // The VM assumes write ordering wrt these fields, which can be
  // read from other threads.

 protected:                         // protected for jvmtiRawMonitor
    // 占用当前锁的线程
  void *  volatile _owner;          // pointer to owning thread OR BasicLock
  volatile jlong _previous_owner_tid; // thread id of the previous owner of the monitor
    //记录嵌套（递归）加锁的次数，最外层的锁的_recursions属性为0
  volatile intptr_t  _recursions;   // recursion count, 0 for first entry
 private:
    // 表明当前owner原来持有轻量级锁
  int OwnerIsThread ;               // _owner is (Thread *) vs SP/BasicLock
    // cxq链表头元素
  ObjectWaiter * volatile _cxq ;    // LL of recently-arrived threads blocked on entry.
                                    // The list is actually composed of WaitNodes, acting
                                    // as proxies for Threads.
 protected:
    // EntryList 链表头元素
  ObjectWaiter * volatile _EntryList ;     // Threads blocked on entry or reentry.
 private:
  Thread * volatile _succ ;          // Heir presumptive thread - used for futile wakeup throttling
  Thread * volatile _Responsible ;
  int _PromptDrain ;                // rqst to drain cxq into EntryList ASAP

    // 用来记录正在自旋的线程数
  volatile int _Spinner ;           // for exit->spinner handoff optimization
  volatile int _SpinFreq ;          // Spin 1-out-of-N attempts: success rate
  volatile int _SpinClock ;
    //用来控制自旋的总次数
  volatile int _SpinDuration ;
  volatile intptr_t _SpinState ;    // MCS/CLH list of spinners

  // TODO-FIXME: _count, _waiters and _recursions should be of
  // type int, or int32_t but not intptr_t.  There's no reason
  // to use 64-bit fields for these variables on a 64-bit JVM.

    // 抢占该锁的线程数
  volatile intptr_t  _count;        // reference count to prevent reclaimation/deflation
                                    // at stop-the-world time.  See deflate_idle_monitors().
                                    // _count is approximately |_WaitSet| + |_EntryList|
 protected:
    // 调用wait方法后等待的线程数
  volatile intptr_t  _waiters;      // number of waiting threads
 private:
 protected:
    // 调用wait方法后等待的ObjectWaiter链表
  ObjectWaiter * volatile _WaitSet; // LL of threads wait()ing on the monitor
 private:
    // 操作WaitSet链表的锁
  volatile int _WaitSetLock;        // protects Wait Queue - simple spinlock

 public:
  int _QMix ;                       // Mixed prepend queue discipline
  ObjectMonitor * FreeNext ;        // Free list linkage
  intptr_t StatA, StatsB ;

 public:
  static void Initialize () ;
  static PerfCounter * _sync_ContendedLockAttempts ;
  static PerfCounter * _sync_FutileWakeups ;
  static PerfCounter * _sync_Parks ;
  static PerfCounter * _sync_EmptyNotifications ;
  static PerfCounter * _sync_Notifications ;
  static PerfCounter * _sync_SlowEnter ;
  static PerfCounter * _sync_SlowExit ;
  static PerfCounter * _sync_SlowNotify ;
  static PerfCounter * _sync_SlowNotifyAll ;
  static PerfCounter * _sync_FailedSpins ;
  static PerfCounter * _sync_SuccessfulSpins ;
  static PerfCounter * _sync_PrivateA ;
  static PerfCounter * _sync_PrivateB ;
  static PerfCounter * _sync_MonInCirculation ;
  static PerfCounter * _sync_MonScavenged ;
  static PerfCounter * _sync_Inflations ;
  static PerfCounter * _sync_Deflations ;
  static PerfLongVariable * _sync_MonExtant ;

 public:
  static int Knob_Verbose;
  static int Knob_SpinLimit;
  void* operator new (size_t size) throw() {
    return AllocateHeap(size, mtInternal);
  }
  void* operator new[] (size_t size) throw() {
    return operator new (size);
  }
  void operator delete(void* p) {
    FreeHeap(p, mtInternal);
  }
  void operator delete[] (void *p) {
    operator delete(p);
  }
};

#undef TEVENT
#define TEVENT(nom) {if (SyncVerbose) FEVENT(nom); }

#define FEVENT(nom) { static volatile int ctr = 0 ; int v = ++ctr ; if ((v & (v-1)) == 0) { ::printf (#nom " : %d \n", v); ::fflush(stdout); }}

#undef  TEVENT
#define TEVENT(nom) {;}


#endif // SHARE_VM_RUNTIME_OBJECTMONITOR_HPP
