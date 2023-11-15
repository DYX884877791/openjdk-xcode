/*
 * Copyright (c) 1998, 2012, Oracle and/or its affiliates. All rights reserved.
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

#ifndef SHARE_VM_RUNTIME_VMTHREAD_HPP
#define SHARE_VM_RUNTIME_VMTHREAD_HPP

#include "runtime/perfData.hpp"
#include "runtime/thread.inline.hpp"
#include "runtime/vm_operations.hpp"

//
// Prioritized queue of VM operations.
//
// Encapsulates both queue management and
// and priority policy
//
class VMOperationQueue : public CHeapObj<mtInternal> {
 private:
  enum Priorities {
     SafepointPriority, // Highest priority (operation executed at a safepoint)
     MediumPriority,    // Medium priority
     nof_priorities
  };

  // We maintain a doubled linked list, with explicit count.
  int           _queue_length[nof_priorities];
  int           _queue_counter;
  VM_Operation* _queue       [nof_priorities];
  // we also allow the vmThread to register the ops it has drained so we
  // can scan them from oops_do
  VM_Operation* _drain_list;

  // Double-linked non-empty list insert.
  void insert(VM_Operation* q,VM_Operation* n);
  void unlink(VM_Operation* q);

  // Basic queue manipulation
  bool queue_empty                (int prio);
  void queue_add_front            (int prio, VM_Operation *op);
  void queue_add_back             (int prio, VM_Operation *op);
  VM_Operation* queue_remove_front(int prio);
  void queue_oops_do(int queue, OopClosure* f);
  void drain_list_oops_do(OopClosure* f);
  VM_Operation* queue_drain(int prio);
  // lock-free query: may return the wrong answer but must not break
  bool queue_peek(int prio) { return _queue_length[prio] > 0; }

 public:
  VMOperationQueue();

  // Highlevel operations. Encapsulates policy
  bool add(VM_Operation *op);
  VM_Operation* remove_next();                        // Returns next or null
  VM_Operation* remove_next_at_safepoint_priority()   { return queue_remove_front(SafepointPriority); }
  VM_Operation* drain_at_safepoint_priority() { return queue_drain(SafepointPriority); }
  void set_drain_list(VM_Operation* list) { _drain_list = list; }
  bool peek_at_safepoint_priority() { return queue_peek(SafepointPriority); }

  // GC support
  void oops_do(OopClosure* f);

  void verify_queue(int prio) PRODUCT_RETURN;
};


//
// A single VMThread (the primordial thread) spawns all other threads
// and is itself used by other threads to offload heavy vm operations
// like scavenge, garbage_collect etc.
//

// 表示一个特殊的专门用来执行比较耗时的VM_Operation的原生线程，VMThread线程在整个JAVA进程有且只会有一个。
// 可以想象一下VMThread线程的简单执行过程：不断地轮询某个任务列表并在有任务时依次执行任务。
// 任务执行时，它会根据具体的任务决定是否会暂停整个应用，也就是stop the world，这是不是让我们联想到了我们熟悉的GC过程？
// 是的，我们的ygc以及cmsgc的两个暂停应用的阶段(init_mark和remark)都是由这个线程来执行的，并且都要求暂停整个应用。
// 比如在执行jstack命令时，触发的线程dump过程也是会暂停应用的，只是这个过程一般很快就结束，不会有明显的感觉。
// 另外内存dump的jmap命令，也是会暂停整个应用的，如果使用了-F的参数，其底层也是使用serviceability agent的api来dump的，但是dump内存的速度会明显慢很多。
class VMThread: public NamedThread {
 private:
    // 线程的优先级
  static ThreadPriority _current_priority;

    // 是否应该终止
  static bool _should_terminate;
    // 是否终止
  static bool _terminated;
  static bool _gclog_reentry;
    // 终止动作对应的锁
  static Monitor * _terminate_lock;
    // 累计的执行VM_Operation的耗时
  static PerfCounter* _perf_accumulated_vm_operation_time;

  void evaluate_operation(VM_Operation* op);
 public:
  // Constructor
  VMThread();

  // Tester
  bool is_VM_thread() const                      { return true; }
  bool is_GC_thread() const                      { return true; }

  // The ever running loop for the VMThread
  void loop();

  // Called to stop the VM thread
  static void wait_for_vm_thread_exit();
  static bool should_terminate()                  { return _should_terminate; }
  static bool is_terminated()                     { return _terminated == true; }
  static bool is_gclog_reentry()                  { return _gclog_reentry; }
  static void set_gclog_reentry(bool reentry)     { _gclog_reentry = reentry; }

  // Execution of vm operation
  static void execute(VM_Operation* op);

  // Returns the current vm operation if any.
  static VM_Operation* vm_operation()             { return _cur_vm_operation;   }

  // Returns the single instance of VMThread.
  static VMThread* vm_thread()                    { return _vm_thread; }

  // GC support
  void oops_do(OopClosure* f, CLDClosure* cld_f, CodeBlobClosure* cf);

  void verify();

  // Performance measurement
  static PerfCounter* perf_accumulated_vm_operation_time()               { return _perf_accumulated_vm_operation_time; }

  // Entry for starting vm thread
  virtual void run();

  // Creations/Destructions
  // 用来创建和销毁唯一的VMThread实例
  static void create();
  static void destroy();

 private:
  // VM_Operation support
  // 当前执行的VM operation
  static VM_Operation*     _cur_vm_operation;   // Current VM operation
    // 缓存待执行的VM operation 队列
  static VMOperationQueue* _vm_queue;           // Queue (w/ policy) of VM operations

  // Pointer to single-instance of VM thread
  // 唯一的VMThread实例
  static VMThread*     _vm_thread;
};

#endif // SHARE_VM_RUNTIME_VMTHREAD_HPP
