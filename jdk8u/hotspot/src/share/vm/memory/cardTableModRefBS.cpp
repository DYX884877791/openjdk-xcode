/*
 * Copyright (c) 2000, 2014, Oracle and/or its affiliates. All rights reserved.
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
#include "memory/allocation.inline.hpp"
#include "memory/cardTableModRefBS.hpp"
#include "memory/cardTableRS.hpp"
#include "memory/sharedHeap.hpp"
#include "memory/space.hpp"
#include "memory/space.inline.hpp"
#include "memory/universe.hpp"
#include "runtime/java.hpp"
#include "runtime/mutexLocker.hpp"
#include "runtime/virtualspace.hpp"
#include "services/memTracker.hpp"
#include "utilities/macros.hpp"
#ifdef COMPILER1
#include "c1/c1_LIR.hpp"
#include "c1/c1_LIRGenerator.hpp"
#endif

// This kind of "BarrierSet" allows a "CollectedHeap" to detect and
// enumerate ref fields that have been modified (since the last
// enumeration.)

size_t CardTableModRefBS::compute_byte_map_size()
{
  assert(_guard_index == cards_required(_whole_heap.word_size()) - 1,
                                        "unitialized, check declaration order");
  assert(_page_size != 0, "unitialized, check declaration order");
  const size_t granularity = os::vm_allocation_granularity();
    //_guard_index+1相当于卡表项的数量，每个卡表项都是1个字节，总的字节数就是_guard_index+1
    //将其向上取整获取实际的字节数
  return align_size_up(_guard_index + 1, MAX2(_page_size, granularity));
}

//传递进来的whole_heap实际是整个Java堆
CardTableModRefBS::CardTableModRefBS(MemRegion whole_heap,
                                     int max_covered_regions):
    //初始化属性
  ModRefBarrierSet(max_covered_regions),
  _whole_heap(whole_heap),
  _guard_index(0),
  _guard_region(),
  _last_valid_index(0),
  _page_size(os::vm_page_size()),
  _byte_map_size(0),
  _covered(NULL),
  _committed(NULL),
  _cur_covered_regions(0),
  _byte_map(NULL),
  byte_map_base(NULL),
  // LNC functionality
  _lowest_non_clean(NULL),
  _lowest_non_clean_chunk_size(NULL),
  _lowest_non_clean_base_chunk_index(NULL),
  _last_LNC_resizing_collection(NULL)
{
  _kind = BarrierSet::CardTableModRef;

    //校验堆内存的起始地址的低8位必须都是0
  assert((uintptr_t(_whole_heap.start())  & (card_size - 1))  == 0, "heap must start at card boundary");
  assert((uintptr_t(_whole_heap.end()) & (card_size - 1))  == 0, "heap must end at card boundary");

  assert(card_size <= 512, "card_size must be less than 512"); // why?

    //ParallelScavengeHeap下传3，GenCollectedHeap下不是一个固定值，G1CollectedHeap下传2
  _covered   = new MemRegion[_max_covered_regions];
  if (_covered == NULL) {
    vm_exit_during_initialization("Could not allocate card table covered region set.");
  }
}

void CardTableModRefBS::initialize() {
    //cards_required用于计算堆内存对应的卡表项的个数
  _guard_index = cards_required(_whole_heap.word_size()) - 1;
  _last_valid_index = _guard_index - 1;

    //计算_byte_map对应的内存块大小
  _byte_map_size = compute_byte_map_size();

  HeapWord* low_bound  = _whole_heap.start();
  HeapWord* high_bound = _whole_heap.end();

  _cur_covered_regions = 0;
    //初始化_committed，跟_covered的数组元素个数一致
  _committed = new MemRegion[_max_covered_regions];
  if (_committed == NULL) {
    vm_exit_during_initialization("Could not allocate card table committed region set.");
  }

    //_page_size初始化的时候就等于os::vm_page_size()
  const size_t rs_align = _page_size == (size_t) os::vm_page_size() ? 0 :
    MAX2(_page_size, (size_t) os::vm_allocation_granularity());
    //为byte_map申请一段连续内存
  ReservedSpace heap_rs(_byte_map_size, rs_align, false);

  MemTracker::record_virtual_memory_type((address)heap_rs.base(), mtGC);

  os::trace_page_sizes("card table", _guard_index + 1, _guard_index + 1,
                       _page_size, heap_rs.base(), heap_rs.size());
  if (!heap_rs.is_reserved()) {
      //申请失败，抛出异常
    vm_exit_during_initialization("Could not reserve enough space for the "
                                  "card marking array");
  }

  // The assember store_check code will do an unsigned shift of the oop,
  // then add it to byte_map_base, i.e.
  //
  //   _byte_map = byte_map_base + (uintptr_t(low_bound) >> card_shift)
    //获取基地址，作为_byte_map数组的地址
  _byte_map = (jbyte*) heap_rs.base();
    //计算从内存地址映射到字节数组元素的基地址
  byte_map_base = _byte_map - (uintptr_t(low_bound) >> card_shift);
    //校验堆内存的地址范围都在_byte_map支持映射的地址范围内
  assert(byte_for(low_bound) == &_byte_map[0], "Checking start of map");
  assert(byte_for(high_bound-1) <= &_byte_map[_last_valid_index], "Checking end of map");

    //初始化guard_card
  jbyte* guard_card = &_byte_map[_guard_index];
  uintptr_t guard_page = align_size_down((uintptr_t)guard_card, _page_size);
  _guard_region = MemRegion((HeapWord*)guard_page, _page_size);
  os::commit_memory_or_exit((char*)guard_page, _page_size, _page_size,
                            !ExecMem, "card table last card");
  *guard_card = last_card;

    //初始化4个数组
  _lowest_non_clean =
    NEW_C_HEAP_ARRAY(CardArr, _max_covered_regions, mtGC);
  _lowest_non_clean_chunk_size =
    NEW_C_HEAP_ARRAY(size_t, _max_covered_regions, mtGC);
  _lowest_non_clean_base_chunk_index =
    NEW_C_HEAP_ARRAY(uintptr_t, _max_covered_regions, mtGC);
  _last_LNC_resizing_collection =
    NEW_C_HEAP_ARRAY(int, _max_covered_regions, mtGC);
  if (_lowest_non_clean == NULL
      || _lowest_non_clean_chunk_size == NULL
      || _lowest_non_clean_base_chunk_index == NULL
      || _last_LNC_resizing_collection == NULL)
      //内存分配失败抛出异常
    vm_exit_during_initialization("couldn't allocate an LNC array.");
  for (int i = 0; i < _max_covered_regions; i++) {
      //数组元素初始化
    _lowest_non_clean[i] = NULL;
    _lowest_non_clean_chunk_size[i] = 0;
    _last_LNC_resizing_collection[i] = -1;
  }

  if (TraceCardTableModRefBS) {
    gclog_or_tty->print_cr("CardTableModRefBS::CardTableModRefBS: ");
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

CardTableModRefBS::~CardTableModRefBS() {
    //释放各数组
  if (_covered) {
    delete[] _covered;
    _covered = NULL;
  }
  if (_committed) {
    delete[] _committed;
    _committed = NULL;
  }
  if (_lowest_non_clean) {
    FREE_C_HEAP_ARRAY(CardArr, _lowest_non_clean, mtGC);
    _lowest_non_clean = NULL;
  }
  if (_lowest_non_clean_chunk_size) {
    FREE_C_HEAP_ARRAY(size_t, _lowest_non_clean_chunk_size, mtGC);
    _lowest_non_clean_chunk_size = NULL;
  }
  if (_lowest_non_clean_base_chunk_index) {
    FREE_C_HEAP_ARRAY(uintptr_t, _lowest_non_clean_base_chunk_index, mtGC);
    _lowest_non_clean_base_chunk_index = NULL;
  }
  if (_last_LNC_resizing_collection) {
    FREE_C_HEAP_ARRAY(int, _last_LNC_resizing_collection, mtGC);
    _last_LNC_resizing_collection = NULL;
  }
}

//根据基地址找到对应的covering_region，如果没有匹配的则创建一个新的
int CardTableModRefBS::find_covering_region_by_base(HeapWord* base) {
  int i;
    //_covered数组中的MemRegion是按照地址升序来的，所以找到一个基地址大于base的就可以停止查找了
  for (i = 0; i < _cur_covered_regions; i++) {
    if (_covered[i].start() == base) return i;
    if (_covered[i].start() > base) break;
  }
  // If we didn't find it, create a new one.
    //没有对应的MemRegion则创建一个新的，只有两种情形，要么所有的_covered MemRegion的基地址小于start，i就等于_cur_covered_regions
    //要么有一个_covered MemRegion的基地址大于base，i就小于_cur_covered_regions
  assert(_cur_covered_regions < _max_covered_regions,
         "too many covered regions");
  // Move the ones above up, to maintain sorted order.
    //把i之后数组元素往后挪一个，从而让新的元素放到i的位置上，如果i等于_cur_covered_regions则不处理
  for (int j = _cur_covered_regions; j > i; j--) {
    _covered[j] = _covered[j-1];
    _committed[j] = _committed[j-1];
  }
  int res = i;
    //初始化新的_covered和_committed数组元素
  _cur_covered_regions++;
  _covered[res].set_start(base);
  _covered[res].set_word_size(0);
    //计算base对应的卡表项地址，并做内存对齐
  jbyte* ct_start = byte_for(base);
  uintptr_t ct_start_aligned = align_size_down((uintptr_t)ct_start, _page_size);
  _committed[res].set_start((HeapWord*)ct_start_aligned);
  _committed[res].set_word_size(0);
  return res;
}

int CardTableModRefBS::find_covering_region_containing(HeapWord* addr) {
  for (int i = 0; i < _cur_covered_regions; i++) {
    if (_covered[i].contains(addr)) {
      return i;
    }
  }
  assert(0, "address outside of heap?");
  return -1;
}

//获取ind之前的_committed元素的最大的end地址
HeapWord* CardTableModRefBS::largest_prev_committed_end(int ind) const {
  HeapWord* max_end = NULL;
  for (int j = 0; j < ind; j++) {
    HeapWord* this_end = _committed[j].end();
    if (this_end > max_end) max_end = this_end;
  }
  return max_end;
}

//遍历所有_committed元素，保证mr是唯一的，不会跟其他_committed元素的内存区域有重叠
MemRegion CardTableModRefBS::committed_unique_to_self(int self,
                                                      MemRegion mr) const {
  MemRegion result = mr;
  for (int r = 0; r < _cur_covered_regions; r += 1) {
    if (r != self) {
        //minus表示取result区域减去_committed[r]区域的剩余区域
      result = result.minus(_committed[r]);
    }
  }
  // Never include the guard page.
  result = result.minus(_guard_region);
  return result;
}

// resize_covered_region用于调整_covered中MemRegion的内存大小，并配套调整与之对应的表示卡表项内存的_committed中的MemRegion，如果扩容则commit申请新内存，如果缩容则uncommit减少内存。
void CardTableModRefBS::resize_covered_region(MemRegion new_region) {
  // We don't change the start of a region, only the end.
    //校验堆内存是否包含指定的内存块new_region
  assert(_whole_heap.contains(new_region),
           "attempt to cover area not in reserved area");
  debug_only(verify_guard();)
  // collided is true if the expansion would push into another committed region
  debug_only(bool collided = false;)
    //根据new_region的基地址找到对应的_covered元素，如果没有找到则创建一个新的
  int const ind = find_covering_region_by_base(new_region.start());
  MemRegion const old_region = _covered[ind];
  assert(old_region.start() == new_region.start(), "just checking");
    //如果两个MemRegion大小不同
  if (new_region.word_size() != old_region.word_size()) {
    // Commit new or uncommit old pages, if necessary.
    MemRegion cur_committed = _committed[ind];
    // Extend the end of this _commited region
    // to cover the end of any lower _committed regions.
    // This forms overlapping regions, but never interior regions.
      //获取ind之前的committed元素end的最大值，正常情况
    HeapWord* const max_prev_end = largest_prev_committed_end(ind);
    if (max_prev_end > cur_committed.end()) {
        //只有cur_committed是在find_covering_region_by_base方法中新添加的，时才会出现进入此分支
        //设置成最大值，这是一个初始值
      cur_committed.set_end(max_prev_end);
    }
    // Align the end up to a page size (starts are already aligned).
      //获取new_region终止地址对应的卡表项地址，并做内存对齐
    jbyte* const new_end = byte_after(new_region.last());
    HeapWord* new_end_aligned =
      (HeapWord*) align_size_up((uintptr_t)new_end, _page_size);
    assert(new_end_aligned >= (HeapWord*) new_end,
           "align up, but less");
    // Check the other regions (excludes "ind") to ensure that
    // the new_end_aligned does not intrude onto the committed
    // space of another region.
      //检查其他的committed元素，判断new_end_aligned是否在其范围内，如果在则修改new_end_aligned
    int ri = 0;
    for (ri = 0; ri < _cur_covered_regions; ri++) {
      if (ri != ind) {
        if (_committed[ri].contains(new_end_aligned)) {
          // The prior check included in the assert
          // (new_end_aligned >= _committed[ri].start())
          // is redundant with the "contains" test.
          // Any region containing the new end
          // should start at or beyond the region found (ind)
          // for the new end (committed regions are not expected to
          // be proper subsets of other committed regions).
            //如果包含则ri的起始地址肯定大于ind的，即ri对应内存区域在ind的后面
          assert(_committed[ri].start() >= _committed[ind].start(),
                 "New end of committed region is inconsistent");
            //将new_end_aligned缩小为ri的起始地址，避免重叠
          new_end_aligned = _committed[ri].start();
          // new_end_aligned can be equal to the start of its
          // committed region (i.e., of "ind") if a second
          // region following "ind" also start at the same location
          // as "ind".
          assert(new_end_aligned >= _committed[ind].start(),
            "New end of committed region is before start");
          debug_only(collided = true;)
          // Should only collide with 1 region
            //因为commit元素是按照地址升序排序的，所以只可能出现一个重叠的
          break;
        }
      }
    }
#ifdef ASSERT
    for (++ri; ri < _cur_covered_regions; ri++) {
      assert(!_committed[ri].contains(new_end_aligned),
        "New end of committed region is in a second committed region");
    }
#endif
    // The guard page is always committed and should not be committed over.
    // "guarded" is used for assertion checking below and recalls the fact
    // that the would-be end of the new committed region would have
    // penetrated the guard page.
    HeapWord* new_end_for_commit = new_end_aligned;

    DEBUG_ONLY(bool guarded = false;)
    if (new_end_for_commit > _guard_region.start()) {
        //不能超过_guard_region的内存区域
      new_end_for_commit = _guard_region.start();
      DEBUG_ONLY(guarded = true;)
    }

    if (new_end_for_commit > cur_committed.end()) {
      // Must commit new pages.
        //需要扩容，申请新的内存
      MemRegion const new_committed =
        MemRegion(cur_committed.end(), new_end_for_commit);

      assert(!new_committed.is_empty(), "Region should not be empty here");
      os::commit_memory_or_exit((char*)new_committed.start(),
                                new_committed.byte_size(), _page_size,
                                !ExecMem, "card table expansion");
    // Use new_end_aligned (as opposed to new_end_for_commit) because
    // the cur_committed region may include the guard region.
    } else if (new_end_aligned < cur_committed.end()) {
      // Must uncommit pages.
        //新的MemRegion就是需要缩容（uncommit）掉的内存区域
        //遍历所有_committed元素，确保新的MemRegion不会跟其他_committed元素的内存区域有重叠
      MemRegion const uncommit_region =
        committed_unique_to_self(ind, MemRegion(new_end_aligned,
                                                cur_committed.end()));
      if (!uncommit_region.is_empty()) {
        // It is not safe to uncommit cards if the boundary between
        // the generations is moving.  A shrink can uncommit cards
        // owned by generation A but being used by generation B.
          //UseAdaptiveGCBoundary表示能否移动young和old之间的边界，默认为false
          //如果在移动young和old之间的边界的时候缩容（uncommit 卡表对应的内存区域）是不安全的
        if (!UseAdaptiveGCBoundary) {
          if (!os::uncommit_memory((char*)uncommit_region.start(),
                                   uncommit_region.byte_size())) {
              //  uncommit失败，new_end_aligned重置成原来的
            assert(false, "Card table contraction failed");
            // The call failed so don't change the end of the
            // committed region.  This is better than taking the
            // VM down.
            new_end_aligned = _committed[ind].end();
          }
        } else {
            //UseAdaptiveGCBoundary为true，因为此时uncommit不安全，所以一样将new_end_aligned重置成原来的
          new_end_aligned = _committed[ind].end();
        }
      }
    }
    // In any case, we can reset the end of the current committed entry.
      //设置end，等同于设置了size
    _committed[ind].set_end(new_end_aligned);

#ifdef ASSERT
    // Check that the last card in the new region is committed according
    // to the tables.
    bool covered = false;
    for (int cr = 0; cr < _cur_covered_regions; cr++) {
      if (_committed[cr].contains(new_end - 1)) {
        covered = true;
        break;
      }
    }
    assert(covered, "Card for end of new region not committed");
#endif

    // The default of 0 is not necessarily clean cards.
    jbyte* entry;
    if (old_region.last() < _whole_heap.start()) {
      entry = byte_for(_whole_heap.start());
    } else {
      entry = byte_after(old_region.last());
    }
    assert(index_for(new_region.last()) <  _guard_index,
      "The guard card will be overwritten");
    // This line commented out cleans the newly expanded region and
    // not the aligned up expanded region.
    // jbyte* const end = byte_after(new_region.last());
    jbyte* const end = (jbyte*) new_end_for_commit;
    assert((end >= byte_after(new_region.last())) || collided || guarded,
      "Expect to be beyond new region unless impacting another region");
    // do nothing if we resized downward.
#ifdef ASSERT
    for (int ri = 0; ri < _cur_covered_regions; ri++) {
      if (ri != ind) {
        // The end of the new committed region should not
        // be in any existing region unless it matches
        // the start of the next region.
        assert(!_committed[ri].contains(end) ||
               (_committed[ri].start() == (HeapWord*) end),
               "Overlapping committed regions");
      }
    }
#endif
    if (entry < end) {
        //如果entry小于end，则两者之间的卡表项为扩容部分对应的卡表项
        //将entry和end之间的卡表项置为clean_card
      memset(entry, clean_card, pointer_delta(end, entry, sizeof(jbyte)));
    }
  }
  // In any case, the covered size changes.
    //修改_covered元素的word_size
  _covered[ind].set_word_size(new_region.word_size());
  if (TraceCardTableModRefBS) {
      //打印Trace日志
    gclog_or_tty->print_cr("CardTableModRefBS::resize_covered_region: ");
    gclog_or_tty->print_cr("  "
                  "  _covered[%d].start(): " INTPTR_FORMAT
                  "  _covered[%d].last(): " INTPTR_FORMAT,
                  ind, p2i(_covered[ind].start()),
                  ind, p2i(_covered[ind].last()));
    gclog_or_tty->print_cr("  "
                  "  _committed[%d].start(): " INTPTR_FORMAT
                  "  _committed[%d].last(): " INTPTR_FORMAT,
                  ind, p2i(_committed[ind].start()),
                  ind, p2i(_committed[ind].last()));
    gclog_or_tty->print_cr("  "
                  "  byte_for(start): " INTPTR_FORMAT
                  "  byte_for(last): " INTPTR_FORMAT,
                  p2i(byte_for(_covered[ind].start())),
                  p2i(byte_for(_covered[ind].last())));
    gclog_or_tty->print_cr("  "
                  "  addr_for(start): " INTPTR_FORMAT
                  "  addr_for(last): " INTPTR_FORMAT,
                  p2i(addr_for((jbyte*) _committed[ind].start())),
                  p2i(addr_for((jbyte*) _committed[ind].last())));
  }
  // Touch the last card of the covered region to show that it
  // is committed (or SEGV).
  debug_only((void) (*byte_for(_covered[ind].last()));)
  debug_only(verify_guard();)
}

// Note that these versions are precise!  The scanning code has to handle the
// fact that the write barrier may be either precise or imprecise.

void CardTableModRefBS::write_ref_field_work(void* field, oop newVal, bool release) {
  inline_write_ref_field(field, newVal, release);
}


void CardTableModRefBS::non_clean_card_iterate_possibly_parallel(Space* sp,
                                                                 MemRegion mr,
                                                                 OopsInGenClosure* cl,
                                                                 CardTableRS* ct) {
  if (!mr.is_empty()) {
    // Caller (process_roots()) claims that all GC threads
    // execute this call.  With UseDynamicNumberOfGCThreads now all
    // active GC threads execute this call.  The number of active GC
    // threads needs to be passed to par_non_clean_card_iterate_work()
    // to get proper partitioning and termination.
    //
    // This is an example of where n_par_threads() is used instead
    // of workers()->active_workers().  n_par_threads can be set to 0 to
    // turn off parallelism.  For example when this code is called as
    // part of verification and SharedHeap::process_roots() is being
    // used, then n_par_threads() may have been set to 0.  active_workers
    // is not overloaded with the meaning that it is a switch to disable
    // parallelism and so keeps the meaning of the number of
    // active gc workers.  If parallelism has not been shut off by
    // setting n_par_threads to 0, then n_par_threads should be
    // equal to active_workers.  When a different mechanism for shutting
    // off parallelism is used, then active_workers can be used in
    // place of n_par_threads.
    //  This is an example of a path where n_par_threads is
    // set to 0 to turn off parallism.
    //  [7] CardTableModRefBS::non_clean_card_iterate()
    //  [8] CardTableRS::younger_refs_in_space_iterate()
    //  [9] Generation::younger_refs_in_space_iterate()
    //  [10] OneContigSpaceCardGeneration::younger_refs_iterate()
    //  [11] CompactingPermGenGen::younger_refs_iterate()
    //  [12] CardTableRS::younger_refs_iterate()
    //  [13] SharedHeap::process_strong_roots()
    //  [14] G1CollectedHeap::verify()
    //  [15] Universe::verify()
    //  [16] G1CollectedHeap::do_collection_pause_at_safepoint()
    //
    int n_threads =  SharedHeap::heap()->n_par_threads();
    bool is_par = n_threads > 0;
    if (is_par) {
#if INCLUDE_ALL_GCS
      assert(SharedHeap::heap()->n_par_threads() ==
             SharedHeap::heap()->workers()->active_workers(), "Mismatch");
      non_clean_card_iterate_parallel_work(sp, mr, cl, ct, n_threads);
#else  // INCLUDE_ALL_GCS
      fatal("Parallel gc not supported here.");
#endif // INCLUDE_ALL_GCS
    } else {
      // We do not call the non_clean_card_iterate_serial() version below because
      // we want to clear the cards (which non_clean_card_iterate_serial() does not
      // do for us): clear_cl here does the work of finding contiguous dirty ranges
      // of cards to process and clear.

      DirtyCardToOopClosure* dcto_cl = sp->new_dcto_cl(cl, precision(),
                                                       cl->gen_boundary());
      ClearNoncleanCardWrapper clear_cl(dcto_cl, ct);

      clear_cl.do_MemRegion(mr);
    }
  }
}

// The iterator itself is not MT-aware, but
// MT-aware callers and closures can use this to
// accomplish dirty card iteration in parallel. The
// iterator itself does not clear the dirty cards, or
// change their values in any manner.
//MemRegionClosure通过此方法遍历所有的脏的卡表项，具体的处理逻辑都在MemRegionClosure中
void CardTableModRefBS::non_clean_card_iterate_serial(MemRegion mr,
                                                      MemRegionClosure* cl) {
  bool is_par = (SharedHeap::heap()->n_par_threads() > 0);
    //校验要么不并行，要么两个地方的并行线程数一致
  assert(!is_par ||
          (SharedHeap::heap()->n_par_threads() ==
          SharedHeap::heap()->workers()->active_workers()), "Mismatch");
  for (int i = 0; i < _cur_covered_regions; i++) {
      //取两个MemRegion的交集区域
    MemRegion mri = mr.intersection(_covered[i]);
    if (mri.word_size() > 0) {
        //获取mri起始地址对应的卡表范围
      jbyte* cur_entry = byte_for(mri.last());
      jbyte* limit = byte_for(mri.start());
      while (cur_entry >= limit) {
        jbyte* next_entry = cur_entry - 1;
        if (*cur_entry != clean_card) {
          size_t non_clean_cards = 1;
          // Should the next card be included in this range of dirty cards.
            //往前按照地址降序遍历，找到紧挨着的同样是脏的卡表项
          while (next_entry >= limit && *next_entry != clean_card) {
            non_clean_cards++;
            cur_entry = next_entry;
            next_entry--;
          }
          // The memory region may not be on a card boundary.  So that
          // objects beyond the end of the region are not processed, make
          // cur_cards precise with regard to the end of the memory region.
            //将多个连续的脏的卡表项对应的内存块合并成一个MemRegion
          MemRegion cur_cards(addr_for(cur_entry),
                              non_clean_cards * card_size_in_words);
            //取cur_cards与mri的交集，避免根据卡表项反推出来的地址范围超过了原来mri的范围
          MemRegion dirty_region = cur_cards.intersection(mri);
            //通过MemRegionClosure处理脏的卡表项
          cl->do_MemRegion(dirty_region);
        }
        cur_entry = next_entry;
      }
    }
  }
}

void CardTableModRefBS::dirty_MemRegion(MemRegion mr) {
  assert((HeapWord*)align_size_down((uintptr_t)mr.start(), HeapWordSize) == mr.start(), "Unaligned start");
  assert((HeapWord*)align_size_up  ((uintptr_t)mr.end(),   HeapWordSize) == mr.end(),   "Unaligned end"  );
  jbyte* cur  = byte_for(mr.start());
    //last返回MemRegion最后一个字宽的地址
  jbyte* last = byte_after(mr.last());
    //将MemRegion内存区域对应的所有卡表项都标记为脏的
    //将MemRegion对应的卡表项都置为dirty_card
  while (cur < last) {
    *cur = dirty_card;
    cur++;
  }
}

// invalidate将某个MemRegion对应的卡表项全部置为dirty_card
void CardTableModRefBS::invalidate(MemRegion mr, bool whole_heap) {
    //校验mr的起始地址经过内存对齐的
  assert((HeapWord*)align_size_down((uintptr_t)mr.start(), HeapWordSize) == mr.start(), "Unaligned start");
  assert((HeapWord*)align_size_up  ((uintptr_t)mr.end(),   HeapWordSize) == mr.end(),   "Unaligned end"  );
  for (int i = 0; i < _cur_covered_regions; i++) {
      //取交集
    MemRegion mri = mr.intersection(_covered[i]);
    if (!mri.is_empty()) dirty_MemRegion(mri);
  }
}

void CardTableModRefBS::clear_MemRegion(MemRegion mr) {
  // Be conservative: only clean cards entirely contained within the
  // region.
    //计算mr对应的卡表项范围
  jbyte* cur;
  if (mr.start() == _whole_heap.start()) {
    cur = byte_for(mr.start());
  } else {
    assert(mr.start() > _whole_heap.start(), "mr is not covered.");
    cur = byte_after(mr.start() - 1);
  }
  jbyte* last = byte_after(mr.last());
    //将范围内的卡表项的值都置为clean_card
  memset(cur, clean_card, pointer_delta(last, cur, sizeof(jbyte)));
}

// clear用于将某个MemRegion对应的卡表项全部置为clean_card
void CardTableModRefBS::clear(MemRegion mr) {
  for (int i = 0; i < _cur_covered_regions; i++) {
      //取mr同_covered的交集
    MemRegion mri = mr.intersection(_covered[i]);
    if (!mri.is_empty()) clear_MemRegion(mri);
  }
}

void CardTableModRefBS::dirty(MemRegion mr) {
  jbyte* first = byte_for(mr.start());
  jbyte* last  = byte_after(mr.last());
  memset(first, dirty_card, last-first);
}

// Unlike several other card table methods, dirty_card_iterate()
// iterates over dirty cards ranges in increasing address order.
void CardTableModRefBS::dirty_card_iterate(MemRegion mr,
                                           MemRegionClosure* cl) {
  for (int i = 0; i < _cur_covered_regions; i++) {
      //取交集
    MemRegion mri = mr.intersection(_covered[i]);
    if (!mri.is_empty()) {
      jbyte *cur_entry, *next_entry, *limit;
        //按地址升序的方式遍历卡表项
      for (cur_entry = byte_for(mri.start()), limit = byte_for(mri.last());
           cur_entry <= limit;
           cur_entry  = next_entry) {
        next_entry = cur_entry + 1;
        if (*cur_entry == dirty_card) {
          size_t dirty_cards;
          // Accumulate maximal dirty card range, starting at cur_entry
            //找到多个连续的脏的卡表项
          for (dirty_cards = 1;
               next_entry <= limit && *next_entry == dirty_card;
               dirty_cards++, next_entry++);
            //将多个脏的卡表项对应的内存区域合并成一个MemRegion
          MemRegion cur_cards(addr_for(cur_entry),
                              dirty_cards*card_size_in_words);
          cl->do_MemRegion(cur_cards);
        }
      }
    }
  }
}

MemRegion CardTableModRefBS::dirty_card_range_after_reset(MemRegion mr,
                                                          bool reset,
                                                          int reset_val) {
  for (int i = 0; i < _cur_covered_regions; i++) {
    MemRegion mri = mr.intersection(_covered[i]);
    if (!mri.is_empty()) {
      jbyte* cur_entry, *next_entry, *limit;
      for (cur_entry = byte_for(mri.start()), limit = byte_for(mri.last());
           cur_entry <= limit;
           cur_entry  = next_entry) {
        next_entry = cur_entry + 1;
        if (*cur_entry == dirty_card) {
          size_t dirty_cards;
          // Accumulate maximal dirty card range, starting at cur_entry
            //遍历逻辑同dirty_card_iterate，不同的是支持卡表项的值重置
          for (dirty_cards = 1;
               next_entry <= limit && *next_entry == dirty_card;
               dirty_cards++, next_entry++);
          MemRegion cur_cards(addr_for(cur_entry),
                              dirty_cards*card_size_in_words);
          if (reset) {
            for (size_t i = 0; i < dirty_cards; i++) {
              cur_entry[i] = reset_val;
            }
          }
            //找到连续的多个脏的卡表项就返回，不继续遍历
          return cur_cards;
        }
      }
    }
  }
    //没有找到返回一个空的MemRegion
  return MemRegion(mr.end(), mr.end());
}

uintx CardTableModRefBS::ct_max_alignment_constraint() {
  return card_size * os::vm_page_size();
}

void CardTableModRefBS::verify_guard() {
  // For product build verification
  guarantee(_byte_map[_guard_index] == last_card,
            "card table guard has been modified");
}

void CardTableModRefBS::verify() {
  verify_guard();
}

#ifndef PRODUCT
void CardTableModRefBS::verify_region(MemRegion mr,
                                      jbyte val, bool val_equals) {
  jbyte* start    = byte_for(mr.start());
  jbyte* end      = byte_for(mr.last());
  bool failures = false;
  for (jbyte* curr = start; curr <= end; ++curr) {
    jbyte curr_val = *curr;
    bool failed = (val_equals) ? (curr_val != val) : (curr_val == val);
    if (failed) {
      if (!failures) {
        tty->cr();
        tty->print_cr("== CT verification failed: [" INTPTR_FORMAT "," INTPTR_FORMAT "]", p2i(start), p2i(end));
        tty->print_cr("==   %sexpecting value: %d",
                      (val_equals) ? "" : "not ", val);
        failures = true;
      }
      tty->print_cr("==   card " PTR_FORMAT " [" PTR_FORMAT "," PTR_FORMAT "], "
                    "val: %d", p2i(curr), p2i(addr_for(curr)),
                    p2i((HeapWord*) (((size_t) addr_for(curr)) + card_size)),
                    (int) curr_val);
    }
  }
  guarantee(!failures, "there should not have been any failures");
}

void CardTableModRefBS::verify_not_dirty_region(MemRegion mr) {
  verify_region(mr, dirty_card, false /* val_equals */);
}

void CardTableModRefBS::verify_dirty_region(MemRegion mr) {
  verify_region(mr, dirty_card, true /* val_equals */);
}
#endif

void CardTableModRefBS::print_on(outputStream* st) const {
  st->print_cr("Card table byte_map: [" INTPTR_FORMAT "," INTPTR_FORMAT "] byte_map_base: " INTPTR_FORMAT,
               p2i(_byte_map), p2i(_byte_map + _byte_map_size), p2i(byte_map_base));
}

bool CardTableModRefBSForCTRS::card_will_be_scanned(jbyte cv) {
  return
    CardTableModRefBS::card_will_be_scanned(cv) ||
    _rs->is_prev_nonclean_card_val(cv);
};

bool CardTableModRefBSForCTRS::card_may_have_been_dirty(jbyte cv) {
  return
    cv != clean_card &&
    (CardTableModRefBS::card_may_have_been_dirty(cv) ||
     CardTableRS::youngergen_may_have_been_dirty(cv));
};
