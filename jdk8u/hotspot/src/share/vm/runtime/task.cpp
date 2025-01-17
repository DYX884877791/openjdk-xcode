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
#include "memory/allocation.hpp"
#include "runtime/init.hpp"
#include "runtime/task.hpp"
#include "runtime/thread.inline.hpp"
#include "runtime/timer.hpp"
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

int PeriodicTask::_num_tasks = 0;
PeriodicTask* PeriodicTask::_tasks[PeriodicTask::max_tasks];
#ifndef PRODUCT
elapsedTimer PeriodicTask::_timer;
int PeriodicTask::_intervalHistogram[PeriodicTask::max_interval];
int PeriodicTask::_ticks;

void PeriodicTask::print_intervals() {
  if (ProfilerCheckIntervals) {
    for (int i = 0; i < PeriodicTask::max_interval; i++) {
      int n = _intervalHistogram[i];
      if (n > 0) tty->print_cr("%3d: %5d (%4.1f%%)", i, n, 100.0 * n / _ticks);
    }
  }
}
#endif

void PeriodicTask::real_time_tick(int delay_time) {
#ifndef PRODUCT
  if (ProfilerCheckIntervals) {
    _ticks++;
    _timer.stop();
    int ms = (int)(_timer.seconds() * 1000.0);
    _timer.reset();
    _timer.start();
    if (ms >= PeriodicTask::max_interval) ms = PeriodicTask::max_interval - 1;
    _intervalHistogram[ms]++;
  }
#endif

  {
    MutexLockerEx ml(PeriodicTask_lock, Mutex::_no_safepoint_check_flag);
    int orig_num_tasks = _num_tasks;

    for(int index = 0; index < _num_tasks; index++) {
      _tasks[index]->execute_if_pending(delay_time);
      if (_num_tasks < orig_num_tasks) { // task dis-enrolled itself
        index--;  // re-do current slot as it has changed
        orig_num_tasks = _num_tasks;
      }
    }
  }
}

int PeriodicTask::time_to_wait() {
  MutexLockerEx ml(PeriodicTask_lock->owned_by_self() ?
                     NULL : PeriodicTask_lock, Mutex::_no_safepoint_check_flag);

  if (_num_tasks == 0) {
    return 0; // sleep until shutdown or a task is enrolled
  }

  int delay = _tasks[0]->time_to_next_interval();
  for (int index = 1; index < _num_tasks; index++) {
    delay = MIN2(delay, _tasks[index]->time_to_next_interval());
  }
  return delay;
}


PeriodicTask::PeriodicTask(size_t interval_time) :
  // 将interval_time赋值给了_interval属性，_interval是一个私有常量，以毫秒为单位，同时_counter设置为0：
  _counter(0), _interval((int) interval_time) {
  // Sanity check the interval time
  assert(_interval >= PeriodicTask::min_interval &&
         _interval %  PeriodicTask::interval_gran == 0,
              "improper PeriodicTask interval time");
}

PeriodicTask::~PeriodicTask() {
  disenroll();
}

/* enroll could be called from a JavaThread, so we have to check for
 * safepoint when taking the lock to avoid deadlocking */
void PeriodicTask::enroll() {
  MutexLockerEx ml(PeriodicTask_lock->owned_by_self() ?
                     NULL : PeriodicTask_lock);

  if (_num_tasks == PeriodicTask::max_tasks) {
    fatal("Overflow in PeriodicTask table");
  }
  _tasks[_num_tasks++] = this;

    // 可以看到最终还是使用了一个WatcherThread对象，WatcherThread是Thread对象的派生类，其定义在thread.hpp中。
  WatcherThread* thread = WatcherThread::watcher_thread();
  if (thread) {
    thread->unpark();
  } else {
    WatcherThread::start();
  }
}

/* disenroll could be called from a JavaThread, so we have to check for
 * safepoint when taking the lock to avoid deadlocking */
void PeriodicTask::disenroll() {
  MutexLockerEx ml(PeriodicTask_lock->owned_by_self() ?
                     NULL : PeriodicTask_lock);

  int index;
  for(index = 0; index < _num_tasks && _tasks[index] != this; index++)
    ;

  if (index == _num_tasks) {
    return;
  }

  _num_tasks--;

  for (; index < _num_tasks; index++) {
    _tasks[index] = _tasks[index+1];
  }
}
