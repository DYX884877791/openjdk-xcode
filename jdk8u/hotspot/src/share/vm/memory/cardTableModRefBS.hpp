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

#ifndef SHARE_VM_MEMORY_CARDTABLEMODREFBS_HPP
#define SHARE_VM_MEMORY_CARDTABLEMODREFBS_HPP

#include "memory/modRefBarrierSet.hpp"
#include "oops/oop.hpp"
#include "oops/oop.inline2.hpp"

// This kind of "BarrierSet" allows a "CollectedHeap" to detect and
// enumerate ref fields that have been modified (since the last
// enumeration.)

// As it currently stands, this barrier is *imprecise*: when a ref field in
// an object "o" is modified, the card table entry for the card containing
// the head of "o" is dirtied, not necessarily the card containing the
// modified field itself.  For object arrays, however, the barrier *is*
// precise; only the card containing the modified element is dirtied.
// Any MemRegionClosures used to scan dirty cards should take these
// considerations into account.

class Generation;
class OopsInGenClosure;
class DirtyCardToOopClosure;
class ClearNoncleanCardWrapper;

// CardTableModRefBS继承自ModRefBarrierSet，表示一个在卡表（Card Table）下使用的ModRefBarrierSet
// CardTableModRefBS在ModRefBarrierSet的基础上增加了不少跟卡表本身相关的方法
// 在CardTableModRefBS当前的实现下，当某个对象o的引用类型字段发生了修改，包含了对象o的对象头的卡表项被标记为脏的，而不是包含了这个被修改的字段的卡表项；
// 当对象数组的某个元素发生修改了，只有包含了这个被修改元素的卡表项被标记为脏的。用来遍历查找脏的卡表项的MemRegionClosures的实现必须考虑上述问题。
class CardTableModRefBS: public ModRefBarrierSet {
  // Some classes get to look at some private stuff.
  friend class BytecodeInterpreter;
  friend class VMStructs;
  friend class CardTableRS;
  friend class CheckForUnmarkedOops; // Needs access to raw card bytes.
  friend class SharkBuilder;
#ifndef PRODUCT
  // For debugging.
  friend class GuaranteeNotModClosure;
#endif
 protected:

    // 卡表项的取值用枚举CardValues表示，其中dirty_card和precleaned_card被认为是脏的，参考card_is_dirty_wrt_gen_iter方法的实现
  enum CardValues {
    clean_card                  = -1,
    // The mask contains zeros in places for all other values.
    clean_card_mask             = clean_card - 31,

    dirty_card                  =  0,
    precleaned_card             =  1,
    claimed_card                =  2,
    deferred_card               =  4,
    last_card                   =  8,
    CT_MR_BS_last_reserved      = 16
  };

  // a word's worth (row) of clean card values
  static const intptr_t clean_card_row = (intptr_t)(-1);

  // dirty and precleaned are equivalent wrt younger_refs_iter.
  static bool card_is_dirty_wrt_gen_iter(jbyte cv) {
    return cv == dirty_card || cv == precleaned_card;
  }

  // Returns "true" iff the value "cv" will cause the card containing it
  // to be scanned in the current traversal.  May be overridden by
  // subtypes.
  virtual bool card_will_be_scanned(jbyte cv) {
    return CardTableModRefBS::card_is_dirty_wrt_gen_iter(cv);
  }

  // Returns "true" iff the value "cv" may have represented a dirty card at
  // some point.
  virtual bool card_may_have_been_dirty(jbyte cv) {
    return card_is_dirty_wrt_gen_iter(cv);
  }

  // The declaration order of these const fields is important; see the
  // constructor before changing.
    //卡表对应的堆内存区域
  const MemRegion _whole_heap;       // the region covered by the card table
    //卡表中最后一个元素的索引
  size_t          _guard_index;      // index of very last element in the card
                                     // table; it is set to a guard value
                                     // (last_card) and should never be modified
    //卡表中最后一个有效元素的索引
  size_t          _last_valid_index; // index of the last valid element
    //映射_byte_map时的内存页大小
  const size_t    _page_size;        // page size used when mapping _byte_map
    //_byte_map的字节数，即元素个数
  size_t          _byte_map_size;    // in bytes
    //用于标记的卡表字节数组
  jbyte*          _byte_map;         // the card marking array

    //当前的MemRegion在_covered数组的索引
  int _cur_covered_regions;
  // The covered regions should be in address order.
    //卡表映射的内存区域
  MemRegion* _covered;
  // The committed regions correspond one-to-one to the covered regions.
  // They represent the card-table memory that has been committed to service
  // the corresponding covered region.  It may be that committed region for
  // one covered region corresponds to a larger region because of page-size
  // roundings.  Thus, a committed region for one covered region may
  // actually extend onto the card-table space for the next covered region.
    //与_covered中的MemRegion一一对应，表示对应的卡表项内存区域，当_covered中的元素内存扩容或者缩容时，_committed中对应的卡表项内存会跟着扩容或者缩容
  MemRegion* _committed;

  // The last card is a guard card, and we commit the page for it so
  // we can use the card for verification purposes. We make sure we never
  // uncommit the MemRegion for that page.
    //位于_guard_index的卡表项对应的一个内存页
  MemRegion _guard_region;

 protected:
  // Initialization utilities; covered_words is the size of the covered region
  // in, um, words.
  inline size_t cards_required(size_t covered_words) {
    // Add one for a guard card, used to detect errors.
    const size_t words = align_size_up(covered_words, card_size_in_words);
    return words / card_size_in_words + 1;
  }

  inline size_t compute_byte_map_size();

  // Finds and return the index of the region, if any, to which the given
  // region would be contiguous.  If none exists, assign a new region and
  // returns its index.  Requires that no more than the maximum number of
  // covered regions defined in the constructor are ever in use.
  int find_covering_region_by_base(HeapWord* base);

  // Same as above, but finds the region containing the given address
  // instead of starting at a given base address.
  int find_covering_region_containing(HeapWord* addr);

  // Resize one of the regions covered by the remembered set.
  virtual void resize_covered_region(MemRegion new_region);

  // Returns the leftmost end of a committed region corresponding to a
  // covered region before covered region "ind", or else "NULL" if "ind" is
  // the first covered region.
  HeapWord* largest_prev_committed_end(int ind) const;

  // Returns the part of the region mr that doesn't intersect with
  // any committed region other than self.  Used to prevent uncommitting
  // regions that are also committed by other regions.  Also protects
  // against uncommitting the guard region.
  MemRegion committed_unique_to_self(int self, MemRegion mr) const;

  // Mapping from address to card marking array entry
    //将某个地址映射到卡表的字节数组元素
  jbyte* byte_for(const void* p) const {
      //校验这个地址位于堆内存中
    assert(_whole_heap.contains(p),
           err_msg("Attempt to access p = " PTR_FORMAT " out of bounds of "
                   " card marking array's _whole_heap = [" PTR_FORMAT "," PTR_FORMAT ")",
                   p2i(p), p2i(_whole_heap.start()), p2i(_whole_heap.end())));
      //此处的计算相当于_byte_map - (uintptr_t(low_bound) >> card_shift)+ (uintptr_t(p) >> card_shift)
    jbyte* result = &byte_map_base[uintptr_t(p) >> card_shift];
      //校验result没有超过数组的范围
    assert(result >= _byte_map && result < _byte_map + _byte_map_size,
           "out of bounds accessor for card marking array");
    return result;
  }

  // The card table byte one after the card marking array
  // entry for argument address. Typically used for higher bounds
  // for loops iterating through the card table.
  jbyte* byte_after(const void* p) const {
    return byte_for(p) + 1;
  }

  // Iterate over the portion of the card-table which covers the given
  // region mr in the given space and apply cl to any dirty sub-regions
  // of mr. Dirty cards are _not_ cleared by the iterator method itself,
  // but closures may arrange to do so on their own should they so wish.
  void non_clean_card_iterate_serial(MemRegion mr, MemRegionClosure* cl);

  // A variant of the above that will operate in a parallel mode if
  // worker threads are available, and clear the dirty cards as it
  // processes them.
  // XXX ??? MemRegionClosure above vs OopsInGenClosure below XXX
  // XXX some new_dcto_cl's take OopClosure's, plus as above there are
  // some MemRegionClosures. Clean this up everywhere. XXX
  void non_clean_card_iterate_possibly_parallel(Space* sp, MemRegion mr,
                                                OopsInGenClosure* cl, CardTableRS* ct);

 private:
  // Work method used to implement non_clean_card_iterate_possibly_parallel()
  // above in the parallel case.
  void non_clean_card_iterate_parallel_work(Space* sp, MemRegion mr,
                                            OopsInGenClosure* cl, CardTableRS* ct,
                                            int n_threads);

 protected:
  // Dirty the bytes corresponding to "mr" (not all of which must be
  // covered.)
  void dirty_MemRegion(MemRegion mr);

  // Clear (to clean_card) the bytes entirely contained within "mr" (not
  // all of which must be covered.)
  void clear_MemRegion(MemRegion mr);

  // *** Support for parallel card scanning.

  // This is an array, one element per covered region of the card table.
  // Each entry is itself an array, with one element per chunk in the
  // covered region.  Each entry of these arrays is the lowest non-clean
  // card of the corresponding chunk containing part of an object from the
  // previous chunk, or else NULL.
    // CardArr是一个jbyte*的指针的别名
  typedef jbyte*  CardPtr;
  typedef CardPtr* CardArr;
    //下列四个数组的长度都跟_covered数组保持一致，为了支持并行的扫描卡表
  CardArr* _lowest_non_clean;
  size_t*  _lowest_non_clean_chunk_size;
  uintptr_t* _lowest_non_clean_base_chunk_index;
  volatile int* _last_LNC_resizing_collection;

  // Initializes "lowest_non_clean" to point to the array for the region
  // covering "sp", and "lowest_non_clean_base_chunk_index" to the chunk
  // index of the corresponding to the first element of that array.
  // Ensures that these arrays are of sufficient size, allocating if necessary.
  // May be called by several threads concurrently.
  void get_LNC_array_for_space(Space* sp,
                               jbyte**& lowest_non_clean,
                               uintptr_t& lowest_non_clean_base_chunk_index,
                               size_t& lowest_non_clean_chunk_size);

  // Returns the number of chunks necessary to cover "mr".
  size_t chunks_to_cover(MemRegion mr) {
    return (size_t)(addr_to_chunk_index(mr.last()) -
                    addr_to_chunk_index(mr.start()) + 1);
  }

  // Returns the index of the chunk in a stride which
  // covers the given address.
  uintptr_t addr_to_chunk_index(const void* addr) {
    uintptr_t card = (uintptr_t) byte_for(addr);
    return card / ParGCCardsPerStrideChunk;
  }

  // Apply cl, which must either itself apply dcto_cl or be dcto_cl,
  // to the cards in the stride (of n_strides) within the given space.
  void process_stride(Space* sp,
                      MemRegion used,
                      jint stride, int n_strides,
                      OopsInGenClosure* cl,
                      CardTableRS* ct,
                      jbyte** lowest_non_clean,
                      uintptr_t lowest_non_clean_base_chunk_index,
                      size_t lowest_non_clean_chunk_size);

  // Makes sure that chunk boundaries are handled appropriately, by
  // adjusting the min_done of dcto_cl, and by using a special card-table
  // value to indicate how min_done should be set.
  void process_chunk_boundaries(Space* sp,
                                DirtyCardToOopClosure* dcto_cl,
                                MemRegion chunk_mr,
                                MemRegion used,
                                jbyte** lowest_non_clean,
                                uintptr_t lowest_non_clean_base_chunk_index,
                                size_t    lowest_non_clean_chunk_size);

public:
  // Constants
  // 每个卡表项对应的内存块card_size的大小通过枚举SomePublicConstants定义
  enum SomePublicConstants {
    card_shift                  = 9,
    card_size                   = 1 << card_shift,
    card_size_in_words          = card_size / sizeof(HeapWord)
  };

  static int clean_card_val()      { return clean_card; }
  static int clean_card_mask_val() { return clean_card_mask; }
  static int dirty_card_val()      { return dirty_card; }
  static int claimed_card_val()    { return claimed_card; }
  static int precleaned_card_val() { return precleaned_card; }
  static int deferred_card_val()   { return deferred_card; }

  // For RTTI simulation.
  bool is_a(BarrierSet::Name bsn) {
    return bsn == BarrierSet::CardTableModRef || ModRefBarrierSet::is_a(bsn);
  }

  CardTableModRefBS(MemRegion whole_heap, int max_covered_regions);
  ~CardTableModRefBS();

  virtual void initialize();

  // *** Barrier set functions.
    // 其补充的描述其支持的动作类型的方法的实现如下:

  bool has_write_ref_pre_barrier() { return false; }

  // Record a reference update. Note that these versions are precise!
  // The scanning code has to handle the fact that the write barrier may be
  // either precise or imprecise. We make non-virtual inline variants of
  // these functions here for performance.
protected:
  void write_ref_field_work(oop obj, size_t offset, oop newVal);
  virtual void write_ref_field_work(void* field, oop newVal, bool release = false);
public:

  bool has_write_ref_array_opt() { return true; }
  bool has_write_region_opt() { return true; }

  // 下面四个inline方法就是BarrierSet中内联方法调用的子类方法，其实现都是将某个地址或者一段连续的内存地址对应的卡表项标记为脏的

  inline void inline_write_region(MemRegion mr) {
    dirty_MemRegion(mr);
  }
protected:
  void write_region_work(MemRegion mr) {
    inline_write_region(mr);
  }
public:

  inline void inline_write_ref_array(MemRegion mr) {
    dirty_MemRegion(mr);
  }
protected:
  void write_ref_array_work(MemRegion mr) {
    inline_write_ref_array(mr);
  }
public:

  bool is_aligned(HeapWord* addr) {
    return is_card_aligned(addr);
  }

  // *** Card-table-barrier-specific things.

  template <class T> inline void inline_write_ref_field_pre(T* field, oop newVal) {}

  template <class T> inline void inline_write_ref_field(T* field, oop newVal, bool release) {
      //获取该字段的内存地址对应的卡表项
    jbyte* byte = byte_for((void*)field);
      //将卡表项标记为脏的
    if (release) {
      // Perform a releasing store if requested.
      OrderAccess::release_store((volatile jbyte*) byte, dirty_card);
    } else {
      *byte = dirty_card;
    }
  }

  // These are used by G1, when it uses the card table as a temporary data
  // structure for card claiming.
  bool is_card_dirty(size_t card_index) {
    return _byte_map[card_index] == dirty_card_val();
  }

  void mark_card_dirty(size_t card_index) {
    _byte_map[card_index] = dirty_card_val();
  }

  bool is_card_clean(size_t card_index) {
    return _byte_map[card_index] == clean_card_val();
  }

  // Card marking array base (adjusted for heap low boundary)
  // This would be the 0th element of _byte_map, if the heap started at 0x0.
  // But since the heap starts at some higher address, this points to somewhere
  // before the beginning of the actual _byte_map.
    //用于计算某个地址映射到卡表字节数组的基地址
  jbyte* byte_map_base;

  // Return true if "p" is at the start of a card.
  bool is_card_aligned(HeapWord* p) {
    jbyte* pcard = byte_for(p);
    return (addr_for(pcard) == p);
  }

  HeapWord* align_to_card_boundary(HeapWord* p) {
    jbyte* pcard = byte_for(p + card_size_in_words - 1);
    return addr_for(pcard);
  }

  // The kinds of precision a CardTableModRefBS may offer.
  enum PrecisionStyle {
    Precise,
    ObjHeadPreciseArray
  };

  // Tells what style of precision this card table offers.
  PrecisionStyle precision() {
    return ObjHeadPreciseArray; // Only one supported for now.
  }

  // ModRefBS functions.
  virtual void invalidate(MemRegion mr, bool whole_heap = false);
  void clear(MemRegion mr);
  void dirty(MemRegion mr);

  // *** Card-table-RemSet-specific things.

  // Invoke "cl.do_MemRegion" on a set of MemRegions that collectively
  // includes all the modified cards (expressing each card as a
  // MemRegion).  Thus, several modified cards may be lumped into one
  // region.  The regions are non-overlapping, and are visited in
  // *decreasing* address order.  (This order aids with imprecise card
  // marking, where a dirty card may cause scanning, and summarization
  // marking, of objects that extend onto subsequent cards.)
    // mod_card_iterate /dirty_card_iterate /dirty_card_range_after_reset
    // 这三个方法都是用来查找遍历脏的卡片，mod_card_iterate有两个重载版本，区别在于两者遍历的卡表项范围不同，一个是遍历整个堆内存对应的MemRegion，
    // 一个是遍历指定的MemRegion对应的卡表项，遍历的时候都是按照地址降序的顺序遍历，将找到的多个连续的脏的卡表项对应的内存块合并成一个MemRegion交给MemRegionClosure处理。
    // dirty_card_iterate与mod_card_iterate最大的不同是dirty_card_iterate是按照地址升序的方式遍历。dirty_card_range_after_reset跟dirty_card_iterate类似，
    // 区别在于dirty_card_range_after_reset支持将找到的脏的卡表项重置成指定的值，并且在找到连续的多个脏的卡表项就返回这些卡表项对应的MemRegion，不继续遍历了。
  void mod_card_iterate(MemRegionClosure* cl) {
    non_clean_card_iterate_serial(_whole_heap, cl);
  }

  // Like the "mod_cards_iterate" above, except only invokes the closure
  // for cards within the MemRegion "mr" (which is required to be
  // card-aligned and sized.)
  void mod_card_iterate(MemRegion mr, MemRegionClosure* cl) {
    non_clean_card_iterate_serial(mr, cl);
  }

  static uintx ct_max_alignment_constraint();

  // Apply closure "cl" to the dirty cards containing some part of
  // MemRegion "mr".
  void dirty_card_iterate(MemRegion mr, MemRegionClosure* cl);

  // Return the MemRegion corresponding to the first maximal run
  // of dirty cards lying completely within MemRegion mr.
  // If reset is "true", then sets those card table entries to the given
  // value.
  MemRegion dirty_card_range_after_reset(MemRegion mr, bool reset,
                                         int reset_val);

  // Provide read-only access to the card table array.
  const jbyte* byte_for_const(const void* p) const {
    return byte_for(p);
  }
  const jbyte* byte_after_const(const void* p) const {
    return byte_after(p);
  }

  // Mapping from card marking array entry to address of first word
  //获取卡表项地址对应的内存地址
  HeapWord* addr_for(const jbyte* p) const {
    assert(p >= _byte_map && p < _byte_map + _byte_map_size,
           "out of bounds access to card marking array");
    size_t delta = pointer_delta(p, byte_map_base, sizeof(jbyte));
    HeapWord* result = (HeapWord*) (delta << card_shift);
    assert(_whole_heap.contains(result),
           err_msg("Returning result = " PTR_FORMAT " out of bounds of "
                   " card marking array's _whole_heap = [" PTR_FORMAT "," PTR_FORMAT ")",
                   p2i(result), p2i(_whole_heap.start()), p2i(_whole_heap.end())));
    return result;
  }

  // Mapping from address to card marking array index.
  size_t index_for(void* p) {
    assert(_whole_heap.contains(p),
           err_msg("Attempt to access p = " PTR_FORMAT " out of bounds of "
                   " card marking array's _whole_heap = [" PTR_FORMAT "," PTR_FORMAT ")",
                   p2i(p), p2i(_whole_heap.start()), p2i(_whole_heap.end())));
    return byte_for(p) - _byte_map;
  }

  const jbyte* byte_for_index(const size_t card_index) const {
    return _byte_map + card_index;
  }

  // Print a description of the memory for the barrier set
  virtual void print_on(outputStream* st) const;

  void verify();
  void verify_guard();

  // val_equals -> it will check that all cards covered by mr equal val
  // !val_equals -> it will check that all cards covered by mr do not equal val
  void verify_region(MemRegion mr, jbyte val, bool val_equals) PRODUCT_RETURN;
  void verify_not_dirty_region(MemRegion mr) PRODUCT_RETURN;
  void verify_dirty_region(MemRegion mr) PRODUCT_RETURN;

  static size_t par_chunk_heapword_alignment() {
    return ParGCCardsPerStrideChunk * card_size_in_words;
  }

};

class CardTableRS;

// A specialization for the CardTableRS gen rem set.
// CardTableModRefBSForCTRS的定义和CardTableModRefBS在同一个cardTableModRefBS.hpp中
class CardTableModRefBSForCTRS: public CardTableModRefBS {
  CardTableRS* _rs;
protected:
  bool card_will_be_scanned(jbyte cv);
  bool card_may_have_been_dirty(jbyte cv);
public:
  CardTableModRefBSForCTRS(MemRegion whole_heap,
                           int max_covered_regions) :
    CardTableModRefBS(whole_heap, max_covered_regions) {}

  void set_CTRS(CardTableRS* rs) { _rs = rs; }
};


#endif // SHARE_VM_MEMORY_CARDTABLEMODREFBS_HPP
