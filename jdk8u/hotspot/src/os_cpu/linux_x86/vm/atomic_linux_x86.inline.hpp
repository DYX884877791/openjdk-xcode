/*
 * Copyright (c) 1999, 2013, Oracle and/or its affiliates. All rights reserved.
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

#ifndef OS_CPU_LINUX_X86_VM_ATOMIC_LINUX_X86_INLINE_HPP
#define OS_CPU_LINUX_X86_VM_ATOMIC_LINUX_X86_INLINE_HPP

#include "runtime/atomic.hpp"
#include "runtime/os.hpp"
#include "vm_version_x86.hpp"

// Implementation of class atomic

inline void Atomic::store    (jbyte    store_value, jbyte*    dest) { *dest = store_value; }
inline void Atomic::store    (jshort   store_value, jshort*   dest) { *dest = store_value; }
inline void Atomic::store    (jint     store_value, jint*     dest) { *dest = store_value; }
inline void Atomic::store_ptr(intptr_t store_value, intptr_t* dest) { *dest = store_value; }
inline void Atomic::store_ptr(void*    store_value, void*     dest) { *(void**)dest = store_value; }

inline void Atomic::store    (jbyte    store_value, volatile jbyte*    dest) { *dest = store_value; }
inline void Atomic::store    (jshort   store_value, volatile jshort*   dest) { *dest = store_value; }
inline void Atomic::store    (jint     store_value, volatile jint*     dest) { *dest = store_value; }
inline void Atomic::store_ptr(intptr_t store_value, volatile intptr_t* dest) { *dest = store_value; }
inline void Atomic::store_ptr(void*    store_value, volatile void*     dest) { *(void* volatile *)dest = store_value; }


// Adding a lock prefix to an instruction on MP machine
/**
 * 效果mp为0时会执行lock;指令,否则不会
 * 解释: mp值判断是否添加lock前缀, 多核处理器需要, 单核不需要. mp值表示是否为多核处理器.
 * 带有lock前缀的指令在执行期间会锁住总线，使得其它处理器暂时无法通过总线访问内存.
 */
#define LOCK_IF_MP(mp) "cmp $0, " #mp "; je 1f; lock; 1: "

inline jint     Atomic::add    (jint     add_value, volatile jint*     dest) {
  jint addend = add_value;
  int mp = os::is_MP();
  __asm__ volatile (  LOCK_IF_MP(%3) "xaddl %0,(%2)"
                    : "=r" (addend)
                    : "0" (addend), "r" (dest), "r" (mp)
                    : "cc", "memory");
  return addend + add_value;
}

inline void Atomic::inc    (volatile jint*     dest) {
  int mp = os::is_MP();
  __asm__ volatile (LOCK_IF_MP(%1) "addl $1,(%0)" :
                    : "r" (dest), "r" (mp) : "cc", "memory");
}

inline void Atomic::inc_ptr(volatile void*     dest) {
  inc_ptr((volatile intptr_t*)dest);
}

inline void Atomic::dec    (volatile jint*     dest) {
  int mp = os::is_MP();
  __asm__ volatile (LOCK_IF_MP(%1) "subl $1,(%0)" :
                    : "r" (dest), "r" (mp) : "cc", "memory");
}

inline void Atomic::dec_ptr(volatile void*     dest) {
  dec_ptr((volatile intptr_t*)dest);
}

inline jint     Atomic::xchg    (jint     exchange_value, volatile jint*     dest) {
  __asm__ volatile (  "xchgl (%2),%0"
                    : "=r" (exchange_value)
                    : "0" (exchange_value), "r" (dest)
                    : "memory");
  return exchange_value;
}

inline void*    Atomic::xchg_ptr(void*    exchange_value, volatile void*     dest) {
  return (void*)xchg_ptr((intptr_t)exchange_value, (volatile intptr_t*)dest);
}


/**
 * 其实cmpxchg实现的原子操作原理早已被熟知：
 *
 * cmpxchg(void* ptr, int old, int new)，如果ptr和old的值一样，则把new写到ptr内存，否则返回ptr的值，整个操作是原子的。在Intel平台下，会用lock cmpxchg来实现，这里的lock个人理解是锁住内存总线，这样如果有另一个线程想访问ptr的内存，就会被block住。
 *
 * 好了，让我们来看Linux Kernel中的cmpxchg，在include/asm-i386/cmpxchg.h中声明了函数原型：
 * cmpxchg(void *ptr, unsigned long old, unsigned long new);
 * 具体实现为：
 * #define cmpxchg(ptr,o,n)\
 *  ((__typeof__(*(ptr))) __cmpxchg((ptr),(unsigned long)(o),\
 *                  (unsigned long)(n),sizeof(*(ptr))))
 * 很明显，这个函数就是调用了__cmpxchg。
 *
 * static inline unsigned long __cmpxchg(volatile void *ptr, unsigned long old,
 *                       unsigned long new, int size)
 * {
 *     unsigned long prev;
 *     switch (size) {
 *     case 1:
 *         __asm__ __volatile__(LOCK_PREFIX "cmpxchgb %b1,%2"
 *                      : "=a"(prev)
 *                      : "q"(new), "m"(*__xg(ptr)), "0"(old)
 *                      : "memory");
 *         return prev;
 *     case 2:
 *         __asm__ __volatile__(LOCK_PREFIX "cmpxchgw %w1,%2"
 *                      : "=a"(prev)
 *                      : "r"(new), "m"(*__xg(ptr)), "0"(old)
 *                      : "memory");
 *         return prev;
 *     case 4:
 *         __asm__ __volatile__(LOCK_PREFIX "cmpxchgl %1,%2"
 *                      : "=a"(prev)
 *                      : "r"(new), "m"(*__xg(ptr)), "0"(old)
 *                      : "memory");
 *         return prev;
 *     }
 *     return old;
 * }
 *
 * 以最为常用的4字节交换为例，主要的操作就是汇编指令cmpxchgl %1,%2，注意一下其中的%2，也就是后面的"m"(*__xg(ptr))。
 * __xg是在这个文件中定义的宏：
 * struct __xchg_dummy { unsigned long a[100]; };
 * #define __xg(x) ((struct __xchg_dummy *)(x))
 * 那么%2经过预处理，展开就是"m"(*((struct __xchg_dummy *)(ptr)))，这种做法，就可以达到在cmpxchg中的%2是一个地址，就是ptr指向的地址。如果%2是"m"(ptr)，那么指针本身的值就出现在cmpxchg指令中。
 *
 *
 * 来看cmpxchgl，在Intel开发文档上说：
 *
 * 0F B1/r        CMPXCHG r/m32, r32           MR Valid Valid*          Compare EAX with r/m32. If equal, ZF is set
 *                                                                                                      and r32 is loaded into r/m32. Else, clear ZF
 *                                                                                                      and load r/m32 into EAX.
 *
 * 翻译一下：
 *
 * 比较eax和目的操作数(第一个操作数)的值，如果相同，ZF标志被设置，同时源操作数(第二个操作)的值被写到目的操作数，否则，清ZF标志，并且把目的操作数的值写回eax。
 *
 * 好了，把上面这句话套在cmpxchg上就是：
 *
 * 比较_old和(*__ptr)的值，如果相同，ZF标志被设置，同时_new的值被写到(*__ptr)，否则，清ZF标志，并且把(*__ptr)的值写回_old。
 *
 * 很明显，符合我们对cmpxchg的理解。
 *
 * 换成AT&T的格式就是：
 * cmpxchg %ecx, %ebx；如果EAX与EBX相等，则ECX送EBX且ZF置1；否则EBX送ECX，且ZF清0
 *
 * 在上述例子中，eax就是old，ebx就是ptr指向的内容，ecx就是new。所以cmpxchg指令的操作就是：如果old等于ptr指向的内容，那么就把new写入到ptr中，返回old(%eax没有改变过，一直是old)，这部分和cmpxchg函数的原意是符合的；
 * 如果old不等于ptr指向的内容，那么ptr的内容写入new(%ecx)中，返回old(%eax没有改变过，一直是old)，这明显不符合cmpxchg函数的意思。对此是大惑不解，后来经过Google才知道，那份资料有错。正解是：
 *
 * cmpxchg %ecx, %ebx；如果EAX与EBX相等，则ECX送EBX且ZF置1；否则EBX送EAX，且ZF清0
 *
 * 也就是说，在old和ptr指向的内容不相等的时候，将ptr的内容写入eax中，这样，ptr的内容就会返回给cmpxchg函数的调用者。这样就和原意相符合了。
 *
 *
 * 内联汇编的语法格式
 * __asm__　volatile("Instruction List" //要执行的指令
 * : Output //要写出的值
 * : Input //要读取的值
 * : Clobber/Modify); //可能会对哪些寄存器或内存进行修改
 *
 * output operands和input operands指定参数，它们从左到右依次排列，用','分割，编号从0开始。以cmpxchg汇编为例，(__ret)对应0，(*__ptr)对应1，(_new)对应2，(_old)对应3，如果在汇编中用到"%2"，那么就是指代_new，"%1"指代(*__ptr)。
 * "=a"是说要把结果写到__ret中，而且要使用eax寄存器，所以最后写结果的时候是的操作是mov eax, ret (eax==>__ret)。"r" (_new)是要把_new的值读到一个通用寄存器中使用。
 * 在cmpxchg中，注意"0"(_old)，这个是困惑我的地方，它像告诉你(_old)和第0号操作数使用相同的寄存器或者内存，即(_old)的存储在和0号操作数一样的地方。在cmpxchg中，就是说_old和__ret使用一样的寄存器，而__ret使用的寄存器是eax，所以_old也用eax。
 *
 *
 * __asm__表示汇编的开始, volatile表示禁止编译器优化
 * "cc"代表可能会修改标志寄存器(EFLAGS)
 * "memory"代表会修改内存
 * %1代表参数占位符, 下标从0开始, %1的表达式为"r"(exchange_value)
 * (%3)代表参数占位符, 下标从0开始,()代表实际值, (%3)表达式为"r"(dest)就是随机出来的寄存器的值
 * "=a" (exchange_value) 第0个参数, 等号(=)表示当前输出表达式的属性为只写, a表示寄存器EAX/AX/AL的简写.
 * "r"(exchange_value) 第1个参数, r表示寄存器.
 * "r"约束指明gcc使用任一可用的寄存器
 *
 * LOCK_IF_MP(%4) 是个内联函数
 *
 * 翻译汇编代码
 * 根据以上规则手工将内联汇编翻译为汇编代码
 * 此时先明确该方法中几个传入变量的值为多少:
 *
 * exchange_value=11, dest=obj的指针[10], compare_value=10, mp=1(当前为多核处理器)
 * // mov指令作用: mov 读取数据,数据写入位置
 * mov 11,ecx; //赋值exchange_value到ECX
 * mov 10,eax; //赋值compare_value到EAX
 * mov offset obj指针,edx; //赋值obj指针的地址(如00491000)到EDX
 * mov 1,ebx; //mp==1标示多核处理器
 * cmp $0,ebx; //比较ebx==0
 * je 1f; //如果成立跳转到标记为1:的位置
 * lock; // 多核处理器需要加lock指令保证下一句指令的内存操作被主线锁定,其他核不能操作
 * 1: cmpxchg ecx, dword ptr ds:[edx]; //CPU级别的比较赋值
 * mov eax,$exchange_value; //最后将eax值写到exchange_value中,会用于判断是否复制成功
 *
 * cmpxchg执行过后会由两种结果
 * EAX比较ECX
 * 1.相等: obj指针指向的值从10变为11, eax=10(开始从compare_value读入的)
 * 2.不相等: eax=obj指针指向的值, 因为只有obj指针指向的值已经不是10了才会与eax的10不相等
 * 汇编最后会将eax的值写到exchange_value中
 *
 *
 *  这里ecx,edx,ebx因为表达式是"r", 在实际运算中是随机选择可用的寄存器, 这里翻译是为了方便看, 实际并不固定.
 *   只有eax, 读和写都用了表达式"a"指定了从哪读,写到哪.
 *   要了解这个只需要了解到每个CPU核都有4个通用数据寄存器EAX,EBX,ECX,EDX即可, CPU还有其他类型寄存器, 但我认为这里不需要了解这么多.
 *
 */
inline jint     Atomic::cmpxchg    (jint     exchange_value, volatile jint*     dest, jint     compare_value) {
    // 第一步调用了os::is_MP()函数，该函数定义在os.hpp中，主要是判断当前环境是否为多处理器环境：
  int mp = os::is_MP();
    // 这是C++内联汇编写法, 也就是汇编语言在C++中的内联汇编语法写法
    // 使用__asm__ volatile嵌入汇编代码片段，一个内联汇编表达式分为四个部分，以：进行分隔。
    // 使用__asm__来声明表达式，volatile则保证指令不会被gcc优化影响。cmpxchgl指令则是x86的比较并交换指令，如果是多处理器会使用lock前缀，关于lock前缀，其可以达到一个内存屏障的效果，也可以参照intel手册。
  __asm__ volatile (LOCK_IF_MP(%4) "cmpxchgl %1,(%3)"
                    : "=a" (exchange_value)
                    : "r" (exchange_value), "a" (compare_value), "r" (dest), "r" (mp)
                    : "cc", "memory");
  return exchange_value;
}

#ifdef AMD64
inline void Atomic::store    (jlong    store_value, jlong*    dest) { *dest = store_value; }
inline void Atomic::store    (jlong    store_value, volatile jlong*    dest) { *dest = store_value; }

inline intptr_t Atomic::add_ptr(intptr_t add_value, volatile intptr_t* dest) {
  intptr_t addend = add_value;
  bool mp = os::is_MP();
  __asm__ __volatile__ (LOCK_IF_MP(%3) "xaddq %0,(%2)"
                        : "=r" (addend)
                        : "0" (addend), "r" (dest), "r" (mp)
                        : "cc", "memory");
  return addend + add_value;
}

inline void*    Atomic::add_ptr(intptr_t add_value, volatile void*     dest) {
  return (void*)add_ptr(add_value, (volatile intptr_t*)dest);
}

inline void Atomic::inc_ptr(volatile intptr_t* dest) {
  bool mp = os::is_MP();
  __asm__ __volatile__ (LOCK_IF_MP(%1) "addq $1,(%0)"
                        :
                        : "r" (dest), "r" (mp)
                        : "cc", "memory");
}

inline void Atomic::dec_ptr(volatile intptr_t* dest) {
  bool mp = os::is_MP();
  __asm__ __volatile__ (LOCK_IF_MP(%1) "subq $1,(%0)"
                        :
                        : "r" (dest), "r" (mp)
                        : "cc", "memory");
}

inline intptr_t Atomic::xchg_ptr(intptr_t exchange_value, volatile intptr_t* dest) {
  __asm__ __volatile__ ("xchgq (%2),%0"
                        : "=r" (exchange_value)
                        : "0" (exchange_value), "r" (dest)
                        : "memory");
  return exchange_value;
}

inline jlong    Atomic::cmpxchg    (jlong    exchange_value, volatile jlong*    dest, jlong    compare_value) {
  bool mp = os::is_MP();
  __asm__ __volatile__ (LOCK_IF_MP(%4) "cmpxchgq %1,(%3)"
                        : "=a" (exchange_value)
                        : "r" (exchange_value), "a" (compare_value), "r" (dest), "r" (mp)
                        : "cc", "memory");
  return exchange_value;
}

inline intptr_t Atomic::cmpxchg_ptr(intptr_t exchange_value, volatile intptr_t* dest, intptr_t compare_value) {
  return (intptr_t)cmpxchg((jlong)exchange_value, (volatile jlong*)dest, (jlong)compare_value);
}

inline void*    Atomic::cmpxchg_ptr(void*    exchange_value, volatile void*     dest, void*    compare_value) {
  return (void*)cmpxchg((jlong)exchange_value, (volatile jlong*)dest, (jlong)compare_value);
}

inline jlong Atomic::load(volatile jlong* src) { return *src; }

#else // !AMD64

inline intptr_t Atomic::add_ptr(intptr_t add_value, volatile intptr_t* dest) {
  return (intptr_t)Atomic::add((jint)add_value, (volatile jint*)dest);
}

inline void*    Atomic::add_ptr(intptr_t add_value, volatile void*     dest) {
  return (void*)Atomic::add((jint)add_value, (volatile jint*)dest);
}


inline void Atomic::inc_ptr(volatile intptr_t* dest) {
  inc((volatile jint*)dest);
}

inline void Atomic::dec_ptr(volatile intptr_t* dest) {
  dec((volatile jint*)dest);
}

inline intptr_t Atomic::xchg_ptr(intptr_t exchange_value, volatile intptr_t* dest) {
  return (intptr_t)xchg((jint)exchange_value, (volatile jint*)dest);
}

extern "C" {
  // defined in linux_x86.s
  jlong _Atomic_cmpxchg_long(jlong, volatile jlong*, jlong, bool);
  void _Atomic_move_long(volatile jlong* src, volatile jlong* dst);
}

inline jlong    Atomic::cmpxchg    (jlong    exchange_value, volatile jlong*    dest, jlong    compare_value) {
  return _Atomic_cmpxchg_long(exchange_value, dest, compare_value, os::is_MP());
}

inline intptr_t Atomic::cmpxchg_ptr(intptr_t exchange_value, volatile intptr_t* dest, intptr_t compare_value) {
  return (intptr_t)cmpxchg((jint)exchange_value, (volatile jint*)dest, (jint)compare_value);
}

inline void*    Atomic::cmpxchg_ptr(void*    exchange_value, volatile void*     dest, void*    compare_value) {
  return (void*)cmpxchg((jint)exchange_value, (volatile jint*)dest, (jint)compare_value);
}

inline jlong Atomic::load(volatile jlong* src) {
  volatile jlong dest;
  _Atomic_move_long(src, &dest);
  return dest;
}

inline void Atomic::store(jlong store_value, jlong* dest) {
  _Atomic_move_long((volatile jlong*)&store_value, (volatile jlong*)dest);
}

inline void Atomic::store(jlong store_value, volatile jlong* dest) {
  _Atomic_move_long((volatile jlong*)&store_value, dest);
}

#endif // AMD64

#endif // OS_CPU_LINUX_X86_VM_ATOMIC_LINUX_X86_INLINE_HPP
