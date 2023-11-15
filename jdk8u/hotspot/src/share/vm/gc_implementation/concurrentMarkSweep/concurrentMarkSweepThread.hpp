/*
 * Copyright (c) 2001, 2014, Oracle and/or its affiliates. All rights reserved.
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

#ifndef SHARE_VM_GC_IMPLEMENTATION_CONCURRENTMARKSWEEP_CONCURRENTMARKSWEEPTHREAD_HPP
#define SHARE_VM_GC_IMPLEMENTATION_CONCURRENTMARKSWEEP_CONCURRENTMARKSWEEPTHREAD_HPP

#include "gc_implementation/concurrentMarkSweep/concurrentMarkSweepGeneration.hpp"
#include "gc_implementation/shared/concurrentGCThread.hpp"
#include "runtime/thread.inline.hpp"

class ConcurrentMarkSweepGeneration;
class CMSCollector;

// The Concurrent Mark Sweep GC Thread
//  ConcurrentMarkSweepThread表示CMS并行标记清理的线程，其定义在hotspot\src\share\vm\gc_implementation\concurrentMarkSweep\concurrentMarkSweepThread.hpp中
//
// ConcurrentMarkSweepThread在执行过程中如果收到请求会让出CPU的使用权限（yield动作），从而保证业务线程优先执行。
// 有两种请求方式，一种是同步的，适用于年轻代垃圾收集和老年代的内存分配，请求方会在执行业务逻辑前把_pending_yields加1，在执行业务逻辑后把_pending_yields减1，
// ConcurrentMarkSweepThread在执行相关GC逻辑时会判断_pending_yields是否大于0，如果大于0则会让当前线程休眠1ms，会不断循环直到_pending_yields等于0为止；
// 另一种是异步的，在iCMS模式下使用，请求方通过调用stop_icms方法通知CMS Thread暂停执行，在iCMS_lock上等待，当请求方执行完相关逻辑会调用start_icms唤醒CMS Thread继续执行，
// 注意请求方调用stop_icms方法后并不会像第一种同步的请求方式立即通知CMS Thread业务操作执行完成。
//
// 通过CMSSynchronousYieldRequest的构造和析构函数实现第一种同步请求；asynchronous_yield_request方法用于增加_pending_yields和_pending_decrements计数，
// acknowledge_yield_request用于减少_pending_yields和_pending_decrements计数，两者都是通过原子指令操作；should_yield方法判断当前CMS Thread是否需要让出CPU使用权限
class ConcurrentMarkSweepThread: public ConcurrentGCThread {
  friend class VMStructs;
  friend class ConcurrentMarkSweepGeneration;   // XXX should remove friendship
  friend class CMSCollector;
 public:
  virtual void run();

 private:
    //全局的ConcurrentMarkSweepThread实例
  static ConcurrentMarkSweepThread*     _cmst;
    //关联的CMSCollector
  static CMSCollector*                  _collector;
    //关联的SurrogateLockerThread
  static SurrogateLockerThread*         _slt;
    //设置SurrogateLockerThread的buffer属性
  static SurrogateLockerThread::SLT_msg_type _sltBuffer;
  static Monitor*                       _sltMonitor;

    //初始为fase，当前线程是否应该终止
  static bool _should_terminate;

  enum CMS_flag_type {
      // NoBits的值就是0
    CMS_nil             = NoBits,
      // nth_bit就是把1右移多少位，即不同的flag Type对应不同位，所以不同的flag Type可以共存。
    CMS_cms_wants_token = nth_bit(0),
    CMS_cms_has_token   = nth_bit(1),
    CMS_vm_wants_token  = nth_bit(2),
    CMS_vm_has_token    = nth_bit(3)
  };

    //初始为CMS_nil，实际就是0，_CMS_flag的取值是一个枚举CMS_flag_type
  static int _CMS_flag;

  static bool CMS_flag_is_set(int b)        { return (_CMS_flag & b) != 0;   }
  static bool set_CMS_flag(int b)           { return (_CMS_flag |= b) != 0;  }
  static bool clear_CMS_flag(int b)         { return (_CMS_flag &= ~b) != 0; }
  void sleepBeforeNextCycle();

  // CMS thread should yield for a young gen collection, direct allocation,
  // and iCMS activity.
    // 用来避免高速缓存的缓存行共享问题，ConcurrentMarkSweepThread未使用
  static char _pad_1[64 - sizeof(jint)];    // prevent cache-line sharing
    //下面两个属性用来判断GC线程是否应该让出CPU的使用权，_pending_yields大于0则应该让出CPU使用权
  static volatile jint _pending_yields;
    //注意_pending_decrements是只在iCMS模式下使用
  static volatile jint _pending_decrements; // decrements to _pending_yields
    // 同上(_pad_1)
  static char _pad_2[64 - sizeof(jint)];    // prevent cache-line sharing

  // Tracing messages, enabled by CMSTraceThreadState.
  static inline void trace_state(const char* desc);

    // 跟踪iCMS模式开启和关闭的计数器
  static volatile int _icms_disabled;   // a counter to track #iCMS disable & enable
    //初始为false
  static volatile bool _should_run;     // iCMS may run
    //初始为true
  static volatile bool _should_stop;    // iCMS should stop

  // debugging
  void verify_ok_to_terminate() const PRODUCT_RETURN;

 public:
  // Constructor
  ConcurrentMarkSweepThread(CMSCollector* collector);

  static void makeSurrogateLockerThread(TRAPS);
  static SurrogateLockerThread* slt() { return _slt; }

  // Tester
  bool is_ConcurrentGC_thread() const { return true;       }

  static void threads_do(ThreadClosure* tc);

  // Printing
  static void print_all_on(outputStream* st);
  static void print_all()                             { print_all_on(tty); }

  // Returns the CMS Thread
  static ConcurrentMarkSweepThread* cmst()    { return _cmst; }
  static CMSCollector*         collector()    { return _collector;  }

  // Create and start the CMS Thread, or stop it on shutdown
  static ConcurrentMarkSweepThread* start(CMSCollector* collector);
  static void stop();
  static bool should_terminate() { return _should_terminate; }

  // Synchronization using CMS token
  static void synchronize(bool is_cms_thread);
  static void desynchronize(bool is_cms_thread);
  static bool vm_thread_has_cms_token() {
    return CMS_flag_is_set(CMS_vm_has_token);
  }
  static bool cms_thread_has_cms_token() {
    return CMS_flag_is_set(CMS_cms_has_token);
  }
  static bool vm_thread_wants_cms_token() {
    return CMS_flag_is_set(CMS_vm_wants_token);
  }
  static bool cms_thread_wants_cms_token() {
    return CMS_flag_is_set(CMS_cms_wants_token);
  }

  // Wait on CMS lock until the next synchronous GC
  // or given timeout, whichever is earlier. A timeout value
  // of 0 indicates that there is no upper bound on the wait time.
  // A concurrent full gc request terminates the wait.
  void wait_on_cms_lock(long t_millis);

  // Wait on CMS lock until the next synchronous GC
  // or given timeout, whichever is earlier. A timeout value
  // of 0 indicates that there is no upper bound on the wait time.
  // A concurrent full gc request terminates the wait.
  void wait_on_cms_lock_for_scavenge(long t_millis);

  // The CMS thread will yield during the work portion of its cycle
  // only when requested to.  Both synchronous and asychronous requests
  // are provided:
  // (1) A synchronous request is used for young gen collections and
  //     for direct allocations.  The requesting thread increments
  //     _pending_yields at the beginning of an operation, and decrements
  //     _pending_yields when that operation is completed.
  //     In turn, the CMS thread yields when _pending_yields is positive,
  //     and continues to yield until the value reverts to 0.
  // (2) An asynchronous request, on the other hand, is used by iCMS
  //     for the stop_icms() operation. A single yield satisfies all of
  //     the outstanding asynch yield requests, of which there may
  //     occasionally be several in close succession. To accomplish
  //     this, an asynch-requesting thread atomically increments both
  //     _pending_yields and _pending_decrements. An asynchr requesting
  //     thread does not wait and "acknowledge" completion of an operation
  //     and deregister the request, like the synchronous version described
  //     above does. In turn, after yielding, the CMS thread decrements both
  //     _pending_yields and _pending_decrements by the value seen in
  //     _pending_decrements before the decrement.
  //  NOTE: The above scheme is isomorphic to having two request counters,
  //  one for async requests and one for sync requests, and for the CMS thread
  //  to check the sum of the two counters to decide whether it should yield
  //  and to clear only the async counter when it yields. However, it turns out
  //  to be more efficient for CMS code to just check a single counter
  //  _pending_yields that holds the sum (of both sync and async requests), and
  //  a second counter _pending_decrements that only holds the async requests,
  //  for greater efficiency, since in a typical CMS run, there are many more
  //  pontential (i.e. static) yield points than there are actual
  //  (i.e. dynamic) yields because of requests, which are few and far between.
  //
  // Note that, while "_pending_yields >= _pending_decrements" is an invariant,
  // we cannot easily test that invariant, since the counters are manipulated via
  // atomic instructions without explicit locking and we cannot read
  // the two counters atomically together: one suggestion is to
  // use (for example) 16-bit counters so as to be able to read the
  // two counters atomically even on 32-bit platforms. Notice that
  // the second assert in acknowledge_yield_request() below does indeed
  // check a form of the above invariant, albeit indirectly.

  static void increment_pending_yields()   {
    Atomic::inc(&_pending_yields);
    assert(_pending_yields >= 0, "can't be negative");
  }
  static void decrement_pending_yields()   {
    Atomic::dec(&_pending_yields);
    assert(_pending_yields >= 0, "can't be negative");
  }
  static void asynchronous_yield_request() {
    assert(CMSIncrementalMode, "Currently only used w/iCMS");
      //同时原子的增加_pending_yields和_pending_decrements
    increment_pending_yields();
    Atomic::inc(&_pending_decrements);
    assert(_pending_decrements >= 0, "can't be negative");
  }
  static void acknowledge_yield_request() {
      //获取当值的值
    jint decrement = _pending_decrements;
    if (decrement > 0) {
        //decrement大于0说明开启了iCMS模式
      assert(CMSIncrementalMode, "Currently only used w/iCMS");
      // Order important to preserve: _pending_yields >= _pending_decrements
        //注意 _pending_yields >= _pending_decrements，这里相当于将decrement重置成0，_pending_decrements不一定是0
      Atomic::add(-decrement, &_pending_decrements);
      Atomic::add(-decrement, &_pending_yields);
        //在并发环境下decrement也可能不等于0
      assert(_pending_decrements >= 0, "can't be negative");
      assert(_pending_yields >= 0, "can't be negative");
    }
  }
  static bool should_yield()   { return _pending_yields > 0; }

  // CMS incremental mode.
  static void start_icms(); // notify thread to start a quantum of work
  static void stop_icms();  // request thread to stop working
  void icms_wait();         // if asked to stop, wait until notified to start

  // Incremental mode is enabled globally by the flag CMSIncrementalMode.  It
  // must also be enabled/disabled dynamically to allow foreground collections.
#define ICMS_ENABLING_ASSERT                                                      \
          assert((CMSIncrementalMode  && _icms_disabled >= 0) ||                  \
                 (!CMSIncrementalMode && _icms_disabled <= 0), "Error")

  static inline void enable_icms() {
    ICMS_ENABLING_ASSERT;
    Atomic::dec(&_icms_disabled);
  }
  static inline void disable_icms() {
   ICMS_ENABLING_ASSERT;
   Atomic::inc(&_icms_disabled);
  }
  static inline bool icms_is_disabled() {
   ICMS_ENABLING_ASSERT;
   return _icms_disabled > 0;
  }
  static inline bool icms_is_enabled() {
   return !icms_is_disabled();
  }
};

inline void ConcurrentMarkSweepThread::trace_state(const char* desc) {
  if (CMSTraceThreadState) {
    char buf[128];
    TimeStamp& ts = gclog_or_tty->time_stamp();
    if (!ts.is_updated()) {
      ts.update();
    }
    jio_snprintf(buf, sizeof(buf), " [%.3f:  CMSThread %s] ",
                 ts.seconds(), desc);
    buf[sizeof(buf) - 1] = '\0';
    gclog_or_tty->print("%s", buf);
  }
}

// For scoped increment/decrement of (synchronous) yield requests
class CMSSynchronousYieldRequest: public StackObj {
 public:
  CMSSynchronousYieldRequest() {
    ConcurrentMarkSweepThread::increment_pending_yields();
  }
  ~CMSSynchronousYieldRequest() {
    ConcurrentMarkSweepThread::decrement_pending_yields();
  }
};

// Used to emit a warning in case of unexpectedly excessive
// looping (in "apparently endless loops") in CMS code.
class CMSLoopCountWarn: public StackObj {
 private:
  const char* _src;
  const char* _msg;
  const intx  _threshold;
  intx        _ticks;

 public:
  inline CMSLoopCountWarn(const char* src, const char* msg,
                          const intx threshold) :
    _src(src), _msg(msg), _threshold(threshold), _ticks(0) { }

  inline void tick() {
    _ticks++;
    if (CMSLoopWarn && _ticks % _threshold == 0) {
      warning("%s has looped " INTX_FORMAT " times %s", _src, _ticks, _msg);
    }
  }
};

#endif // SHARE_VM_GC_IMPLEMENTATION_CONCURRENTMARKSWEEP_CONCURRENTMARKSWEEPTHREAD_HPP
