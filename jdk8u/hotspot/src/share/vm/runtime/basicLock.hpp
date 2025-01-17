/*
 * Copyright (c) 1998, 2010, Oracle and/or its affiliates. All rights reserved.
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

#ifndef SHARE_VM_RUNTIME_BASICLOCK_HPP
#define SHARE_VM_RUNTIME_BASICLOCK_HPP

#include "oops/markOop.hpp"
#include "runtime/handles.hpp"
#include "utilities/top.hpp"

class BasicLock VALUE_OBJ_CLASS_SPEC {
  friend class VMStructs;
 private:
    // displaced_header属性用于保存锁对象oop的原始对象头，即无锁状态下的对象头，但是在synchronized嵌套的情形下displaced_header为NULL，
    // 因为外层synchronized对应的BasicObjectLock已经保存了原始对象头了，此处不需要再保存；
    // 另外，如果某个轻量级锁膨胀成重量级锁了，则displaced_header会被置为unused_mark，因为重量级锁本身会保存锁对象oop的原始对象头。
    /**
     * public static void main(String[] args) {
     *       Object lock = new Object();
     *       synchronized (lock) {
     *           System.out.println("start");
     *           synchronized (lock) {
     *               System.out.println("inner");
     *           }
     *           System.out.println("end");
     *       }
     *   }
     *
     */
  volatile markOop _displaced_header;
 public:
  markOop      displaced_header() const               { return _displaced_header; }
  void         set_displaced_header(markOop header)   { _displaced_header = header; }

  void print_on(outputStream* st) const;

  // move a basic lock (used during deoptimization
  void move_to(oop obj, BasicLock* dest);

  static int displaced_header_offset_in_bytes()       { return offset_of(BasicLock, _displaced_header); }
};

// A BasicObjectLock associates a specific Java object with a BasicLock.
// It is currently embedded in an interpreter frame.

// Because some machines have alignment restrictions on the control stack,
// the actual space allocated by the interpreter may include padding words
// after the end of the BasicObjectLock.  Also, in order to guarantee
// alignment of the embedded BasicLock objects on such machines, we
// put the embedded BasicLock at the beginning of the struct.

// 当锁对象升级到轻量级锁的时候，线程运行的时候在自己栈帧中分配的一块空间，用于存储锁对象头中mark word原始信息，
// 拷贝过来的mark word也叫作displaced mark word，这块空间就是Lock Record，用于保存锁记录信息。
//
// 为什么要将锁对象的mark word拷贝到持有轻量级锁的线程的栈帧中？这样获取锁之后，还能保存锁对象的原始的hashcode、分代信息等。
// 这也是轻量级锁和偏向锁的区别之一，否则如果只是和偏向锁类似，将锁对象通过一个线程ID标识当前被哪一个线程持有，那么在释放锁的时候只需要将这个线程ID清空就可以。
// 但是如何保存锁对象的原始信息，比如hashcode和分代信息等，那就只有升级为重量级锁了，由监视器去保存这些东西。
//
// 在代码实现锁记录主要由BasicObjectLock和BasicLock实现，一个BasicLock表示保存锁记录；BasicObjectLock关联锁对象和BasicLock。
class BasicObjectLock VALUE_OBJ_CLASS_SPEC {
  friend class VMStructs;
 private:
    // lock属性的地址用于实现轻量级锁，即所谓的Thread ID
  BasicLock _lock;                                    // the lock, must be double word aligned
    // obj属性用于保存关联的锁对象oop
  oop       _obj;                                     // object holds the lock;

 public:
  // Manipulation
  oop      obj() const                                { return _obj;  }
  void set_obj(oop obj)                               { _obj = obj; }
  BasicLock* lock()                                   { return &_lock; }

  // Note: Use frame::interpreter_frame_monitor_size() for the size of BasicObjectLocks
  //       in interpreter activation frames since it includes machine-specific padding.
  static int size()                                   { return sizeof(BasicObjectLock)/wordSize; }

  // GC support
  void oops_do(OopClosure* f) { f->do_oop(&_obj); }

  static int obj_offset_in_bytes()                    { return offset_of(BasicObjectLock, _obj);  }
  static int lock_offset_in_bytes()                   { return offset_of(BasicObjectLock, _lock); }
};


#endif // SHARE_VM_RUNTIME_BASICLOCK_HPP
