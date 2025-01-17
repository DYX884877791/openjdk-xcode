/*
 * Copyright (c) 1997, 2014, Oracle and/or its affiliates. All rights reserved.
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

#ifndef SHARE_VM_RUNTIME_SAFEPOINT_HPP
#define SHARE_VM_RUNTIME_SAFEPOINT_HPP

#include "asm/assembler.hpp"
#include "code/nmethod.hpp"
#include "memory/allocation.hpp"
#include "runtime/extendedPC.hpp"
#include "runtime/mutexLocker.hpp"
#include "runtime/os.hpp"
#include "utilities/ostream.hpp"

//
// Safepoint synchronization
////
// The VMThread or CMS_thread uses the SafepointSynchronize::begin/end
// methods to enter/exit a safepoint region. The begin method will roll
// all JavaThreads forward to a safepoint.
//
// JavaThreads must use the ThreadSafepointState abstraction (defined in
// thread.hpp) to indicate that that they are at a safepoint.
//
// The Mutex/Condition variable and ObjectLocker classes calls the enter/
// exit safepoint methods, when a thread is blocked/restarted. Hence, all mutex exter/
// exit points *must* be at a safepoint.


class ThreadSafepointState;
class SnippetCache;
class nmethod;

//
// Implements roll-forward to safepoint (safepoint synchronization)
//
// SafepointSynchronize的定义位于hotspot\src\share\vm\runtime\safepoint.hpp中，用来实现安全点的进入和退出。
//
// 线程处于不同的状态下都有对应的机制让该线程进入到安全点，主要有以下几种：
//   处于解释执行中，解释器通过字节码路由表（dispatch table）开始执行下一个字节码时会检查安全点的状态
//   处于本地代码执行中，即正在执行JNI方法，当Java线程从JNI方法中退出后必须检查安全点的状态，VMThread并不会等待处于本地代码执行中的线程进入阻塞状态。
//   处于编译执行中，编译代码在适当的位置上会读取Safepoint Polling内存页，如果该内存页是脏的，则表示需要进入安全点
//   处于JVM执行中，即JVM执行上述三种状态的切换时，JVM会在切换前检查安全点的状态
//   处于阻塞状态，即等待某个锁，JVM会一直阻塞该线程直到VMThead从安全点退出
class SafepointSynchronize : AllStatic {
 public:
    // SynchronizeState是一个枚举，用来描述安全点的状态
  enum SynchronizeState {
        // 0表示所有线程都不在安全点上
      _not_synchronized = 0,                   // Threads not synchronized at a safepoint
                                               // Keep this value 0. See the coment in do_call_back()
        // 1表示所有线程在执行安全点同步中
      _synchronizing    = 1,                   // Synchronizing in progress
        // 2表示所有线程都停留在安全点上，只有VMThread在执行
      _synchronized     = 2                    // All Java threads are stopped at a safepoint. Only VM thread is running
  };

  enum SafepointingThread {
      _null_thread  = 0,
      _vm_thread    = 1,
      _other_thread = 2
  };

  enum SafepointTimeoutReason {
    _spinning_timeout = 0,
    _blocking_timeout = 1
  };

    // SafepointStats是一个数据结构，用来记录触发本次安全点的相关信息
  typedef struct {
    float  _time_stamp;                        // record when the current safepoint occurs in seconds
    int    _vmop_type;                         // type of VM operation triggers the safepoint
    int    _nof_total_threads;                 // total number of Java threads
    int    _nof_initial_running_threads;       // total number of initially seen running threads
    int    _nof_threads_wait_to_block;         // total number of threads waiting for to block
    bool   _page_armed;                        // true if polling page is armed, false otherwise
    int    _nof_threads_hit_page_trap;         // total number of threads hitting the page trap
    jlong  _time_to_spin;                      // total time in millis spent in spinning
    jlong  _time_to_wait_to_block;             // total time in millis spent in waiting for to block
    jlong  _time_to_do_cleanups;               // total time in millis spent in performing cleanups
    jlong  _time_to_sync;                      // total time in millis spent in getting to _synchronized
    jlong  _time_to_exec_vmop;                 // total time in millis spent in vm operation itself
  } SafepointStats;

 private:
    // 表示安全点的状态
  static volatile SynchronizeState _state;     // Threads might read this flag directly, without acquireing the Threads_lock
    // 等待被阻塞（同步）的线程数
  static volatile int _waiting_to_block;       // number of threads we are waiting for to block
    //记录安全点期间处于JNI关键区的线程的总数
  static int _current_jni_active_count;        // Counts the number of active critical natives during the safepoint

  // This counter is used for fast versions of jni_Get<Primitive>Field.
  // An even value means there is no ongoing safepoint operations.
  // The counter is incremented ONLY at the beginning and end of each
  // safepoint. The fact that Threads_lock is held throughout each pair of
  // increments (at the beginning and end of each safepoint) guarantees
  // race freedom.
public:
    //进入和退出安全点的总次数，进入和退出时都会加1
  static volatile int _safepoint_counter;
private:
    //上一次退出安全点的时间
  static long       _end_of_last_safepoint;     // Time of last safepoint in milliseconds

  // statistics
    // 进入安全点的时间，单位是纳秒
  static jlong            _safepoint_begin_time;     // time when safepoint begins
    // SafepointStats数组，剩下几个参数都是跟SafepointStats配合使用，用来统计安全点相关的数据
  static SafepointStats*  _safepoint_stats;          // array of SafepointStats struct
  static int              _cur_stat_index;           // current index to the above array
  static julong           _safepoint_reasons[];      // safepoint count for each VM op
  static julong           _coalesced_vmop_count;     // coalesced vmop count
  static jlong            _max_sync_time;            // maximum sync time in nanos
  static jlong            _max_vmop_time;            // maximum vm operation time in nanos
    // 进入安全点的时间，单位是秒
  static float            _ts_of_current_safepoint;  // time stamp of current safepoint in seconds

  static void begin_statistics(int nof_threads, int nof_running);
  static void update_statistics_on_spin_end();
  static void update_statistics_on_sync_end(jlong end_time);
  static void update_statistics_on_cleanup_end(jlong end_time);
  static void end_statistics(jlong end_time);
  static void print_statistics();
  inline static void inc_page_trap_count() {
    Atomic::inc(&_safepoint_stats[_cur_stat_index]._nof_threads_hit_page_trap);
  }

  // For debug long safepoint
  static void print_safepoint_timeout(SafepointTimeoutReason timeout_reason);

public:

  // Main entry points

  // Roll all threads forward to safepoint. Must be called by the
  // VMThread or CMS_thread.
  static void begin();
  static void end();                    // Start all suspended threads again...

  static bool safepoint_safe(JavaThread *thread, JavaThreadState state);

  static void check_for_lazy_critical_native(JavaThread *thread, JavaThreadState state);

  // Query
  inline static bool is_at_safepoint()   { return _state == _synchronized;  }
  inline static bool is_synchronizing()  { return _state == _synchronizing;  }
  inline static int safepoint_counter()  { return _safepoint_counter; }

  inline static bool do_call_back() {
    return (_state != _not_synchronized);
  }

  inline static void increment_jni_active_count() {
    assert_locked_or_safepoint(Safepoint_lock);
    _current_jni_active_count++;
  }

  // Called when a thread volantary blocks
  static void   block(JavaThread *thread);
  static void   signal_thread_at_safepoint()              { _waiting_to_block--; }

  // Exception handling for page polling
  static void handle_polling_page_exception(JavaThread *thread);

  // VM Thread interface for determining safepoint rate
  static long last_non_safepoint_interval() {
    return os::javaTimeMillis() - _end_of_last_safepoint;
  }
  static long end_of_last_safepoint() {
    return _end_of_last_safepoint;
  }
  static bool is_cleanup_needed();
  static void do_cleanup_tasks();

  // debugging
  static void print_state()                                PRODUCT_RETURN;
  static void safepoint_msg(const char* format, ...) ATTRIBUTE_PRINTF(1, 2) PRODUCT_RETURN;

  static void deferred_initialize_stat();
  static void print_stat_on_exit();
  inline static void inc_vmop_coalesced_count() { _coalesced_vmop_count++; }

  static void set_is_at_safepoint()                        { _state = _synchronized; }
  static void set_is_not_at_safepoint()                    { _state = _not_synchronized; }

  // assembly support
  static address address_of_state()                        { return (address)&_state; }

  static address safepoint_counter_addr()                  { return (address)&_safepoint_counter; }
};

// State class for a thread suspended at a safepoint
class ThreadSafepointState: public CHeapObj<mtThread> {
 public:
  // These states are maintained by VM thread while threads are being brought
  // to a safepoint.  After SafepointSynchronize::end(), they are reset to
  // _running.
  enum suspend_type {
    _running                =  0, // Thread state not yet determined (i.e., not at a safepoint yet)
    _at_safepoint           =  1, // Thread at a safepoint (f.ex., when blocked on a lock)
    _call_back              =  2  // Keep executing and wait for callback (if thread is in interpreted or vm)
  };
 private:
  volatile bool _at_poll_safepoint;  // At polling page safepoint (NOT a poll return safepoint)
  // Thread has called back the safepoint code (for debugging)
  bool                           _has_called_back;

  JavaThread *                   _thread;
  volatile suspend_type          _type;
  JavaThreadState                _orig_thread_state;


 public:
  ThreadSafepointState(JavaThread *thread);

  // examine/roll-forward/restart
  void examine_state_of_thread();
  void roll_forward(suspend_type type);
  void restart();

  // Query
  JavaThread*  thread() const         { return _thread; }
  suspend_type type() const           { return _type; }
  bool         is_running() const     { return (_type==_running); }
  JavaThreadState orig_thread_state() const { return _orig_thread_state; }

  // Support for safepoint timeout (debugging)
  bool has_called_back() const                   { return _has_called_back; }
  void set_has_called_back(bool val)             { _has_called_back = val; }
  bool              is_at_poll_safepoint() { return _at_poll_safepoint; }
  void              set_at_poll_safepoint(bool val) { _at_poll_safepoint = val; }

  void handle_polling_page_exception();

  // debugging
  void print_on(outputStream* st) const;
  void print() const                        { print_on(tty); }

  // Initialize
  static void create(JavaThread *thread);
  static void destroy(JavaThread *thread);

  void safepoint_msg(const char* format, ...) ATTRIBUTE_PRINTF(2, 3) {
    if (ShowSafepointMsgs) {
      va_list ap;
      va_start(ap, format);
      tty->vprint_cr(format, ap);
      va_end(ap);
    }
  }
};



#endif // SHARE_VM_RUNTIME_SAFEPOINT_HPP
