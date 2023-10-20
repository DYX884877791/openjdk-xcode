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
#include "runtime/thread.hpp"
#include "utilities/slog.hpp"



// Lifecycle management for TSM ParkEvents.
// ParkEvents are type-stable (TSM).
// In our particular implementation they happen to be immortal.
//
// We manage concurrency on the FreeList with a CAS-based
// detach-modify-reattach idiom that avoids the ABA problems
// that would otherwise be present in a simple CAS-based
// push-pop implementation.   (push-one and pop-all)
//
// Caveat: Allocate() and Release() may be called from threads
// other than the thread associated with the Event!
// If we need to call Allocate() when running as the thread in
// question then look for the PD calls to initialize native TLS.
// Native TLS (Win32/Linux/Solaris) can only be initialized or
// accessed by the associated thread.
// See also pd_initialize().
//
// Note that we could defer associating a ParkEvent with a thread
// until the 1st time the thread calls park().  unpark() calls to
// an unprovisioned thread would be ignored.  The first park() call
// for a thread would allocate and associate a ParkEvent and return
// immediately.

volatile int ParkEvent::ListLock = 0 ;
ParkEvent * volatile ParkEvent::FreeList = NULL ;

// 同Parker，这两方法分别用于分配和销毁一个ParkEvent
ParkEvent * ParkEvent::Allocate (Thread * t) {
  // In rare cases -- JVM_RawMonitor* operations -- we can find t == null.
  ParkEvent * ev ;

  // Start by trying to recycle an existing but unassociated
  // ParkEvent from the global free list.
  // Using a spin lock since we are part of the mutex impl.
  // 8028280: using concurrent free list without memory management can leak
  // pretty badly it turns out.
    //获取锁
  Thread::SpinAcquire(&ListLock, "ParkEventFreeListAllocate");
  {
      //将FreeList链表头从链表中移除
    ev = FreeList;
    if (ev != NULL) {
      FreeList = ev->FreeNext;
    }
  }
    //释放锁
  Thread::SpinRelease(&ListLock);

  if (ev != NULL) {
      //如果找到一个空闲的ParkEvent
    guarantee (ev->AssociatedWith == NULL, "invariant") ;
  } else {
    // Do this the hard way -- materialize a new ParkEvent.
      //如果没有找到，则创建一个
    ev = new ParkEvent () ;
    guarantee ((intptr_t(ev) & 0xFF) == 0, "invariant") ;
  }
    //将_Event置为0
  ev->reset() ;                     // courtesy to caller
    //保存关联的线程
  ev->AssociatedWith = t ;          // Associate ev with t
  ev->FreeNext       = NULL ;
  return ev ;
}

void ParkEvent::Release (ParkEvent * ev) {
  if (ev == NULL) return ;
  guarantee (ev->FreeNext == NULL      , "invariant") ;
  ev->AssociatedWith = NULL ;
  // Note that if we didn't have the TSM/immortal constraint, then
  // when reattaching we could trim the list.
    //获取锁
  Thread::SpinAcquire(&ListLock, "ParkEventFreeListRelease");
  {
      //归还到FreeList链表中
    ev->FreeNext = FreeList;
    FreeList = ev;
  }
    //释放锁
  Thread::SpinRelease(&ListLock);
}

// Override operator new and delete so we can ensure that the
// least significant byte of ParkEvent addresses is 0.
// Beware that excessive address alignment is undesirable
// as it can result in D$ index usage imbalance as
// well as bank access imbalance on Niagara-like platforms,
// although Niagara's hash function should help.

void * ParkEvent::operator new (size_t sz) throw() {
  return (void *) ((intptr_t (AllocateHeap(sz + 256, mtInternal, CALLER_PC)) + 256) & -256) ;
}

void ParkEvent::operator delete (void * a) {
  // ParkEvents are type-stable and immortal ...
  ShouldNotReachHere();
}


// 6399321 As a temporary measure we copied & modified the ParkEvent::
// allocate() and release() code for use by Parkers.  The Parker:: forms
// will eventually be removed as we consolide and shift over to ParkEvents
// for both builtin synchronization and JSR166 operations.

volatile int Parker::ListLock = 0 ;
Parker * volatile Parker::FreeList = NULL ;

//  Allocate方法会创建一个Parker，主要是线程创建时调用
Parker * Parker::Allocate (JavaThread * t) {
  slog_debug("进入hotspot/src/share/vm/runtime/park.cpp中的Parker::Allocate函数...");
  guarantee (t != NULL, "invariant") ;
  Parker * p ;

  // Start by trying to recycle an existing but unassociated
  // Parker from the global free list.
  // 8028280: using concurrent free list without memory management can leak
  // pretty badly it turns out.
    //获取锁ListLock
  Thread::SpinAcquire(&ListLock, "ParkerFreeListAllocate");
  {
      //获取链表头
    p = FreeList;
    if (p != NULL) {
        //将链表头从链表中移除
      FreeList = p->FreeNext;
    }
  }
    //释放锁
  Thread::SpinRelease(&ListLock);

  if (p != NULL) {
      //有空闲的Parker
    guarantee (p->AssociatedWith == NULL, "invariant") ;
  } else {
    // Do this the hard way -- materialize a new Parker..
      //没有空闲的，重新创建一个
    p = new Parker() ;
  }
    //保存关联的线程
  p->AssociatedWith = t ;          // Associate p with t
  p->FreeNext       = NULL ;
  return p ;
}


void Parker::Release (Parker * p) {
  if (p == NULL) return ;
  guarantee (p->AssociatedWith != NULL, "invariant") ;
  guarantee (p->FreeNext == NULL      , "invariant") ;
    //关联的线程置为NULL
  p->AssociatedWith = NULL ;

    //获取锁
  Thread::SpinAcquire(&ListLock, "ParkerFreeListRelease");
  {
      //将p插入到链表头
    p->FreeNext = FreeList;
    FreeList = p;
  }
    //释放锁
  Thread::SpinRelease(&ListLock);
}

