/*
 * Copyright (c) 2003, 2013, Oracle and/or its affiliates. All rights reserved.
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

#ifndef OS_CPU_LINUX_X86_VM_ORDERACCESS_LINUX_X86_INLINE_HPP
#define OS_CPU_LINUX_X86_VM_ORDERACCESS_LINUX_X86_INLINE_HPP

#include "runtime/atomic.inline.hpp"
#include "runtime/orderAccess.hpp"
#include "runtime/os.hpp"
#include "vm_version_x86.hpp"

// Implementation of class OrderAccess.

inline void OrderAccess::loadload()   { acquire(); }
inline void OrderAccess::storestore() { release(); }
inline void OrderAccess::loadstore()  { acquire(); }
inline void OrderAccess::storeload()  { fence(); }

// 在高版本的openjdk中，编译器屏障代码如下：
// 底层其实就是c++ volatile，不带有任何内存屏障...
// 上述代码是GCC的扩展内联汇编形式，简单解释下compiler_barrier方法中的内容：
//      asm，插入汇编指令；
//      volatile，禁止编译器优化此处的汇编指令；
//      memory，汇编指令修改了内存，要重新读取内存数据。
//static inline void compiler_barrier() {
//    __asm__ volatile ("" : : : "memory");
//}
inline void OrderAccess::acquire() {
  volatile intptr_t local_dummy;
#ifdef AMD64
  __asm__ volatile ("movq 0(%%rsp), %0" : "=r" (local_dummy) : : "memory");
#else
  // C++中volatile是禁止编译器对代码的优化，等于插入编译器级别屏障，但是并不能阻止CPU硬件级别导致的重排，
  // 即只是插入了编译器屏障，但是没有插入内存屏障...
  __asm__ volatile ("movl 0(%%esp),%0" : "=r" (local_dummy) : : "memory");
#endif // AMD64
}

/**
 * 关于这里release、acquire两个方法的语义可见：
 * https://zhuanlan.zhihu.com/p/490465757
 */
inline void OrderAccess::release() {
  // Avoid hitting the same cache-line from
  // different threads.
  volatile jint local_dummy = 0;
}

/**
 * x86-TSO中唯一重排的地方在于StoreBuffer，因为StoreBuffer的存在，核心的写入操作被缓存，无法马上刷新到共享存储中被其他核心观察到，
 * 所以就有了 “ 写 ” 比 “读” 晚执行的直观感受，也可以说是读操作提前了，排到了写操作前
 *
 * 阻止这种重排的方法是 使用带 lock 前缀的指令或者XCHG指令，或MFENCE指令，将StoreBuffer中的内容刷入到共享存储，以便被其他核心观察到
 */

// fence屏障：x86下只有StoreLoad乱序，通过mfence也可以实现全屏障，但是这个指令在这里被认为不如lock+addl的指令快
// 所以这里使用lock+addl的指令实现，同时还需要compiler_barrier组成完成的fence
inline void OrderAccess::fence() {
    // 首先判断处理器是单核还是多核，如果是单核，就没必要每次来使用内存屏障，反而更加消耗资源
  if (os::is_MP()) {
      // 实际上X86架构提供了内存屏障指令lfence，sfence，mfence等，但为什么不使用内存屏障指令呢？原因在fence的注释中：
      // sfence ，实现Store Barrier 会将store buffer中缓存的修改刷入L1 cache中，使得其他cpu核可以观察到这些修改，而且之后的写操作不会被调度到之前，即sfence之前的写操作一定在sfence完成且全局可见；
      // lfence ，实现Load Barrier 会将invalidate queue失效，强制读取入L1 cache中，而且lfence之后的读操作不会被调度到之前，即lfence之前的读操作一定在lfence完成（并未规定全局可见性）；
      // mfence ，实现Full Barrier 同时刷新store buffer和invalidate queue，保证了mfence前后的读写操作的顺序，同时要求mfence之后写操作结果全局可见之前，mfence之前写操作结果全局可见；
      //
      // lock 用来修饰当前指令操作的内存只能由当前CPU使用，若指令不操作内存仍然由用，因为这个修饰会让指令操作本身原子化，而且自带Full Barrier效果；
      // 还有指令比如IO操作的指令、exch等原子交换的指令，任何带有lock前缀的指令以及CPUID等指令都有内存屏障的作用。
      // X86-64下仅支持一种指令重排：Store-Load ，即读操作可能会重排到写操作前面，同时不同线程的写操作并没有保证全局可见，
      // 例子见《Intel® 64 and IA-32 Architectures Software Developer’s Manual》手册8.6.1、8.2.3.7节。
      //
      // 要注意的是这个问题只能用mfence解决，不能靠组合sfence和lfence解决。（用sfence+lfence组合仅可以解决重排问题，但不能解决全局可见性问题，简单理解不如视为sfence和lfence本身也能乱序重拍）
      //
      // X86-64一般情况根本不会需要使用lfence与sfence这两个指令，除非操作Write-Through内存或使用 non-temporal 指令（NT指令，属于SSE指令集），
      // 比如movntdq, movnti, maskmovq，这些指令也使用Write-Through内存策略，通常使用在图形学或视频处理，Linux编程里就需要使用GNU提供的专门的函数
      // （例子见参考资料13：Memory part 5: What programmers can do）。

      //  mfence is sometimes expensive
      //       即mfence指令在性能上的开销较大。
    // always use locked addl since mfence is sometimes expensive
#ifdef AMD64
/**
 * 代码lock; addl $0,0(%%rsp) 其中的addl $0,0(%%rsp) 是把寄存器的值加0，相当于一个空操作（之所以用它，不用空操作专用指令nop，是因为lock前缀不允许配合nop指令使用）

 lock 汇编指令，会锁住操作的缓存行.
lock前缀，会保证某个处理器对共享内存（一般是缓存行cacheline，这里记住缓存行概念，后续重点介绍）的独占使用。它将本处理器缓存写入内存，该写入操作会引起其他处理器或内核对应的缓存失效。
 通过独占内存、使其他处理器缓存失效，达到了“指令重排序无法越过内存屏障”的作用


 在运行java代码时，加上JVM的参数：-XX:+UnlockDiagnosticVMOptions -XX:+PrintAssembly，就可以看到它的汇编输出。
 */
    __asm__ volatile ("lock; addl $0,0(%%rsp)" : : : "cc", "memory");
#else
    __asm__ volatile ("lock; addl $0,0(%%esp)" : : : "cc", "memory");
#endif
  }
}

inline jbyte    OrderAccess::load_acquire(volatile jbyte*   p) { return *p; }
inline jshort   OrderAccess::load_acquire(volatile jshort*  p) { return *p; }
inline jint     OrderAccess::load_acquire(volatile jint*    p) { return *p; }
inline jlong    OrderAccess::load_acquire(volatile jlong*   p) { return Atomic::load(p); }
inline jubyte   OrderAccess::load_acquire(volatile jubyte*  p) { return *p; }
inline jushort  OrderAccess::load_acquire(volatile jushort* p) { return *p; }
inline juint    OrderAccess::load_acquire(volatile juint*   p) { return *p; }
inline julong   OrderAccess::load_acquire(volatile julong*  p) { return Atomic::load((volatile jlong*)p); }
inline jfloat   OrderAccess::load_acquire(volatile jfloat*  p) { return *p; }
inline jdouble  OrderAccess::load_acquire(volatile jdouble* p) { return jdouble_cast(Atomic::load((volatile jlong*)p)); }

inline intptr_t OrderAccess::load_ptr_acquire(volatile intptr_t*   p) { return *p; }
inline void*    OrderAccess::load_ptr_acquire(volatile void*       p) { return *(void* volatile *)p; }
//load_ptr_acquire先将p转换成指针的指针void* const volatile *,然后再取值
inline void*    OrderAccess::load_ptr_acquire(const volatile void* p) { return *(void* const volatile *)p; }

inline void     OrderAccess::release_store(volatile jbyte*   p, jbyte   v) { *p = v; }
inline void     OrderAccess::release_store(volatile jshort*  p, jshort  v) { *p = v; }
inline void     OrderAccess::release_store(volatile jint*    p, jint    v) { *p = v; }
inline void     OrderAccess::release_store(volatile jlong*   p, jlong   v) { Atomic::store(v, p); }
inline void     OrderAccess::release_store(volatile jubyte*  p, jubyte  v) { *p = v; }
inline void     OrderAccess::release_store(volatile jushort* p, jushort v) { *p = v; }
inline void     OrderAccess::release_store(volatile juint*   p, juint   v) { *p = v; }
inline void     OrderAccess::release_store(volatile julong*  p, julong  v) { Atomic::store((jlong)v, (volatile jlong*)p); }
inline void     OrderAccess::release_store(volatile jfloat*  p, jfloat  v) { *p = v; }
inline void     OrderAccess::release_store(volatile jdouble* p, jdouble v) { release_store((volatile jlong *)p, jlong_cast(v)); }

inline void     OrderAccess::release_store_ptr(volatile intptr_t* p, intptr_t v) { *p = v; }
inline void     OrderAccess::release_store_ptr(volatile void*     p, void*    v) { *(void* volatile *)p = v; }

inline void     OrderAccess::store_fence(jbyte*  p, jbyte  v) {
  __asm__ volatile (  "xchgb (%2),%0"
                    : "=q" (v)
                    : "0" (v), "r" (p)
                    : "memory");
}
inline void     OrderAccess::store_fence(jshort* p, jshort v) {
  __asm__ volatile (  "xchgw (%2),%0"
                    : "=r" (v)
                    : "0" (v), "r" (p)
                    : "memory");
}
inline void     OrderAccess::store_fence(jint*   p, jint   v) {
  __asm__ volatile (  "xchgl (%2),%0"
                    : "=r" (v)
                    : "0" (v), "r" (p)
                    : "memory");
}

inline void     OrderAccess::store_fence(jlong*   p, jlong   v) {
#ifdef AMD64
  __asm__ __volatile__ ("xchgq (%2), %0"
                        : "=r" (v)
                        : "0" (v), "r" (p)
                        : "memory");
#else
  *p = v; fence();
#endif // AMD64
}

// AMD64 copied the bodies for the the signed version. 32bit did this. As long as the
// compiler does the inlining this is simpler.
inline void     OrderAccess::store_fence(jubyte*  p, jubyte  v) { store_fence((jbyte*)p,  (jbyte)v);  }
inline void     OrderAccess::store_fence(jushort* p, jushort v) { store_fence((jshort*)p, (jshort)v); }
inline void     OrderAccess::store_fence(juint*   p, juint   v) { store_fence((jint*)p,   (jint)v);   }
inline void     OrderAccess::store_fence(julong*  p, julong  v) { store_fence((jlong*)p,  (jlong)v);  }
inline void     OrderAccess::store_fence(jfloat*  p, jfloat  v) { *p = v; fence(); }
inline void     OrderAccess::store_fence(jdouble* p, jdouble v) { store_fence((jlong*)p, jlong_cast(v)); }

inline void     OrderAccess::store_ptr_fence(intptr_t* p, intptr_t v) {
#ifdef AMD64
  __asm__ __volatile__ ("xchgq (%2), %0"
                        : "=r" (v)
                        : "0" (v), "r" (p)
                        : "memory");
#else
  store_fence((jint*)p, (jint)v);
#endif // AMD64
}

inline void     OrderAccess::store_ptr_fence(void**    p, void*    v) {
#ifdef AMD64
  __asm__ __volatile__ ("xchgq (%2), %0"
                        : "=r" (v)
                        : "0" (v), "r" (p)
                        : "memory");
#else
  store_fence((jint*)p, (jint)v);
#endif // AMD64
}

// Must duplicate definitions instead of calling store_fence because we don't want to cast away volatile.
inline void     OrderAccess::release_store_fence(volatile jbyte*  p, jbyte  v) {
  __asm__ volatile (  "xchgb (%2),%0"
                    : "=q" (v)
                    : "0" (v), "r" (p)
                    : "memory");
}
inline void     OrderAccess::release_store_fence(volatile jshort* p, jshort v) {
  __asm__ volatile (  "xchgw (%2),%0"
                    : "=r" (v)
                    : "0" (v), "r" (p)
                    : "memory");
}
inline void     OrderAccess::release_store_fence(volatile jint*   p, jint   v) {
  __asm__ volatile (  "xchgl (%2),%0"
                    : "=r" (v)
                    : "0" (v), "r" (p)
                    : "memory");
}

inline void     OrderAccess::release_store_fence(volatile jlong*   p, jlong   v) {
#ifdef AMD64
  __asm__ __volatile__ (  "xchgq (%2), %0"
                          : "=r" (v)
                          : "0" (v), "r" (p)
                          : "memory");
#else
  release_store(p, v); fence();
#endif // AMD64
}

inline void     OrderAccess::release_store_fence(volatile jubyte*  p, jubyte  v) { release_store_fence((volatile jbyte*)p,  (jbyte)v);  }
inline void     OrderAccess::release_store_fence(volatile jushort* p, jushort v) { release_store_fence((volatile jshort*)p, (jshort)v); }
inline void     OrderAccess::release_store_fence(volatile juint*   p, juint   v) { release_store_fence((volatile jint*)p,   (jint)v);   }
inline void     OrderAccess::release_store_fence(volatile julong*  p, julong  v) { release_store_fence((volatile jlong*)p,  (jlong)v);  }

inline void     OrderAccess::release_store_fence(volatile jfloat*  p, jfloat  v) { *p = v; fence(); }
inline void     OrderAccess::release_store_fence(volatile jdouble* p, jdouble v) { release_store_fence((volatile jlong*)p, jlong_cast(v)); }

inline void     OrderAccess::release_store_ptr_fence(volatile intptr_t* p, intptr_t v) {
#ifdef AMD64
  __asm__ __volatile__ (  "xchgq (%2), %0"
                          : "=r" (v)
                          : "0" (v), "r" (p)
                          : "memory");
#else
  release_store_fence((volatile jint*)p, (jint)v);
#endif // AMD64
}
inline void     OrderAccess::release_store_ptr_fence(volatile void*     p, void*    v) {
#ifdef AMD64
  __asm__ __volatile__ (  "xchgq (%2), %0"
                          : "=r" (v)
                          : "0" (v), "r" (p)
                          : "memory");
#else
  release_store_fence((volatile jint*)p, (jint)v);
#endif // AMD64
}

#endif // OS_CPU_LINUX_X86_VM_ORDERACCESS_LINUX_X86_INLINE_HPP
