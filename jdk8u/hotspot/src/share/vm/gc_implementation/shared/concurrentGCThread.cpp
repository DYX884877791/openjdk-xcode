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
#include "gc_implementation/shared/concurrentGCThread.hpp"
#include "oops/instanceRefKlass.hpp"
#include "oops/oop.inline.hpp"
#include "runtime/init.hpp"
#include "runtime/interfaceSupport.hpp"
#include "runtime/java.hpp"
#include "runtime/javaCalls.hpp"

// CopyrightVersion 1.2

int  ConcurrentGCThread::_CGC_flag            = CGC_nil;

ConcurrentGCThread::ConcurrentGCThread() :
  _should_terminate(false), _has_terminated(false) {
};

//用于创建关联的本地线程并启动
void ConcurrentGCThread::create_and_start() {
    //创建关联的本地线程
  if (os::create_thread(this, os::cgc_thread)) {
    // XXX: need to set this to low priority
    // unless "agressive mode" set; priority
    // should be just less than that of VMThread.
      //设置优先级
    os::set_priority(this, NearMaxPriority);
      //DisableStartThread的默认值是false
    if (!_should_terminate && !DisableStartThread) {
        //启动本地线程
      os::start_thread(this);
    }
  }
}

// 初始化ConcurrentGCThread本身，记录线程栈基地址和大小，初始化TLAB等
void ConcurrentGCThread::initialize_in_thread() {
    //记录线程栈的基地址和大小
  this->record_stack_base_and_size();
    //初始化线程的本地存储空间TLAB
  this->initialize_thread_local_storage();
    //设置JNIHandleBlock
  this->initialize_named_thread();
  this->set_active_handles(JNIHandleBlock::allocate_block());
  // From this time Thread::current() should be working.
  assert(this == Thread::current(), "just checking");
}

// 不断循环等待Universe初始化完成
void ConcurrentGCThread::wait_for_universe_init() {
    //获取锁CGC_lock
  MutexLockerEx x(CGC_lock, Mutex::_no_safepoint_check_flag);
    //不断循环，每隔200ms检查一次universe是否初始化完成
  while (!is_init_completed() && !_should_terminate) {
    CGC_lock->wait(Mutex::_no_safepoint_check_flag, 200);
  }
}

// 终止当前ConcurrentGCThread的执行
void ConcurrentGCThread::terminate() {
  // Signal that it is terminated
  {
      //获取锁Terminator_lock
    MutexLockerEx mu(Terminator_lock,
                     Mutex::_no_safepoint_check_flag);
    _has_terminated = true;
    Terminator_lock->notify();
  }

  // Thread destructor usually does this..
  ThreadLocalStorage::set_thread(NULL);
}

static void _sltLoop(JavaThread* thread, TRAPS) {
  SurrogateLockerThread* slt = (SurrogateLockerThread*)thread;
  slt->loop();
}

SurrogateLockerThread::SurrogateLockerThread() :
    //_sltLoop就是该线程执行的逻辑，即Run方法，JavaThread构造方法中会负责初始化相关属性并创建关联的本地线程
  JavaThread(&_sltLoop),
  _monitor(Mutex::nonleaf, "SLTMonitor"),
  _buffer(empty)
{}

// make方法用于创建一个SurrogateLockerThread，将其与一个java.lang.Thread实例绑定，并启动线程的执行
SurrogateLockerThread* SurrogateLockerThread::make(TRAPS) {
    //获取java_lang_Thread对应的Klass
  Klass* k =
    SystemDictionary::resolve_or_fail(vmSymbols::java_lang_Thread(),
                                      true, CHECK_NULL);
  instanceKlassHandle klass (THREAD, k);
    //thread_oop用于保存创建的线程实例
  instanceHandle thread_oop = klass->allocate_instance_handle(CHECK_NULL);

  const char thread_name[] = "Surrogate Locker Thread (Concurrent GC)";
  Handle string = java_lang_String::create_from_str(thread_name, CHECK_NULL);

  // Initialize thread_oop to put it into the system threadGroup
  Handle thread_group (THREAD, Universe::system_thread_group());
  JavaValue result(T_VOID);
    //调用构造方法 Thread(ThreadGroup group, String name) 创建一个Thread实例，结果保存在thread_oop中
  JavaCalls::call_special(&result, thread_oop,
                          klass,
                          vmSymbols::object_initializer_name(), //构造方法的方法名
                          vmSymbols::threadgroup_string_void_signature(),   //构造方法的方法签名
                          thread_group, //两个参数
                          string,
                          CHECK_NULL);

  SurrogateLockerThread* res;
  {
    MutexLocker mu(Threads_lock);
    res = new SurrogateLockerThread();

    // At this point it may be possible that no osthread was created for the
    // JavaThread due to lack of memory. We would have to throw an exception
    // in that case. However, since this must work and we do not allow
    // exceptions anyway, check and abort if this fails.
    if (res == NULL || res->osthread() == NULL) {
        //res创建或者初始化失败，通常是因为内存不足导致的
      vm_exit_during_initialization("java.lang.OutOfMemoryError",
                                    "unable to create new native thread");
    }
      //将Thread实例同res关联起来，底层是设置Thread的eetop属性，该属性就是关联的C++线程对象的地址
    java_lang_Thread::set_thread(thread_oop(), res);
      //设置Thread实例的优先级
    java_lang_Thread::set_priority(thread_oop(), NearMaxPriority);
      //设置Thread实例的daemon属性
    java_lang_Thread::set_daemon(thread_oop());

      //res与Thread实例关联
    res->set_threadObj(thread_oop());
      //添加到Threads中保存的线程链表
    Threads::add(res);
      //启动res线程的执行，即构造时传入的_sltLoop方法
    Thread::start(res);
  }
  os::yield(); // This seems to help with initial start-up of SLT
  return res;
}

void SurrogateLockerThread::report_missing_slt() {
  vm_exit_during_initialization(
    "GC before GC support fully initialized: "
    "SLT is needed but has not yet been created.");
  ShouldNotReachHere();
}

//  manipulatePLL会修改_buffer属性，然后释放锁等待loop方法执行完成相应的动作
void SurrogateLockerThread::manipulatePLL(SLT_msg_type msg) {
    //获取锁_monitor
  MutexLockerEx x(&_monitor, Mutex::_no_safepoint_check_flag);
  assert(_buffer == empty, "Should be empty");
  assert(msg != empty, "empty message");
  assert(!Heap_lock->owned_by_self(), "Heap_lock owned by requesting thread");

    //修改buffer
  _buffer = msg;
  while (_buffer != empty) {
      //notify会释放锁monitor，并唤醒等待该锁的线程，此时loop方法会获取锁并根据msg执行相应的加锁或者释放锁
      //执行完成再将buffer重置为empty并唤醒等待的线程，调用manipulatePLL方法的线程就可以退出此方法了
    _monitor.notify();
    _monitor.wait(Mutex::_no_safepoint_check_flag);
  }
}

// ======= Surrogate Locker Thread =============

// loop方法不断循环，根据_buffer属性的值执行加锁和释放锁，执行完成将buffer属性重置为empty
void SurrogateLockerThread::loop() {
    //pll是pending list lock的简称
  BasicLock pll_basic_lock;
  SLT_msg_type msg;
  debug_only(unsigned int owned = 0;)

  while (/* !isTerminated() */ 1) {
    {
      MutexLocker x(&_monitor);
      // Since we are a JavaThread, we can't be here at a safepoint.
      assert(!SafepointSynchronize::is_at_safepoint(),
             "SLT is a JavaThread");
      // wait for msg buffer to become non-empty
        //等待msg变成非empty
      while (_buffer == empty) {
        _monitor.notify();
        _monitor.wait();
      }
      msg = _buffer;
    }
    switch(msg) {
      case acquirePLL: {
          //获取锁
        InstanceRefKlass::acquire_pending_list_lock(&pll_basic_lock);
        debug_only(owned++;)
        break;
      }
      case releaseAndNotifyPLL: {
        assert(owned > 0, "Don't have PLL");
          //释放锁
        InstanceRefKlass::release_and_notify_pending_list_lock(&pll_basic_lock);
        debug_only(owned--;)
        break;
      }
      case empty:
      default: {
        guarantee(false,"Unexpected message in _buffer");
        break;
      }
    }
    {
      MutexLocker x(&_monitor);
      // Since we are a JavaThread, we can't be here at a safepoint.
      assert(!SafepointSynchronize::is_at_safepoint(),
             "SLT is a JavaThread");
        //将buffer重置为empty
      _buffer = empty;
      _monitor.notify();
    }
  }
  assert(!_monitor.owned_by_self(), "Should unlock before exit.");
}
