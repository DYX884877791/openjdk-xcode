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

#ifndef SHARE_VM_MEMORY_DEFNEWGENERATION_HPP
#define SHARE_VM_MEMORY_DEFNEWGENERATION_HPP

#include "gc_implementation/shared/ageTable.hpp"
#include "gc_implementation/shared/cSpaceCounters.hpp"
#include "gc_implementation/shared/generationCounters.hpp"
#include "gc_implementation/shared/copyFailedInfo.hpp"
#include "memory/generation.inline.hpp"
#include "utilities/stack.hpp"

class EdenSpace;
class ContiguousSpace;
class ScanClosure;
class STWGCTimer;

// DefNewGeneration is a young generation containing eden, from- and
// to-space.
// 在JVM内部提供了多种方式来实现新生代的内存，如DefNewGeneration、ParNewGeneration和ASParNewGeneration等，由虚拟机的启动参数决定最终采用哪种方式进行实现。
// DefNewGeneration是一个young generation，包含了eden、from and to内存区，当虚拟机启动参数中没有指定垃圾回收算法时，默认使用该方式实现新生代
// DefNew: 是使用-XX:+UseSerialGC（新生代，老年代都使用串行回收收集器）时启用
// ParNew: 是使用-XX:+UseParNewGC（新生代使用并行收集器，老年代使用串行回收收集器）或者-XX:+UseConcMarkSweepGC(新生代使用并行收集器，老年代使用CMS)时启用。
//
// 原本HotSpotVM里并没有并行GC，当时只有NewGeneration。新生代，老年代都使用串行回收收集。后来准备加入新生代并行GC，就把NewGeneration改名为DefNewGeneration，然后把新加的并行版叫做ParNewGeneration。
// DefNewGeneration、ParNewGeneration都在Hotspot VM”分代式GC框架“内。但后来有个开发不愿意被这个框架憋着(证明了那句：所有壮举都不是在框架内产生的)，自已硬写了个新的并行GC。
// 测试后效果还不错。于是这个也放入VM的GC中。这就是我们现在看到的ParallelScavenge。
//
//  这个时候就出现个两个新生代的并行GC收集器：ParNewGeneration，ParallelScavenge。
//
//  (R大： Scavenge或者叫scavenging GC，其实就是copying GC的另一种叫法而已。HotSpot VM里的GC都是在minor GC收集器里用scavenging的，DefNew、ParNew和ParallelScavenge都是，
//  只不过DefNew是串行的copying GC，而后两者是并行的copying GC。 由此名字就可以知道，“ParallelScavenge”的初衷就是把“scavenge”给并行化。换句话说就是把minor GC并行化。至于full GC，那不是当初关注的重点。 )

// DefNewGeneration继承自Generation，其定义在defNewGeneration.hpp中
// 表示默认配置下CMS算法使用的年轻代
class DefNewGeneration: public Generation {
  friend class VMStructs;

protected:
    //对老年代的引用，初始化时为null，第一次GC时会赋值
  Generation* _next_gen;
    //将对象拷贝到老年代的分代年龄阈值，大于该值拷贝到老年代，否则拷贝到to区，该值在初始化时赋值成参数MaxTenuringThreshold，默认是15；每次GC结束后都会通过ageTable调整。
  uint        _tenuring_threshold;   // Tenuring threshold for next collection.
    //记录不同分代年龄的对象的总大小
  ageTable    _age_table;
  // Size of object to pretenure in words; command line provides bytes
  //表示年轻代中允许分配的对象的最大字宽数，默认是0，即无限制
  size_t      _pretenure_size_threshold_words;

  ageTable*   age_table() { return &_age_table; }

  // Initialize state to optimistically assume no promotion failure will
  // happen.
  void   init_assuming_no_promotion_failure();
  // True iff a promotion has failed in the current collection.
  //记录是否出现promotion失败的oop
  bool   _promotion_failed;
  bool   promotion_failed() { return _promotion_failed; }
    //记录promotion失败的对象信息
  PromotionFailedInfo _promotion_failed_info;

  // Handling promotion failure.  A young generation collection
  // can fail if a live object cannot be copied out of its
  // location in eden or from-space during the collection.  If
  // a collection fails, the young generation is left in a
  // consistent state such that it can be collected by a
  // full collection.
  //   Before the collection
  //     Objects are in eden or from-space
  //     All roots into the young generation point into eden or from-space.
  //
  //   After a failed collection
  //     Objects may be in eden, from-space, or to-space
  //     An object A in eden or from-space may have a copy B
  //       in to-space.  If B exists, all roots that once pointed
  //       to A must now point to B.
  //     All objects in the young generation are unmarked.
  //     Eden, from-space, and to-space will all be collected by
  //       the full collection.
  void handle_promotion_failure(oop);

  // In the absence of promotion failure, we wouldn't look at "from-space"
  // objects after a young-gen collection.  When promotion fails, however,
  // the subsequent full collection will look at from-space objects:
  // therefore we must remove their forwarding pointers.
  void remove_forwarding_pointers();

  // Preserve the mark of "obj", if necessary, in preparation for its mark
  // word being overwritten with a self-forwarding-pointer.
  void   preserve_mark_if_necessary(oop obj, markOop m);
  void   preserve_mark(oop obj, markOop m);    // work routine used by the above

  // Together, these keep <object with a preserved mark, mark value> pairs.
  // They should always contain the same number of elements.
  //记录promotion失败需要被保留的oop，如果对象头中包含锁，分代年龄等非初始化状态的信息，则需要单独保留，否则无法恢复
  Stack<oop, mtGC>     _objs_with_preserved_marks;
    //记录promotion失败需要被保留的对象头
  Stack<markOop, mtGC> _preserved_marks_of_objs;

  // Promotion failure handling
  //用来遍历_promo_failure_scan_stack中的oop所引用的其他对象
  ExtendedOopClosure *_promo_failure_scan_stack_closure;
  void set_promo_failure_scan_stack_closure(ExtendedOopClosure *scan_stack_closure) {
    _promo_failure_scan_stack_closure = scan_stack_closure;
  }

    //保存promotion失败的oop
  Stack<oop, mtGC> _promo_failure_scan_stack;
  void drain_promo_failure_scan_stack(void);
    //是否应该处理_promo_failure_scan_stack中保存的oop
  bool _promo_failure_drain_in_progress;

  // Performance Counters
  GenerationCounters*  _gen_counters;
  CSpaceCounters*      _eden_counters;
  CSpaceCounters*      _from_counters;
  CSpaceCounters*      _to_counters;

  // sizing information
  //当年轻代达到最大内存时对应的eden区的内存，即eden区的最大内存
  size_t               _max_eden_size;
    //当年轻代达到最大内存时对应的survivor区的内存，即survivor区的最大内存
  size_t               _max_survivor_size;

  // Allocation support
  // 是否从to区中分配对象
  bool _should_allocate_from_space;
  bool should_allocate_from_space() const {
    return _should_allocate_from_space;
  }
  void clear_should_allocate_from_space() {
    _should_allocate_from_space = false;
  }
  void set_should_allocate_from_space() {
    _should_allocate_from_space = true;
  }

  // Tenuring
  void adjust_desired_tenuring_threshold(GCTracer &tracer);

  // Spaces
  //三个内存区
  EdenSpace*       _eden_space;
  ContiguousSpace* _from_space;
    //注意to区正常情况下都是空的，只在垃圾回收的时候才使用
  ContiguousSpace* _to_space;

    //计时器
  STWGCTimer* _gc_timer;

  enum SomeProtectedConstants {
    // Generations are GenGrain-aligned and have size that are multiples of
    // GenGrain.
    MinFreeScratchWords = 100
  };

  // Return the size of a survivor space if this generation were of size
  // gen_size.
    //根据SurvivorRatio计算survivor的最大内存，该属性表示年轻代与survivor区内存的比值，默认是8,即默认配置下
    //survivor区约占年轻代内存的10分之一
  size_t compute_survivor_size(size_t gen_size, size_t alignment) const {
    size_t n = gen_size / (SurvivorRatio + 2);
      //向下做内存取整
    return n > alignment ? align_size_down(n, alignment) : alignment;
  }

 public:  // was "protected" but caused compile error on win32
    //  IsAliveClosure用来判断某个oop是否是存活的
  class IsAliveClosure: public BoolObjectClosure {
    Generation* _g;
  public:
    IsAliveClosure(Generation* g);
    bool do_object_b(oop p);
  };

  class KeepAliveClosure: public OopClosure {
  protected:
    ScanWeakRefClosure* _cl;
    CardTableRS* _rs;
    template <class T> void do_oop_work(T* p);
  public:
    KeepAliveClosure(ScanWeakRefClosure* cl);
    virtual void do_oop(oop* p);
    virtual void do_oop(narrowOop* p);
  };

  // FastKeepAliveClosure继承自KeepAliveClosure，只适用于DefNewGeneration，用于将某个oop标记成存活状态，具体来说就是将该对象拷贝到to区或者老年代，
  // 然后更新对象的对象头指针和BS对应的卡表项，从而让is_forwarded方法返回true
  class FastKeepAliveClosure: public KeepAliveClosure {
  protected:
    HeapWord* _boundary;
    template <class T> void do_oop_work(T* p);
  public:
    FastKeepAliveClosure(DefNewGeneration* g, ScanWeakRefClosure* cl);
    virtual void do_oop(oop* p);
    virtual void do_oop(narrowOop* p);
  };

  class EvacuateFollowersClosure: public VoidClosure {
    GenCollectedHeap* _gch;
    int _level;
    ScanClosure* _scan_cur_or_nonheap;
    ScanClosure* _scan_older;
  public:
    EvacuateFollowersClosure(GenCollectedHeap* gch, int level,
                             ScanClosure* cur, ScanClosure* older);
    void do_void();
  };

  // FastEvacuateFollowersClosure比较特殊，主要用来遍历所有被promote到老年代的对象，恢复他们的对象头，并且遍历其包含的所有引用类型属性
  class FastEvacuateFollowersClosure: public VoidClosure {
    GenCollectedHeap* _gch;
    int _level;
    DefNewGeneration* _gen;
    FastScanClosure* _scan_cur_or_nonheap;
    FastScanClosure* _scan_older;
  public:
    FastEvacuateFollowersClosure(GenCollectedHeap* gch, int level,
                                 DefNewGeneration* gen,
                                 FastScanClosure* cur,
                                 FastScanClosure* older);
    void do_void();
  };

 public:
  DefNewGeneration(ReservedSpace rs, size_t initial_byte_size, int level,
                   const char* policy="Copy");

  virtual void ref_processor_init();

  virtual Generation::Name kind() { return Generation::DefNew; }

  // Accessing spaces
  EdenSpace*       eden() const           { return _eden_space; }
  ContiguousSpace* from() const           { return _from_space;  }
  ContiguousSpace* to()   const           { return _to_space;    }

  virtual CompactibleSpace* first_compaction_space() const;

  // Space enquiries
  size_t capacity() const;
  size_t used() const;
  size_t free() const;
  size_t max_capacity() const;
  size_t capacity_before_gc() const;
  size_t unsafe_max_alloc_nogc() const;
  size_t contiguous_available() const;

  size_t max_eden_size() const              { return _max_eden_size; }
  size_t max_survivor_size() const          { return _max_survivor_size; }

  bool supports_inline_contig_alloc() const { return true; }
  HeapWord** top_addr() const;
  HeapWord** end_addr() const;

  // Thread-local allocation buffers
  // supports_tlab_allocation / tlab_capacity / tlab_used / unsafe_max_tlab_alloc
  //     这四个方法都是分配TLAB时使用的，其调用方主要是GenCollectedHeap
  // 在开启TLAB时，基本所有的小对象都是在TLAB中分配的，不会直接在eden区中分配，所以eden区的capacity和used就是该年轻代用于分配TLAB的capacity和used。
  bool supports_tlab_allocation() const { return true; }
  size_t tlab_capacity() const;
  size_t tlab_used() const;
  size_t unsafe_max_tlab_alloc() const;

  // Grow the generation by the specified number of bytes.
  // The size of bytes is assumed to be properly aligned.
  // Return true if the expansion was successful.
  bool expand(size_t bytes);

  // DefNewGeneration cannot currently expand except at
  // a GC.
  virtual bool is_maximal_no_gc() const { return true; }

  // Iteration
  void object_iterate(ObjectClosure* blk);

  void younger_refs_iterate(OopsInGenClosure* cl);

  void space_iterate(SpaceClosure* blk, bool usedOnly = false);

  // Allocation support
  // should_allocate方法判断当前DefNewGeneration是否支持指定大小的内存块的内存分配
  virtual bool should_allocate(size_t word_size, bool is_tlab) {
    assert(UseTLAB || !is_tlab, "Should not allocate tlab");

    size_t overflow_limit    = (size_t)1 << (BitsPerSize_t - LogHeapWordSize);

      //校验word_size大于0，不超过overflow_limit，不超过_pretenure_size_threshold_words
    const bool non_zero      = word_size > 0;
    const bool overflows     = word_size >= overflow_limit;
    const bool check_too_big = _pretenure_size_threshold_words > 0;
    const bool not_too_big   = word_size < _pretenure_size_threshold_words;
    const bool size_ok       = is_tlab || !check_too_big || not_too_big;

    bool result = !overflows &&
                  non_zero   &&
                  size_ok;

    return result;
  }

  HeapWord* allocate(size_t word_size, bool is_tlab);
  HeapWord* allocate_from_space(size_t word_size);

  HeapWord* par_allocate(size_t word_size, bool is_tlab);

  // Prologue & Epilogue
  virtual void gc_prologue(bool full);
  virtual void gc_epilogue(bool full);

  // Save the tops for eden, from, and to
  virtual void record_spaces_top();

  // Doesn't require additional work during GC prologue and epilogue
  virtual bool performs_in_place_marking() const { return false; }

  // Accessing marks
  void save_marks();
  void reset_saved_marks();
  bool no_allocs_since_save_marks();

  // Need to declare the full complement of closures, whether we'll
  // override them or not, or get message from the compiler:
  //   oop_since_save_marks_iterate_nv hides virtual function...
#define DefNew_SINCE_SAVE_MARKS_DECL(OopClosureType, nv_suffix) \
  void oop_since_save_marks_iterate##nv_suffix(OopClosureType* cl);

  ALL_SINCE_SAVE_MARKS_CLOSURES(DefNew_SINCE_SAVE_MARKS_DECL)

#undef DefNew_SINCE_SAVE_MARKS_DECL

  // For non-youngest collection, the DefNewGeneration can contribute
  // "to-space".
  virtual void contribute_scratch(ScratchBlock*& list, Generation* requestor,
                          size_t max_alloc_words);

  // Reset for contribution of "to-space".
  virtual void reset_scratch();

  // GC support
  virtual void compute_new_size();

  // Returns true if the collection is likely to be safely
  // completed. Even if this method returns true, a collection
  // may not be guaranteed to succeed, and the system should be
  // able to safely unwind and recover from that failure, albeit
  // at some additional cost. Override superclass's implementation.
  virtual bool collection_attempt_is_safe();

  virtual void collect(bool   full,
                       bool   clear_all_soft_refs,
                       size_t size,
                       bool   is_tlab);
  HeapWord* expand_and_allocate(size_t size,
                                bool is_tlab,
                                bool parallel = false);

  oop copy_to_survivor_space(oop old);
  uint tenuring_threshold() { return _tenuring_threshold; }

  // Performance Counter support
  void update_counters();

  // Printing
  virtual const char* name() const;
  virtual const char* short_name() const { return "DefNew"; }

  bool must_be_youngest() const { return true; }
  bool must_be_oldest() const { return false; }

  // PrintHeapAtGC support.
  void print_on(outputStream* st) const;

  void verify();

  bool promo_failure_scan_is_complete() const {
    return _promo_failure_scan_stack.is_empty();
  }

 protected:
  // If clear_space is true, clear the survivor spaces.  Eden is
  // cleared if the minimum size of eden is 0.  If mangle_space
  // is true, also mangle the space in debug mode.
  void compute_space_boundaries(uintx minimum_eden_size,
                                bool clear_space,
                                bool mangle_space);
  // Scavenge support
  void swap_spaces();
};

#endif // SHARE_VM_MEMORY_DEFNEWGENERATION_HPP
