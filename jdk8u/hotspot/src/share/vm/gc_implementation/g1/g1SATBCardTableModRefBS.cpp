/*
 * Copyright (c) 2001, 2013, Oracle and/or its affiliates. All rights reserved.
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
#include "gc_implementation/g1/g1CollectedHeap.inline.hpp"
#include "gc_implementation/g1/g1SATBCardTableModRefBS.hpp"
#include "gc_implementation/g1/heapRegion.hpp"
#include "gc_implementation/g1/satbQueue.hpp"
#include "runtime/mutexLocker.hpp"
#include "runtime/orderAccess.inline.hpp"
#include "runtime/thread.inline.hpp"

G1SATBCardTableModRefBS::G1SATBCardTableModRefBS(MemRegion whole_heap,
                                                 int max_covered_regions) :
    CardTableModRefBSForCTRS(whole_heap, max_covered_regions)
{
  _kind = G1SATBCT;
}

// 最终都落脚到enqueue方法，落脚到ObjPtrQueue和SATBMarkQueueSet两个类的实现
void G1SATBCardTableModRefBS::enqueue(oop pre_val) {
  // Nulls should have been already filtered.
    //必须是oop
  assert(pre_val->is_oop(true), "Error");

    //如果satb quene不是活跃的则返回
  if (!JavaThread::satb_mark_queue_set().is_active()) return;
  Thread* thr = Thread::current();
    //如果是Java线程
  if (thr->is_Java_thread()) {
    JavaThread* jt = (JavaThread*)thr;
      //加入到队列中
    jt->satb_mark_queue().enqueue(pre_val);
  } else {
      //如果是JVM线程
    MutexLockerEx x(Shared_SATB_Q_lock, Mutex::_no_safepoint_check_flag);
    JavaThread::satb_mark_queue_set().shared_satb_queue()->enqueue(pre_val);
  }
}

template <class T> void
G1SATBCardTableModRefBS::write_ref_array_pre_work(T* dst, int count) {
  if (!JavaThread::satb_mark_queue_set().is_active()) return;
  T* elem_ptr = dst;
    //遍历dst之后count个oop
  for (int i = 0; i < count; i++, elem_ptr++) {
    T heap_oop = oopDesc::load_heap_oop(elem_ptr);
    if (!oopDesc::is_null(heap_oop)) {
      enqueue(oopDesc::decode_heap_oop_not_null(heap_oop));
    }
  }
}

void G1SATBCardTableModRefBS::write_ref_array_pre(oop* dst, int count, bool dest_uninitialized) {
  if (!dest_uninitialized) {
    write_ref_array_pre_work(dst, count);
  }
}
void G1SATBCardTableModRefBS::write_ref_array_pre(narrowOop* dst, int count, bool dest_uninitialized) {
  if (!dest_uninitialized) {
    write_ref_array_pre_work(dst, count);
  }
}

bool G1SATBCardTableModRefBS::mark_card_deferred(size_t card_index) {
    //获取对应卡表项的值
  jbyte val = _byte_map[card_index];
  // It's already processed
    //如果已经是deferred_card
  if ((val & (clean_card_mask_val() | deferred_card_val())) == deferred_card_val()) {
    return false;
  }

  if  (val == g1_young_gen) {
    // the card is for a young gen region. We don't need to keep track of all pointers into young
      //不需要跟踪指向年轻代的指针
    return false;
  }

  // Cached bit can be installed either on a clean card or on a claimed card.
  jbyte new_val = val;
  if (val == clean_card_val()) {
    new_val = (jbyte)deferred_card_val();
  } else {
      //该卡表项必须原来是claimed_card
    if (val & claimed_card_val()) {
      new_val = val | (jbyte)deferred_card_val();
    }
  }
  if (new_val != val) {
      //原子的更新状态，更新失败不影响
    Atomic::cmpxchg(new_val, &_byte_map[card_index], val);
  }
  return true;
}

void G1SATBCardTableModRefBS::g1_mark_as_young(const MemRegion& mr) {
    //计算mr对应的起始卡表项
  jbyte *const first = byte_for(mr.start());
  jbyte *const last = byte_after(mr.last());

  // Below we may use an explicit loop instead of memset() because on
  // certain platforms memset() can give concurrent readers phantom zeros.
    // UseMemSetInBOT默认为true，表示是否在GC的代码中使用memset完成内存值设置
  if (UseMemSetInBOT) {
    memset(first, g1_young_gen, last - first);
  } else {
      //通过指针的方式遍历
    for (jbyte* i = first; i < last; i++) {
      *i = g1_young_gen;
    }
  }
}

#ifndef PRODUCT
void G1SATBCardTableModRefBS::verify_g1_young_region(MemRegion mr) {
  verify_region(mr, g1_young_gen,  true);
}
#endif

// G1SATBCardTableLoggingModRefBSChangedListener的on_commit方法的实现逻辑：将start_idx对应的内存区域对应的卡表项初始化为clean_card
void G1SATBCardTableLoggingModRefBSChangedListener::on_commit(uint start_idx, size_t num_regions, bool zero_filled) {
  // Default value for a clean card on the card table is -1. So we cannot take advantage of the zero_filled parameter.
  MemRegion mr(G1CollectedHeap::heap()->bottom_addr_for_region(start_idx), num_regions * HeapRegion::GrainWords);
  _card_table->clear(mr);
}

G1SATBCardTableLoggingModRefBS::
G1SATBCardTableLoggingModRefBS(MemRegion whole_heap,
                               int max_covered_regions) :
  G1SATBCardTableModRefBS(whole_heap, max_covered_regions),
  _dcqs(JavaThread::dirty_card_queue_set()),
  _listener()
{
  _kind = G1SATBCTLogging;
  _listener.set_card_table(this);
}

//  该方法相比父类的实现最大的区别有两点，一是没有通过ReservedSpace申请内存，传入的参数G1RegionToSpaceMapper相当于一个已经申请好内存的卡表；二是没有初始化_committed数组，coverd元素只有一个，就是_whole_heap。
void G1SATBCardTableLoggingModRefBS::initialize(G1RegionToSpaceMapper* mapper) {
  mapper->set_mapping_changed_listener(&_listener);

    //G1RegionToSpaceMapper相当于整个卡表了，没有走父类的申请内存逻辑
  _byte_map_size = mapper->reserved().byte_size();

  _guard_index = cards_required(_whole_heap.word_size()) - 1;
  _last_valid_index = _guard_index - 1;

  HeapWord* low_bound  = _whole_heap.start();
  HeapWord* high_bound = _whole_heap.end();

    //只有一个covered元素，并且元素就是整个Java堆
  _cur_covered_regions = 1;
  _covered[0] = _whole_heap;

  _byte_map = (jbyte*) mapper->reserved().start();
  byte_map_base = _byte_map - (uintptr_t(low_bound) >> card_shift);
  assert(byte_for(low_bound) == &_byte_map[0], "Checking start of map");
  assert(byte_for(high_bound-1) <= &_byte_map[_last_valid_index], "Checking end of map");

  if (TraceCardTableModRefBS) {
      //打印日志
    gclog_or_tty->print_cr("G1SATBCardTableModRefBS::G1SATBCardTableModRefBS: ");
    gclog_or_tty->print_cr("  "
                  "  &_byte_map[0]: " INTPTR_FORMAT
                  "  &_byte_map[_last_valid_index]: " INTPTR_FORMAT,
                  p2i(&_byte_map[0]),
                  p2i(&_byte_map[_last_valid_index]));
    gclog_or_tty->print_cr("  "
                  "  byte_map_base: " INTPTR_FORMAT,
                  p2i(byte_map_base));
  }
}

// write_ref_field_work /write_region_work /write_ref_array_work /invalidate
//     上述方法在父类中的实现都是将对应的卡表项置为dirty_card而已，G1SATBCardTableLoggingModRefBS的实现多了一步，将这些dirty_card的卡表项放入DirtyCardQueue中管理起来，避免卡表的扫描。

void
G1SATBCardTableLoggingModRefBS::write_ref_field_work(void* field,
                                                     oop new_val,
                                                     bool release) {
    //获取对应的卡表项
  volatile jbyte* byte = byte_for(field);
    //不处理g1_young_gen的卡表项
  if (*byte == g1_young_gen) {
    return;
  }
  OrderAccess::storeload();
  if (*byte != dirty_card) {
      //将卡表项置为dirty_card
    *byte = dirty_card;
    Thread* thr = Thread::current();
    if (thr->is_Java_thread()) {
      JavaThread* jt = (JavaThread*)thr;
        //将该卡表项放入DirtyCardQueue中
      jt->dirty_card_queue().enqueue(byte);
    } else {
      MutexLockerEx x(Shared_DirtyCardQ_lock,
                      Mutex::_no_safepoint_check_flag);
      _dcqs.shared_dirty_card_queue()->enqueue(byte);
    }
  }
}

void
G1SATBCardTableLoggingModRefBS::write_ref_field_static(void* field,
                                                       oop new_val) {
  uintptr_t field_uint = (uintptr_t)field;
  uintptr_t new_val_uint = cast_from_oop<uintptr_t>(new_val);
  uintptr_t comb = field_uint ^ new_val_uint;
  comb = comb >> HeapRegion::LogOfHRGrainBytes;
  if (comb == 0) return;
  if (new_val == NULL) return;
  // Otherwise, log it.
  G1SATBCardTableLoggingModRefBS* g1_bs =
    (G1SATBCardTableLoggingModRefBS*)Universe::heap()->barrier_set();
  g1_bs->write_ref_field_work(field, new_val);
}

void
G1SATBCardTableLoggingModRefBS::invalidate(MemRegion mr, bool whole_heap) {
    //获取mr内存区域对应的起止卡表项
  volatile jbyte* byte = byte_for(mr.start());
  jbyte* last_byte = byte_for(mr.last());
  Thread* thr = Thread::current();
  if (whole_heap) {
      //如果是整个堆内存
    while (byte <= last_byte) {
      *byte = dirty_card;
      byte++;
    }
  } else {
    // skip all consecutive young cards
      //跳过连续的表示young gen的卡表项
    for (; byte <= last_byte && *byte == g1_young_gen; byte++);

    if (byte <= last_byte) {
      OrderAccess::storeload();
      // Enqueue if necessary.
      if (thr->is_Java_thread()) {
        JavaThread* jt = (JavaThread*)thr;
        for (; byte <= last_byte; byte++) {
            //过滤g1_young_gen
          if (*byte == g1_young_gen) {
            continue;
          }
          if (*byte != dirty_card) {
              //置为dirty_card，并放入队列中
            *byte = dirty_card;
            jt->dirty_card_queue().enqueue(byte);
          }
        }
      } else {
          //非Java线程需要获取锁Shared_DirtyCardQ_lock
        MutexLockerEx x(Shared_DirtyCardQ_lock,
                        Mutex::_no_safepoint_check_flag);
        for (; byte <= last_byte; byte++) {
            // 过滤g1_young_gen
          if (*byte == g1_young_gen) {
            continue;
          }
          if (*byte != dirty_card) {
            *byte = dirty_card;
            _dcqs.shared_dirty_card_queue()->enqueue(byte);
          }
        }
      }
    }
  }
}
