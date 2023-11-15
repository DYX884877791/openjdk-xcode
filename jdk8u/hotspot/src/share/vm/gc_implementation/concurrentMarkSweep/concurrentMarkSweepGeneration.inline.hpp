/*
 * Copyright (c) 2001, 2012, Oracle and/or its affiliates. All rights reserved.
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

#ifndef SHARE_VM_GC_IMPLEMENTATION_CONCURRENTMARKSWEEP_CONCURRENTMARKSWEEPGENERATION_INLINE_HPP
#define SHARE_VM_GC_IMPLEMENTATION_CONCURRENTMARKSWEEP_CONCURRENTMARKSWEEPGENERATION_INLINE_HPP

#include "gc_implementation/concurrentMarkSweep/cmsLockVerifier.hpp"
#include "gc_implementation/concurrentMarkSweep/compactibleFreeListSpace.hpp"
#include "gc_implementation/concurrentMarkSweep/concurrentMarkSweepGeneration.hpp"
#include "gc_implementation/concurrentMarkSweep/concurrentMarkSweepThread.hpp"
#include "gc_implementation/shared/gcUtil.hpp"
#include "memory/defNewGeneration.hpp"

// clear_all用于清空BitMap中所有的标志
inline void CMSBitMap::clear_all() {
  assert_locked();
  // CMS bitmaps are usually cover large memory regions
  _bm.clear_large();
  return;
}

inline size_t CMSBitMap::heapWordToOffset(HeapWord* addr) const {
    //pointer_delta算出addr相对于起始地址的偏移量，单位是字节
  return (pointer_delta(addr, _bmStartWord)) >> _shifter;
}

inline HeapWord* CMSBitMap::offsetToHeapWord(size_t offset) const {
  return _bmStartWord + (offset << _shifter);
}

inline size_t CMSBitMap::heapWordDiffToOffsetDiff(size_t diff) const {
  assert((diff & ((1 << _shifter) - 1)) == 0, "argument check");
  return diff >> _shifter;
}

// mark和par_mark是将某个地址在BitMap中对应的位打标
inline void CMSBitMap::mark(HeapWord* addr) {
    //校验已经获取了锁
  assert_locked();
    //校验addr在CMSBitMap对应的地址范围内
  assert(_bmStartWord <= addr && addr < (_bmStartWord + _bmWordSize),
         "outside underlying space?");
  _bm.set_bit(heapWordToOffset(addr));
}

inline bool CMSBitMap::par_mark(HeapWord* addr) {
  assert_locked();
  assert(_bmStartWord <= addr && addr < (_bmStartWord + _bmWordSize),
         "outside underlying space?");
  return _bm.par_at_put(heapWordToOffset(addr), true);
}

// par_clear用于清除某个地址在BitMap中对应的标志
inline void CMSBitMap::par_clear(HeapWord* addr) {
  assert_locked();
  assert(_bmStartWord <= addr && addr < (_bmStartWord + _bmWordSize),
         "outside underlying space?");
  _bm.par_at_put(heapWordToOffset(addr), false);
}

// mark_range和par_mark_range是将某个小范围的地址区间在BitMap中对应的位打标
inline void CMSBitMap::mark_range(MemRegion mr) {
  NOT_PRODUCT(region_invariant(mr));
  // Range size is usually just 1 bit.
    //通过heapWordToOffset算出来的起始地址通常只相差一位
  _bm.set_range(heapWordToOffset(mr.start()), heapWordToOffset(mr.end()),
                BitMap::small_range);
}

// clear_range和par_clear_range用于清除某个小范围的地址区间在BitMap中对应的位的标志
inline void CMSBitMap::clear_range(MemRegion mr) {
  NOT_PRODUCT(region_invariant(mr));
  // Range size is usually just 1 bit.
  _bm.clear_range(heapWordToOffset(mr.start()), heapWordToOffset(mr.end()),
                  BitMap::small_range);
}

inline void CMSBitMap::par_mark_range(MemRegion mr) {
  NOT_PRODUCT(region_invariant(mr));
  // Range size is usually just 1 bit.
  _bm.par_set_range(heapWordToOffset(mr.start()), heapWordToOffset(mr.end()),
                    BitMap::small_range);
}

inline void CMSBitMap::par_clear_range(MemRegion mr) {
  NOT_PRODUCT(region_invariant(mr));
  // Range size is usually just 1 bit.
  _bm.par_clear_range(heapWordToOffset(mr.start()), heapWordToOffset(mr.end()),
                      BitMap::small_range);
}

inline void CMSBitMap::mark_large_range(MemRegion mr) {
  NOT_PRODUCT(region_invariant(mr));
  // Range size must be greater than 32 bytes.
    //通过heapWordToOffset算出来的起始地址通常只相差至少32位
  _bm.set_range(heapWordToOffset(mr.start()), heapWordToOffset(mr.end()),
                BitMap::large_range);
}

// clear_large_range和par_clear_large_range用于清除某个大范围的地址区间在BitMap中对应的位的标志
inline void CMSBitMap::clear_large_range(MemRegion mr) {
  NOT_PRODUCT(region_invariant(mr));
  // Range size must be greater than 32 bytes.
  _bm.clear_range(heapWordToOffset(mr.start()), heapWordToOffset(mr.end()),
                  BitMap::large_range);
}

inline void CMSBitMap::par_mark_large_range(MemRegion mr) {
  NOT_PRODUCT(region_invariant(mr));
  // Range size must be greater than 32 bytes.
  _bm.par_set_range(heapWordToOffset(mr.start()), heapWordToOffset(mr.end()),
                    BitMap::large_range);
}

inline void CMSBitMap::par_clear_large_range(MemRegion mr) {
  NOT_PRODUCT(region_invariant(mr));
  // Range size must be greater than 32 bytes.
  _bm.par_clear_range(heapWordToOffset(mr.start()), heapWordToOffset(mr.end()),
                      BitMap::large_range);
}

// Starting at "addr" (inclusive) return a memory region
// corresponding to the first maximally contiguous marked ("1") region.
// getAndClearMarkedRegion用于清除指定地址范围内第一个被连续打标的区域的标志
inline MemRegion CMSBitMap::getAndClearMarkedRegion(HeapWord* addr) {
  return getAndClearMarkedRegion(addr, endWord());
}

// Starting at "start_addr" (inclusive) return a memory region
// corresponding to the first maximal contiguous marked ("1") region
// strictly less than end_addr.
inline MemRegion CMSBitMap::getAndClearMarkedRegion(HeapWord* start_addr,
                                                    HeapWord* end_addr) {
  HeapWord *start, *end;
  assert_locked();
    //找到start_addr后第一个打标的地址
  start = getNextMarkedWordAddress  (start_addr, end_addr);
    //找到start之后的第一个没有打标的地址，start和end之间的区域就是一段连续打标的区域
  end   = getNextUnmarkedWordAddress(start,      end_addr);
  assert(start <= end, "Consistency check");
  MemRegion mr(start, end);
  if (!mr.is_empty()) {
      //将start和end之间的标志去掉
    clear_range(mr);
  }
  return mr;
}

//  isMarked 和par_isMarked 用于判断某个地址是否已经打标
inline bool CMSBitMap::isMarked(HeapWord* addr) const {
  assert_locked();
  assert(_bmStartWord <= addr && addr < (_bmStartWord + _bmWordSize),
         "outside underlying space?");
  return _bm.at(heapWordToOffset(addr));
}

// The same as isMarked() but without a lock check.
inline bool CMSBitMap::par_isMarked(HeapWord* addr) const {
    //与isMarked相比，不需要检查锁
  assert(_bmStartWord <= addr && addr < (_bmStartWord + _bmWordSize),
         "outside underlying space?");
  return _bm.at(heapWordToOffset(addr));
}


// isUnmarked是否未打标
inline bool CMSBitMap::isUnmarked(HeapWord* addr) const {
  assert_locked();
  assert(_bmStartWord <= addr && addr < (_bmStartWord + _bmWordSize),
         "outside underlying space?");
  return !_bm.at(heapWordToOffset(addr));
}

// getNextMarkedWordAddress / getNextUnmarkedWordAddress / getAndClearMarkedRegion
//     三个方法都有两个重载版本，指定起止地址和只指定起始地址，结束地址默认BitMap的结束地址。

// Return the HeapWord address corresponding to next "1" bit
// (inclusive).
// getNextMarkedWordAddress返回指定地址范围的第一个位等于1的地址
inline HeapWord* CMSBitMap::getNextMarkedWordAddress(HeapWord* addr) const {
  return getNextMarkedWordAddress(addr, endWord());
}

// Return the least HeapWord address corresponding to next "1" bit
// starting at start_addr (inclusive) but strictly less than end_addr.
inline HeapWord* CMSBitMap::getNextMarkedWordAddress(
  HeapWord* start_addr, HeapWord* end_addr) const {
  assert_locked();
    //找到在指定地址范围内位下一个等于1的地址
  size_t nextOffset = _bm.get_next_one_offset(
                        heapWordToOffset(start_addr),
                        heapWordToOffset(end_addr));
    //将BitMap中的地址转换成实际地址
  HeapWord* nextAddr = offsetToHeapWord(nextOffset);
  assert(nextAddr >= start_addr &&
         nextAddr <= end_addr, "get_next_one postcondition");
  assert((nextAddr == end_addr) ||
         isMarked(nextAddr), "get_next_one postcondition");
  return nextAddr;
}


// Return the HeapWord address corrsponding to the next "0" bit
// (inclusive).
// getNextUnmarkedWordAddress返回指定地址范围的第一个位等于0的地址
inline HeapWord* CMSBitMap::getNextUnmarkedWordAddress(HeapWord* addr) const {
  return getNextUnmarkedWordAddress(addr, endWord());
}

// Return the HeapWord address corrsponding to the next "0" bit
// (inclusive).
inline HeapWord* CMSBitMap::getNextUnmarkedWordAddress(
  HeapWord* start_addr, HeapWord* end_addr) const {
  assert_locked();
    //找到在指定地址范围内位下一个等于0的地址
  size_t nextOffset = _bm.get_next_zero_offset(
                        heapWordToOffset(start_addr),
                        heapWordToOffset(end_addr));
    //将BitMap中的地址转换成实际地址
  HeapWord* nextAddr = offsetToHeapWord(nextOffset);
  assert(nextAddr >= start_addr &&
         nextAddr <= end_addr, "get_next_zero postcondition");
  assert((nextAddr == end_addr) ||
          isUnmarked(nextAddr), "get_next_zero postcondition");
  return nextAddr;
}

// isAllClear用于判断是否所有的标志都被清除了
inline bool CMSBitMap::isAllClear() const {
  assert_locked();
    //获取下一个被标记的地址，如果该地址大于等于结束地址，则认为所有的打标都清空了
  return getNextMarkedWordAddress(startWord()) >= endWord();
}

// 这两个方法都有两个重载版本，一个指定起止地址范围里，一个在整个BitMap对应的地址范围内，都是用来遍历指定地址范围内打标的位，会将这些位转换成真实地址，然后再对真实地址做必要的处理
inline void CMSBitMap::iterate(BitMapClosure* cl, HeapWord* left,
                            HeapWord* right) {
  assert_locked();
  left = MAX2(_bmStartWord, left);
  right = MIN2(_bmStartWord + _bmWordSize, right);
  if (right > left) {
      //遍历逻辑封装在bm里面，BitMapClosure会自动将BitMap的映射地址转换成真实地址
    _bm.iterate(cl, heapWordToOffset(left), heapWordToOffset(right));
  }
}

inline void CMSCollector::start_icms() {
  if (CMSIncrementalMode) {
    ConcurrentMarkSweepThread::start_icms();
  }
}

inline void CMSCollector::stop_icms() {
  if (CMSIncrementalMode) {
    ConcurrentMarkSweepThread::stop_icms();
  }
}

inline void CMSCollector::disable_icms() {
  if (CMSIncrementalMode) {
    ConcurrentMarkSweepThread::disable_icms();
  }
}

inline void CMSCollector::enable_icms() {
  if (CMSIncrementalMode) {
    ConcurrentMarkSweepThread::enable_icms();
  }
}

inline void CMSCollector::icms_wait() {
  if (CMSIncrementalMode) {
    cmsThread()->icms_wait();
  }
}

inline void CMSCollector::save_sweep_limits() {
  _cmsGen->save_sweep_limit();
}

inline bool CMSCollector::is_dead_obj(oop obj) const {
  HeapWord* addr = (HeapWord*)obj;
  assert((_cmsGen->cmsSpace()->is_in_reserved(addr)
          && _cmsGen->cmsSpace()->block_is_obj(addr)),
         "must be object");
  return  should_unload_classes() &&
          _collectorState == Sweeping &&
         !_markBitMap.isMarked(addr);
}

inline bool CMSCollector::should_abort_preclean() const {
  // We are in the midst of an "abortable preclean" and either
  // scavenge is done or foreground GC wants to take over collection
  return _collectorState == AbortablePreclean &&
         (_abort_preclean || _foregroundGCIsActive ||
          GenCollectedHeap::heap()->incremental_collection_will_fail(true /* consult_young */));
}

inline size_t CMSCollector::get_eden_used() const {
  return _young_gen->as_DefNewGeneration()->eden()->used();
}

inline size_t CMSCollector::get_eden_capacity() const {
  return _young_gen->as_DefNewGeneration()->eden()->capacity();
}

inline bool CMSStats::valid() const {
  return _valid_bits == _ALL_VALID;
}

inline void CMSStats::record_gc0_begin() {
  if (_gc0_begin_time.is_updated()) {
    float last_gc0_period = _gc0_begin_time.seconds();
    _gc0_period = AdaptiveWeightedAverage::exp_avg(_gc0_period,
      last_gc0_period, _gc0_alpha);
    _gc0_alpha = _saved_alpha;
    _valid_bits |= _GC0_VALID;
  }
  _cms_used_at_gc0_begin = _cms_gen->cmsSpace()->used();

  _gc0_begin_time.update();
}

inline void CMSStats::record_gc0_end(size_t cms_gen_bytes_used) {
  float last_gc0_duration = _gc0_begin_time.seconds();
  _gc0_duration = AdaptiveWeightedAverage::exp_avg(_gc0_duration,
    last_gc0_duration, _gc0_alpha);

  // Amount promoted.
  _cms_used_at_gc0_end = cms_gen_bytes_used;

  size_t promoted_bytes = 0;
  if (_cms_used_at_gc0_end >= _cms_used_at_gc0_begin) {
    promoted_bytes = _cms_used_at_gc0_end - _cms_used_at_gc0_begin;
  }

  // If the younger gen collections were skipped, then the
  // number of promoted bytes will be 0 and adding it to the
  // average will incorrectly lessen the average.  It is, however,
  // also possible that no promotion was needed.
  //
  // _gc0_promoted used to be calculated as
  // _gc0_promoted = AdaptiveWeightedAverage::exp_avg(_gc0_promoted,
  //  promoted_bytes, _gc0_alpha);
  _cms_gen->gc_stats()->avg_promoted()->sample(promoted_bytes);
  _gc0_promoted = (size_t) _cms_gen->gc_stats()->avg_promoted()->average();

  // Amount directly allocated.
  size_t allocated_bytes = _cms_gen->direct_allocated_words() * HeapWordSize;
  _cms_gen->reset_direct_allocated_words();
  _cms_allocated = AdaptiveWeightedAverage::exp_avg(_cms_allocated,
    allocated_bytes, _gc0_alpha);
}

inline void CMSStats::record_cms_begin() {
  _cms_timer.stop();

  // This is just an approximate value, but is good enough.
  _cms_used_at_cms_begin = _cms_used_at_gc0_end;

  _cms_period = AdaptiveWeightedAverage::exp_avg((float)_cms_period,
    (float) _cms_timer.seconds(), _cms_alpha);
  _cms_begin_time.update();

  _cms_timer.reset();
  _cms_timer.start();
}

inline void CMSStats::record_cms_end() {
  _cms_timer.stop();

  float cur_duration = _cms_timer.seconds();
  _cms_duration = AdaptiveWeightedAverage::exp_avg(_cms_duration,
    cur_duration, _cms_alpha);

  // Avoid division by 0.
  const size_t cms_used_mb = MAX2(_cms_used_at_cms_begin / M, (size_t)1);
  _cms_duration_per_mb = AdaptiveWeightedAverage::exp_avg(_cms_duration_per_mb,
                                 cur_duration / cms_used_mb,
                                 _cms_alpha);

  _cms_end_time.update();
  _cms_alpha = _saved_alpha;
  _allow_duty_cycle_reduction = true;
  _valid_bits |= _CMS_VALID;

  _cms_timer.start();
}

inline double CMSStats::cms_time_since_begin() const {
  return _cms_begin_time.seconds();
}

inline double CMSStats::cms_time_since_end() const {
  return _cms_end_time.seconds();
}

inline double CMSStats::promotion_rate() const {
  assert(valid(), "statistics not valid yet");
  return gc0_promoted() / gc0_period();
}

inline double CMSStats::cms_allocation_rate() const {
  assert(valid(), "statistics not valid yet");
  return cms_allocated() / gc0_period();
}

inline double CMSStats::cms_consumption_rate() const {
  assert(valid(), "statistics not valid yet");
  return (gc0_promoted() + cms_allocated()) / gc0_period();
}

inline unsigned int CMSStats::icms_update_duty_cycle() {
  // Update the duty cycle only if pacing is enabled and the stats are valid
  // (after at least one young gen gc and one cms cycle have completed).
  if (CMSIncrementalPacing && valid()) {
    return icms_update_duty_cycle_impl();
  }
  return _icms_duty_cycle;
}

inline void ConcurrentMarkSweepGeneration::save_sweep_limit() {
  cmsSpace()->save_sweep_limit();
}

inline size_t ConcurrentMarkSweepGeneration::capacity() const {
  return _cmsSpace->capacity();
}

inline size_t ConcurrentMarkSweepGeneration::used() const {
  return _cmsSpace->used();
}

inline size_t ConcurrentMarkSweepGeneration::free() const {
  return _cmsSpace->free();
}

inline MemRegion ConcurrentMarkSweepGeneration::used_region() const {
  return _cmsSpace->used_region();
}

inline MemRegion ConcurrentMarkSweepGeneration::used_region_at_save_marks() const {
  return _cmsSpace->used_region_at_save_marks();
}

inline void MarkFromRootsClosure::do_yield_check() {
  if (ConcurrentMarkSweepThread::should_yield() &&
      !_collector->foregroundGCIsActive() &&
      _yield) {
    do_yield_work();
  }
}

inline void Par_MarkFromRootsClosure::do_yield_check() {
  if (ConcurrentMarkSweepThread::should_yield() &&
      !_collector->foregroundGCIsActive() &&
      _yield) {
    do_yield_work();
  }
}

inline void PushOrMarkClosure::do_yield_check() {
  _parent->do_yield_check();
}

inline void Par_PushOrMarkClosure::do_yield_check() {
  _parent->do_yield_check();
}

// Return value of "true" indicates that the on-going preclean
// should be aborted.
inline bool ScanMarkedObjectsAgainCarefullyClosure::do_yield_check() {
  if (ConcurrentMarkSweepThread::should_yield() &&
      !_collector->foregroundGCIsActive() &&
      _yield) {
    // Sample young gen size before and after yield
    _collector->sample_eden();
    do_yield_work();
    _collector->sample_eden();
    return _collector->should_abort_preclean();
  }
  return false;
}

inline void SurvivorSpacePrecleanClosure::do_yield_check() {
  if (ConcurrentMarkSweepThread::should_yield() &&
      !_collector->foregroundGCIsActive() &&
      _yield) {
    // Sample young gen size before and after yield
    _collector->sample_eden();
    do_yield_work();
    _collector->sample_eden();
  }
}

inline void SweepClosure::do_yield_check(HeapWord* addr) {
  if (ConcurrentMarkSweepThread::should_yield() &&
      !_collector->foregroundGCIsActive() &&
      _yield) {
    do_yield_work(addr);
  }
}

inline void MarkRefsIntoAndScanClosure::do_yield_check() {
  // The conditions are ordered for the remarking phase
  // when _yield is false.
  if (_yield &&
      !_collector->foregroundGCIsActive() &&
      ConcurrentMarkSweepThread::should_yield()) {
    do_yield_work();
  }
}


inline void ModUnionClosure::do_MemRegion(MemRegion mr) {
  // Align the end of mr so it's at a card boundary.
  // This is superfluous except at the end of the space;
  // we should do better than this XXX
  MemRegion mr2(mr.start(), (HeapWord*)round_to((intptr_t)mr.end(),
                 CardTableModRefBS::card_size /* bytes */));
    // _t就是构造方法传入的CMSBitMap指针
  _t->mark_range(mr2);
}

inline void ModUnionClosurePar::do_MemRegion(MemRegion mr) {
  // Align the end of mr so it's at a card boundary.
  // This is superfluous except at the end of the space;
  // we should do better than this XXX
  MemRegion mr2(mr.start(), (HeapWord*)round_to((intptr_t)mr.end(),
                 CardTableModRefBS::card_size /* bytes */));
  _t->par_mark_range(mr2);
}

#endif // SHARE_VM_GC_IMPLEMENTATION_CONCURRENTMARKSWEEP_CONCURRENTMARKSWEEPGENERATION_INLINE_HPP
