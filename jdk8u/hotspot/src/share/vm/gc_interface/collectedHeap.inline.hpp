/*
 * Copyright (c) 2001, 2020, Oracle and/or its affiliates. All rights reserved.
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

#ifndef SHARE_VM_GC_INTERFACE_COLLECTEDHEAP_INLINE_HPP
#define SHARE_VM_GC_INTERFACE_COLLECTEDHEAP_INLINE_HPP

#include "gc_interface/allocTracer.hpp"
#include "gc_interface/collectedHeap.hpp"
#include "memory/threadLocalAllocBuffer.inline.hpp"
#include "memory/universe.hpp"
#include "oops/arrayOop.hpp"
#include "prims/jvmtiExport.hpp"
#include "runtime/sharedRuntime.hpp"
#include "runtime/thread.inline.hpp"
#include "services/lowMemoryDetector.hpp"
#include "utilities/copy.hpp"

// Inline allocation implementations.

void CollectedHeap::post_allocation_setup_common(KlassHandle klass,
                                                 HeapWord* obj_ptr) {
  post_allocation_setup_no_klass_install(klass, obj_ptr);
  oop obj = (oop)obj_ptr;
#if ! INCLUDE_ALL_GCS
    //设置对象的klass属性
  obj->set_klass(klass());
#else
  // Need a release store to ensure array/class length, mark word, and
  // object zeroing are visible before setting the klass non-NULL, for
  // concurrent collectors.
  obj->release_set_klass(klass());
#endif
}

void CollectedHeap::post_allocation_setup_no_klass_install(KlassHandle klass,
                                                           HeapWord* obj_ptr) {
  oop obj = (oop)obj_ptr;

  assert(obj != NULL, "NULL object pointer");
    //设置对象头
  if (UseBiasedLocking && (klass() != NULL)) {
    obj->set_mark(klass->prototype_header());
  } else {
    // May be bootstrapping
    obj->set_mark(markOopDesc::prototype());
  }
}

inline void send_jfr_allocation_event(KlassHandle klass, HeapWord* obj, size_t size) {
  Thread* t = Thread::current();
  ThreadLocalAllocBuffer& tlab = t->tlab();
  if (obj == tlab.start()) {
    // allocate in new TLAB
    size_t new_tlab_size = tlab.hard_size_bytes();
    AllocTracer::send_allocation_in_new_tlab_event(klass, obj, new_tlab_size, size * HeapWordSize, t);
  } else if (!tlab.in_used(obj)) {
    // allocate outside TLAB
    AllocTracer::send_allocation_outside_tlab_event(klass, obj, size * HeapWordSize, t);
  }
}

// Support for jvmti, dtrace and jfr
inline void post_allocation_notify(KlassHandle klass, oop obj, int size) {
  send_jfr_allocation_event(klass, (HeapWord*)obj, size);
  // support low memory notifications (no-op if not enabled)
  LowMemoryDetector::detect_low_memory_for_collected_pools();

  // support for JVMTI VMObjectAlloc event (no-op if not enabled)
  JvmtiExport::vm_object_alloc_event_collector(obj);

  if (DTraceAllocProbes) {
    // support for Dtrace object alloc event (no-op most of the time)
    if (klass() != NULL && klass()->name() != NULL) {
      SharedRuntime::dtrace_object_alloc(obj, size);
    }
  }
}

void CollectedHeap::post_allocation_setup_obj(KlassHandle klass,
                                              HeapWord* obj_ptr,
                                              int size) {
  post_allocation_setup_common(klass, obj_ptr);
  oop obj = (oop)obj_ptr;
  assert(Universe::is_bootstrapping() ||
         !obj->is_array(), "must not be an array");
  // notify jvmti and dtrace
    //通知jvmti对象创建，如果DTraceAllocProbes为true则打印日志
  post_allocation_notify(klass, obj, size);
}

void CollectedHeap::post_allocation_setup_array(KlassHandle klass,
                                                HeapWord* obj_ptr,
                                                int length) {
  // Set array length before setting the _klass field because a
  // non-NULL klass field indicates that the object is parsable by
  // concurrent GC.
  assert(length >= 0, "length should be non-negative");
    //设置数组长度
  ((arrayOop)obj_ptr)->set_length(length);
    //设置对象头和klass
  post_allocation_setup_common(klass, obj_ptr);
  oop new_obj = (oop)obj_ptr;
    //校验obj是数组
  assert(new_obj->is_array(), "must be an array");
  // notify jvmti and dtrace (must be after length is set for dtrace)
    //发布JVMTI事件，打印dtrace日志
  post_allocation_notify(klass, new_obj, new_obj->size());
}

HeapWord* CollectedHeap::common_mem_allocate_noinit(KlassHandle klass, size_t size, TRAPS) {

  // Clear unhandled oops for memory allocation.  Memory allocation might
  // not take out a lock if from tlab, so clear here.
    //清理当前线程TLAB中未使用的oop
  CHECK_UNHANDLED_OOPS_ONLY(THREAD->clear_unhandled_oops();)

    //判断是否发生异常
  if (HAS_PENDING_EXCEPTION) {
    NOT_PRODUCT(guarantee(false, "Should not allocate with exception pending"));
    return NULL;  // caller does a CHECK_0 too
  }

  HeapWord* result = NULL;
    //如果使用TLAB
  if (UseTLAB) {
      //从TLAB分配对象
    result = allocate_from_tlab(klass, THREAD, size);
    if (result != NULL) {
      assert(!HAS_PENDING_EXCEPTION,
             "Unexpected exception, will result in uninitialized storage");
      return result;
    }
  }
  bool gc_overhead_limit_was_exceeded = false;
    //从Java堆中分配对象内存
  result = Universe::heap()->mem_allocate(size,
                                          &gc_overhead_limit_was_exceeded);
    //分配成功
  if (result != NULL) {
    NOT_PRODUCT(Universe::heap()->
      check_for_non_bad_heap_word_value(result, size));
    assert(!HAS_PENDING_EXCEPTION,
           "Unexpected exception, will result in uninitialized storage");
      //增加当前线程记录已分配的内存大小的属性
    THREAD->incr_allocated_bytes(size * HeapWordSize);

    return result;
  }


    //分配失败
  if (!gc_overhead_limit_was_exceeded) {
    // -XX:+HeapDumpOnOutOfMemoryError and -XX:OnOutOfMemoryError support
      //异常处理，Java heap space表示当前堆内存严重不足
    report_java_out_of_memory("Java heap space");

      //通知JVMTI
    if (JvmtiExport::should_post_resource_exhausted()) {
      JvmtiExport::post_resource_exhausted(
        JVMTI_RESOURCE_EXHAUSTED_OOM_ERROR | JVMTI_RESOURCE_EXHAUSTED_JAVA_HEAP,
        "Java heap space");
    }

      //抛出异常
    THROW_OOP_0(Universe::out_of_memory_error_java_heap());
  } else {
    // -XX:+HeapDumpOnOutOfMemoryError and -XX:OnOutOfMemoryError support
      //同上，异常处理，GC overhead limit exceeded表示执行GC后仍不能有效回收内存导致内存不足
    report_java_out_of_memory("GC overhead limit exceeded");

    if (JvmtiExport::should_post_resource_exhausted()) {
      JvmtiExport::post_resource_exhausted(
        JVMTI_RESOURCE_EXHAUSTED_OOM_ERROR | JVMTI_RESOURCE_EXHAUSTED_JAVA_HEAP,
        "GC overhead limit exceeded");
    }

    THROW_OOP_0(Universe::out_of_memory_error_gc_overhead_limit());
  }
}

HeapWord* CollectedHeap::common_mem_allocate_init(KlassHandle klass, size_t size, TRAPS) {
    // 尝试在Java堆中开辟对象
  HeapWord* obj = common_mem_allocate_noinit(klass, size, CHECK_NULL);
    // 内存清零
  init_obj(obj, size);
  return obj;
}

HeapWord* CollectedHeap::allocate_from_tlab(KlassHandle klass, Thread* thread, size_t size) {
  assert(UseTLAB, "should use UseTLAB");

    //从tlab中分配指定大小的内存
  HeapWord* obj = thread->tlab().allocate(size);
  if (obj != NULL) {
    return obj;
  }
  // Otherwise...
    //走慢速模式从tlab中分配内存
  return allocate_from_tlab_slow(klass, thread, size);
}

void CollectedHeap::init_obj(HeapWord* obj, size_t size) {
  assert(obj != NULL, "cannot initialize NULL object");
  const size_t hs = oopDesc::header_size();
  assert(size >= hs, "unexpected object size");
    //设置GC分代年龄
  ((oop)obj)->set_klass_gap(0);
    //将请求头以外的地方填充
    //将分配的对象内存全部初始化为0
  Copy::fill_to_aligned_words(obj + hs, size - hs);
}

// 对象创建的核心逻辑
oop CollectedHeap::obj_allocate(KlassHandle klass, int size, TRAPS) {
  debug_only(check_for_valid_allocation_state());
    //检查Java堆是否正在gc
  assert(!Universe::heap()->is_gc_active(), "Allocation during gc not allowed");
  assert(size >= 0, "int won't convert to size_t");
    // 尝试在Java堆中开辟对象
    //分配对象内存并完成初始化
  HeapWord* obj = common_mem_allocate_init(klass, size, CHECK_NULL);
    // 开辟好Java对象后，做初始化工作，比如：设置对象头、设置Klass指针。
    //设置对象头和klass属性
  post_allocation_setup_obj(klass, obj, size);
    //检查分配的内存是否正常，生产环境不执行
  NOT_PRODUCT(Universe::heap()->check_for_bad_heap_word_value(obj, size));
  return (oop)obj;
}

// array_allocate是与之对应的用来分配某个Klass的oop数组的
// 注意这里的size就是已经计算过的目标数组需要的内存大小
oop CollectedHeap::array_allocate(KlassHandle klass,
                                  int size,
                                  int length,
                                  TRAPS) {
  debug_only(check_for_valid_allocation_state());
  assert(!Universe::heap()->is_gc_active(), "Allocation during gc not allowed");
  assert(size >= 0, "int won't convert to size_t");
    //跟obj_allocate调用一样的方法申请指定大小的内存，如果klass未完成初始化则初始化klass
  HeapWord* obj = common_mem_allocate_init(klass, size, CHECK_NULL);
    //跟obj_allocate不一样，obj_allocate调用的是post_allocation_setup_obj
  post_allocation_setup_array(klass, obj, length);
  NOT_PRODUCT(Universe::heap()->check_for_bad_heap_word_value(obj, size));
  return (oop)obj;
}

// array_allocate_nozero方法的实现array_allocate基本一致，最大的区别在于array_allocate_nozero申请到的内存是未完成初始化的，
// 即还未完成实际的内存分配，更适合一些大数组的分配，在数组元素的填充即实际的使用过程中再逐步完成实际内存的分配
oop CollectedHeap::array_allocate_nozero(KlassHandle klass,
                                         int size,
                                         int length,
                                         TRAPS) {
  debug_only(check_for_valid_allocation_state());
  assert(!Universe::heap()->is_gc_active(), "Allocation during gc not allowed");
  assert(size >= 0, "int won't convert to size_t");
  HeapWord* obj = common_mem_allocate_noinit(klass, size, CHECK_NULL);
  ((oop)obj)->set_klass_gap(0);
  post_allocation_setup_array(klass, obj, length);
#ifndef PRODUCT
  const size_t hs = oopDesc::header_size()+1;
  Universe::heap()->check_for_non_bad_heap_word_value(obj+hs, size-hs);
#endif
  return (oop)obj;
}

inline void CollectedHeap::oop_iterate_no_header(OopClosure* cl) {
  NoHeaderExtendedOopClosure no_header_cl(cl);
  oop_iterate(&no_header_cl);
}


//  align_allocation_or_fail表示将某个地址按照内存分配的粒度向上对齐
//  addr就是待对齐的地址，alignment_in_bytes是内存分配的粒度，end表示向上对齐时允许的内存最大地址
inline HeapWord* CollectedHeap::align_allocation_or_fail(HeapWord* addr,
                                                         HeapWord* end,
                                                         unsigned short alignment_in_bytes) {
  if (alignment_in_bytes <= ObjectAlignmentInBytes) {
    return addr;
  }

    //校验addr已经按照HeapWordSize对齐了
  assert(is_ptr_aligned(addr, HeapWordSize),
    err_msg("Address " PTR_FORMAT " is not properly aligned.", p2i(addr)));
    //校验alignment_in_bytes是按照HeapWordSize取整过了
  assert(is_size_aligned(alignment_in_bytes, HeapWordSize),
    err_msg("Alignment size %u is incorrect.", alignment_in_bytes));

    //将addr按照alignment_in_bytes向上对齐，地址变大
  HeapWord* new_addr = (HeapWord*) align_pointer_up(addr, alignment_in_bytes);
    //获取新地址和原来地址的差异
  size_t padding = pointer_delta(new_addr, addr);

    //如果已经对齐则返回
  if (padding == 0) {
    return addr;
  }

  if (padding < CollectedHeap::min_fill_size()) {
      //如果padding过小则加上一段，方便下面填充
    padding += alignment_in_bytes / HeapWordSize;
    assert(padding >= CollectedHeap::min_fill_size(),
      err_msg("alignment_in_bytes %u is expect to be larger "
      "than the minimum object size", alignment_in_bytes));
    new_addr = addr + padding;
  }

  assert(new_addr > addr, err_msg("Unexpected arithmetic overflow "
    PTR_FORMAT " not greater than " PTR_FORMAT, p2i(new_addr), p2i(addr)));
  if(new_addr < end) {
      //对齐后在end的范围内则填充，否则返回NULL
    CollectedHeap::fill_with_object(addr, padding);
    return new_addr;
  } else {
    return NULL;
  }
}

#ifndef PRODUCT

inline bool
CollectedHeap::promotion_should_fail(volatile size_t* count) {
  // Access to count is not atomic; the value does not have to be exact.
  if (PromotionFailureALot) {
    const size_t gc_num = total_collections();
    const size_t elapsed_gcs = gc_num - _promotion_failure_alot_gc_number;
    if (elapsed_gcs >= PromotionFailureALotInterval) {
      // Test for unsigned arithmetic wrap-around.
      if (++*count >= PromotionFailureALotCount) {
        *count = 0;
        return true;
      }
    }
  }
  return false;
}

inline bool CollectedHeap::promotion_should_fail() {
  return promotion_should_fail(&_promotion_failure_alot_count);
}

inline void CollectedHeap::reset_promotion_should_fail(volatile size_t* count) {
  if (PromotionFailureALot) {
    _promotion_failure_alot_gc_number = total_collections();
    *count = 0;
  }
}

inline void CollectedHeap::reset_promotion_should_fail() {
  reset_promotion_should_fail(&_promotion_failure_alot_count);
}
#endif  // #ifndef PRODUCT

#endif // SHARE_VM_GC_INTERFACE_COLLECTEDHEAP_INLINE_HPP
