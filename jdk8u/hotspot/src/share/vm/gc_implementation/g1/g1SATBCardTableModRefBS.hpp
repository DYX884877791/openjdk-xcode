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

#ifndef SHARE_VM_GC_IMPLEMENTATION_G1_G1SATBCARDTABLEMODREFBS_HPP
#define SHARE_VM_GC_IMPLEMENTATION_G1_G1SATBCARDTABLEMODREFBS_HPP

#include "gc_implementation/g1/g1RegionToSpaceMapper.hpp"
#include "memory/cardTableModRefBS.hpp"
#include "memory/memRegion.hpp"
#include "oops/oop.inline.hpp"
#include "utilities/macros.hpp"

class DirtyCardQueueSet;
class G1SATBCardTableLoggingModRefBS;

// This barrier is specialized to use a logging barrier to support
// snapshot-at-the-beginning marking.

// G1SATBCardTableModRefBS继承自CardTableModRefBSForCTRS，定义在hotspot/src/share/vm/gc_implementation/g1/g1SATBCardTableModRefBS.hpp中，
// 是为了支持G1的SATB（snapshot-at-the-beginning）并发标记算法而定制的特殊BarrierSet，改写了父类CardTableModRefBS的诸多方法的实现
//  CardTableModRefBS中只用到了dirty_card和clean_card两种CardValue，G1SATBCardTableModRefBS增加了claimed_card和deferred_card两种，
//  并自定义了一个扩展的g1_young_gen，表示该卡表项对应的内存区域是一个young gen
class G1SATBCardTableModRefBS: public CardTableModRefBSForCTRS {
protected:
  enum G1CardValues {
    g1_young_gen = CT_MR_BS_last_reserved << 1
  };

public:
  static int g1_young_card_val()   { return g1_young_gen; }

  // Add "pre_val" to a set of objects that may have been disconnected from the
  // pre-marking object graph.
  static void enqueue(oop pre_val);

  G1SATBCardTableModRefBS(MemRegion whole_heap,
                          int max_covered_regions);

  bool is_a(BarrierSet::Name bsn) {
    return bsn == BarrierSet::G1SATBCT || CardTableModRefBS::is_a(bsn);
  }

    // 该方法在CardTableModRefBS中返回false，表示不支持在写入引用类型的属性时执行预处理
  virtual bool has_write_ref_pre_barrier() { return true; }

  // This notes that we don't need to access any BarrierSet data
  // structures, so this can be called from a static context.
  template <class T> static void write_ref_field_pre_static(T* field, oop newVal) {
    T heap_oop = oopDesc::load_heap_oop(field);
      //如果oop非空
    if (!oopDesc::is_null(heap_oop)) {
        //如果采用指针压缩，即T是narrowOop时需要做decode还原
      enqueue(oopDesc::decode_heap_oop(heap_oop));
    }
  }

  // We export this to make it available in cases where the static
  // type of the barrier set is known.  Note that it is non-virtual.
  template <class T> inline void inline_write_ref_field_pre(T* field, oop newVal) {
    write_ref_field_pre_static(field, newVal);
  }

  // These are the more general virtual versions.
  virtual void write_ref_field_pre_work(oop* field, oop new_val) {
    inline_write_ref_field_pre(field, new_val);
  }
  virtual void write_ref_field_pre_work(narrowOop* field, oop new_val) {
    inline_write_ref_field_pre(field, new_val);
  }
  virtual void write_ref_field_pre_work(void* field, oop new_val) {
    guarantee(false, "Not needed");
  }

  template <class T> void write_ref_array_pre_work(T* dst, int count);
  virtual void write_ref_array_pre(oop* dst, int count, bool dest_uninitialized);
  virtual void write_ref_array_pre(narrowOop* dst, int count, bool dest_uninitialized);

/*
   Claimed and deferred bits are used together in G1 during the evacuation
   pause. These bits can have the following state transitions:
   1. The claimed bit can be put over any other card state. Except that
      the "dirty -> dirty and claimed" transition is checked for in
      G1 code and is not used.
   2. Deferred bit can be set only if the previous state of the card
      was either clean or claimed. mark_card_deferred() is wait-free.
      We do not care if the operation is be successful because if
      it does not it will only result in duplicate entry in the update
      buffer because of the "cache-miss". So it's not worth spinning.
 */

  bool is_card_claimed(size_t card_index) {
    jbyte val = _byte_map[card_index];
    return (val & (clean_card_mask_val() | claimed_card_val())) == claimed_card_val();
  }

  void set_card_claimed(size_t card_index) {
        //获取对应卡表项的值
      jbyte val = _byte_map[card_index];
      if (val == clean_card_val()) {
        val = (jbyte)claimed_card_val();
      } else {
          //使用或运算，会保留卡表项原来的状态
        val |= (jbyte)claimed_card_val();
      }
      _byte_map[card_index] = val;
  }

  void verify_g1_young_region(MemRegion mr) PRODUCT_RETURN;
  void g1_mark_as_young(const MemRegion& mr);

  bool mark_card_deferred(size_t card_index);

  bool is_card_deferred(size_t card_index) {
    jbyte val = _byte_map[card_index];
    return (val & (clean_card_mask_val() | deferred_card_val())) == deferred_card_val();
  }
};

//  G1SATBCardTableLoggingModRefBSChangedListener的定义和G1SATBCardTableLoggingModRefBS在同一个文件中，表示G1下CardTable内存变化后触发的动作
class G1SATBCardTableLoggingModRefBSChangedListener : public G1MappingChangedListener {
 private:
  G1SATBCardTableLoggingModRefBS* _card_table;
 public:
  G1SATBCardTableLoggingModRefBSChangedListener() : _card_table(NULL) { }

  void set_card_table(G1SATBCardTableLoggingModRefBS* card_table) { _card_table = card_table; }

  virtual void on_commit(uint start_idx, size_t num_regions, bool zero_filled);
};

// Adds card-table logging to the post-barrier.
// Usual invariant: all dirty cards are logged in the DirtyCardQueueSet.
// G1SATBCardTableLoggingModRefBS跟G1SATBCardTableLoggingModRefBS定义在同一个g1SATBCardTableModRefBS.hpp中，
// G1实际使用的是G1SATBCardTableLoggingModRefBS作为BarrierSet的实现类，该类同样对父类的实现做了大幅调整，以适配G1的垃圾回收机制
class G1SATBCardTableLoggingModRefBS: public G1SATBCardTableModRefBS {
  friend class G1SATBCardTableLoggingModRefBSChangedListener;
 private:
    // G1SATBCardTableLoggingModRefBS新增了两个属性，这两个都是在构造方法中完成初始化
  G1SATBCardTableLoggingModRefBSChangedListener _listener;
  DirtyCardQueueSet& _dcqs;
 public:
  static size_t compute_size(size_t mem_region_size_in_words) {
    size_t number_of_slots = (mem_region_size_in_words / card_size_in_words);
    return ReservedSpace::allocation_align_size_up(number_of_slots);
  }

  G1SATBCardTableLoggingModRefBS(MemRegion whole_heap,
                                 int max_covered_regions);

    // G1SATBCardTableLoggingModRefBS将父类initialize的实现改成了空实现，增加了一个有参数的initialize方法完成其初始化
  virtual void initialize() { }
  virtual void initialize(G1RegionToSpaceMapper* mapper);

    // G1SATBCardTableLoggingModRefBS下resize_covered_region不会被调用，因为covered元素只有一个，就是表示整个堆内存的whole_heap，所以将该方法改成一个空实现
  virtual void resize_covered_region(MemRegion new_region) { ShouldNotReachHere(); }

  bool is_a(BarrierSet::Name bsn) {
    return bsn == BarrierSet::G1SATBCTLogging ||
      G1SATBCardTableModRefBS::is_a(bsn);
  }

  void write_ref_field_work(void* field, oop new_val, bool release = false);

  // Can be called from static contexts.
  static void write_ref_field_static(void* field, oop new_val);

  // NB: if you do a whole-heap invalidation, the "usual invariant" defined
  // above no longer applies.
  void invalidate(MemRegion mr, bool whole_heap = false);

  void write_region_work(MemRegion mr)    { invalidate(mr); }
  void write_ref_array_work(MemRegion mr) { invalidate(mr); }
};

#endif // SHARE_VM_GC_IMPLEMENTATION_G1_G1SATBCARDTABLEMODREFBS_HPP
