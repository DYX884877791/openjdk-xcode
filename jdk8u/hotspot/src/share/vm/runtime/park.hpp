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

#ifndef SHARE_VM_RUNTIME_PARK_HPP
#define SHARE_VM_RUNTIME_PARK_HPP

#include "utilities/debug.hpp"
#include "utilities/globalDefinitions.hpp"
/*
 * Parker和ParkEvent的区别
 * 在os_bsd.cpp中，存在两种形式的park/unpark功能：
 * 1、PlatformEvent实现的park，unpark方法：
 *      PlatformEvent::park();
 *      PlatformEvent::park(jlong millis);
 *      PlatformEvent::unpark()
 * 2、Parker实现的park，unpark方法：
 *      Parker::park(bool isAbsolute, jlong time);
 *      Parker::unpark();
 * 它们的区别可以在park.hpp的源码注释中看到：
 * 意思是说ParkEvent是用于java级别的synchronize关键字，Parker是JSR166来的并发工具集合，后面会统一使用ParkEvent。
 * ParkerEvent继承了PlatformEvent。基类PlatformEvent是特定于平台的，而ParkEvent则是平台无关的。
 * Parker继承自PlatformParker。
 *
 * ParkerEvent中的park，unpark方法用于实现Java的object.wait()方法和object.notify()方法;
 * Parker中的park，unpark方法用于实现Java的Locksupprt.park()方法和Locksupprt.unpark()方法;
 *
 * Per-thread blocking support for JSR166. See the Java-level
 * Documentation for rationale. Basically, park acts like wait, unpark
 * like notify.
 *
 * 6271289 --
 * To avoid errors where an os thread expires but the JavaThread still
 * exists, Parkers are immortal (type-stable) and are recycled across
 * new threads.  This parallels the ParkEvent implementation.
 * Because park-unpark allow spurious wakeups it is harmless if an
 * unpark call unparks a new thread using the old Parker reference.
 *
 * In the future we'll want to think about eliminating Parker and using
 * ParkEvent instead.  There's considerable duplication between the two
 * services.
 *
 */

// Parker 继承自 os::PlatformParker，也就是说，由各个平台具体实现。比如：hotspot/src/os/linux/vm/os_linux.hpp->PlatformParker
// 注意Parker是JavaThread的一个实例属性，Unsafe中park和unpark方法都是针对当前线程，即不存在两个不同的线程访问同一个Parker实例的情形，
// 但是存在同一个Parker的park/unpark方法在同一个线程内被多次调用。
class Parker : public os::PlatformParker {
private:
    // 重要变量，通过 0/1 表示是否持有许可，决定是否阻塞，用来表示Parker的状态，park方法执行完成为0，unpark方法执行完成为1，其中等于1说是一个非常短暂的状态，一旦线程被唤醒又会将其置为0
  volatile int _counter ;
    // 下一个空闲的Parker
  Parker * FreeNext ;
    // 当前关联的 JavaThread 关联的线程
  JavaThread * AssociatedWith ; // Current association

public:
  Parker() : PlatformParker() {
    _counter       = 0 ;
    FreeNext       = NULL ;
    AssociatedWith = NULL ;
  }
protected:
  ~Parker() { ShouldNotReachHere(); }
public:
  // For simplicity of interface with Java, all forms of park (indefinite,
  // relative, and absolute) are multiplexed into one call.
  void park(bool isAbsolute, jlong time);
  void unpark();

  // Lifecycle operators
  static Parker * Allocate (JavaThread * t) ;
  static void Release (Parker * e) ;
private:
    // 空闲的Parker链表
  static Parker * volatile FreeList ;
    // 操作FreeList的锁
  static volatile int ListLock ;

};

/////////////////////////////////////////////////////////////
//
// ParkEvents are type-stable and immortal.
//
// Lifecycle: Once a ParkEvent is associated with a thread that ParkEvent remains
// associated with the thread for the thread's entire lifetime - the relationship is
// stable. A thread will be associated at most one ParkEvent.  When the thread
// expires, the ParkEvent moves to the EventFreeList.  New threads attempt to allocate from
// the EventFreeList before creating a new Event.  Type-stability frees us from
// worrying about stale Event or Thread references in the objectMonitor subsystem.
// (A reference to ParkEvent is always valid, even though the event may no longer be associated
// with the desired or expected thread.  A key aspect of this design is that the callers of
// park, unpark, etc must tolerate stale references and spurious wakeups).
//
// Only the "associated" thread can block (park) on the ParkEvent, although
// any other thread can unpark a reachable parkevent.  Park() is allowed to
// return spuriously.  In fact park-unpark a really just an optimization to
// avoid unbounded spinning and surrender the CPU to be a polite system citizen.
// A degenerate albeit "impolite" park-unpark implementation could simply return.
// See http://blogs.sun.com/dave for more details.
//
// Eventually I'd like to eliminate Events and ObjectWaiters, both of which serve as
// thread proxies, and simply make the THREAD structure type-stable and persistent.
// Currently, we unpark events associated with threads, but ideally we'd just
// unpark threads.
//
// The base-class, PlatformEvent, is platform-specific while the ParkEvent is
// platform-independent.  PlatformEvent provides park(), unpark(), etc., and
// is abstract -- that is, a PlatformEvent should never be instantiated except
// as part of a ParkEvent.
// Equivalently we could have defined a platform-independent base-class that
// exported Allocate(), Release(), etc.  The platform-specific class would extend
// that base-class, adding park(), unpark(), etc.
//
// A word of caution: The JVM uses 2 very similar constructs:
// 1. ParkEvent are used for Java-level "monitor" synchronization.
// 2. Parkers are used by JSR166-JUC park-unpark.
//
// We'll want to eventually merge these redundant facilities and use ParkEvent.


// ParkEvent的定义也是在park.hpp中，继承自os::PlatformEvent
//  ParkEvent在JavaThread中也是实例属性
class ParkEvent : public os::PlatformEvent {
  private:
    // 链表中下一个空闲的ParkEvent
    ParkEvent * FreeNext ;

    // Current association
    // 关联的线程
    Thread * AssociatedWith ;
    // 下面几个属性都是用来实现JVM自身使用的锁Monitor和Mutex
    intptr_t RawThreadIdentity ;        // LWPID etc
    volatile int Incarnation ;

    // diagnostic : keep track of last thread to wake this thread.
    // this is useful for construction of dependency graphs.
    void * LastWaker ;

  public:
    // MCS-CLH list linkage and Native Mutex/Monitor
    ParkEvent * volatile ListNext ;
    ParkEvent * volatile ListPrev ;
    volatile intptr_t OnList ;
    volatile int TState ;
    volatile int Notified ;             // for native monitor construct
    volatile int IsWaiting ;            // Enqueued on WaitSet


  private:
    // 空闲的ParkEvent链表
    static ParkEvent * volatile FreeList ;
    // 操作FreeList 的锁
    static volatile int ListLock ;

    // It's prudent to mark the dtor as "private"
    // ensuring that it's not visible outside the package.
    // Unfortunately gcc warns about such usage, so
    // we revert to the less desirable "protected" visibility.
    // The other compilers accept private dtors.

  protected:        // Ensure dtor is never invoked
    ~ParkEvent() { guarantee (0, "invariant") ; }

    ParkEvent() : PlatformEvent() {
       AssociatedWith = NULL ;
       FreeNext       = NULL ;
       ListNext       = NULL ;
       ListPrev       = NULL ;
       OnList         = 0 ;
       TState         = 0 ;
       Notified       = 0 ;
       IsWaiting      = 0 ;
    }

    // We use placement-new to force ParkEvent instances to be
    // aligned on 256-byte address boundaries.  This ensures that the least
    // significant byte of a ParkEvent address is always 0.

    void * operator new (size_t sz) throw();
    void operator delete (void * a) ;

  public:
    static ParkEvent * Allocate (Thread * t) ;
    static void Release (ParkEvent * e) ;
} ;

#endif // SHARE_VM_RUNTIME_PARK_HPP
