/*
 * Copyright (c) 2001, 2014, Oracle and/or its affiliates. All rights reserved.
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
#include "gc_implementation/shared/collectorCounters.hpp"
#include "gc_implementation/shared/gcPolicyCounters.hpp"
#include "gc_implementation/shared/gcHeapSummary.hpp"
#include "gc_implementation/shared/gcTimer.hpp"
#include "gc_implementation/shared/gcTraceTime.hpp"
#include "gc_implementation/shared/gcTrace.hpp"
#include "gc_implementation/shared/spaceDecorator.hpp"
#include "memory/defNewGeneration.inline.hpp"
#include "memory/gcLocker.inline.hpp"
#include "memory/genCollectedHeap.hpp"
#include "memory/genOopClosures.inline.hpp"
#include "memory/genRemSet.hpp"
#include "memory/generationSpec.hpp"
#include "memory/iterator.hpp"
#include "memory/referencePolicy.hpp"
#include "memory/space.inline.hpp"
#include "oops/instanceRefKlass.hpp"
#include "oops/oop.inline.hpp"
#include "runtime/java.hpp"
#include "runtime/prefetch.inline.hpp"
#include "runtime/thread.inline.hpp"
#include "utilities/copy.hpp"
#include "utilities/stack.inline.hpp"
#include "utilities/slog.hpp"

PRAGMA_FORMAT_MUTE_WARNINGS_FOR_GCC

//
// DefNewGeneration functions.

// Methods of protected closure types.

//IsAliveClosure只适用于年轻代
DefNewGeneration::IsAliveClosure::IsAliveClosure(Generation* g) : _g(g) {
  assert(g->level() == 0, "Optimized for youngest gen.");
}
bool DefNewGeneration::IsAliveClosure::do_object_b(oop p) {
    //如果p不在年轻代或者p即将被复制则认为p是存活的
  return (HeapWord*)p >= _g->reserved().end() || p->is_forwarded();
}

DefNewGeneration::KeepAliveClosure::
KeepAliveClosure(ScanWeakRefClosure* cl) : _cl(cl) {
  GenRemSet* rs = GenCollectedHeap::heap()->rem_set();
  assert(rs->rs_kind() == GenRemSet::CardTable, "Wrong rem set kind.");
  _rs = (CardTableRS*)rs;
}

void DefNewGeneration::KeepAliveClosure::do_oop(oop* p)       { DefNewGeneration::KeepAliveClosure::do_oop_work(p); }
void DefNewGeneration::KeepAliveClosure::do_oop(narrowOop* p) { DefNewGeneration::KeepAliveClosure::do_oop_work(p); }


//FastKeepAliveClosure只适用于DefNewGeneration
DefNewGeneration::FastKeepAliveClosure::
FastKeepAliveClosure(DefNewGeneration* g, ScanWeakRefClosure* cl) :
  DefNewGeneration::KeepAliveClosure(cl) {
  _boundary = g->reserved().end();
}

void DefNewGeneration::FastKeepAliveClosure::do_oop(oop* p)       { DefNewGeneration::FastKeepAliveClosure::do_oop_work(p); }
void DefNewGeneration::FastKeepAliveClosure::do_oop(narrowOop* p) { DefNewGeneration::FastKeepAliveClosure::do_oop_work(p); }

DefNewGeneration::EvacuateFollowersClosure::
EvacuateFollowersClosure(GenCollectedHeap* gch, int level,
                         ScanClosure* cur, ScanClosure* older) :
  _gch(gch), _level(level),
  _scan_cur_or_nonheap(cur), _scan_older(older)
{}

void DefNewGeneration::EvacuateFollowersClosure::do_void() {
  do {
    _gch->oop_since_save_marks_iterate(_level, _scan_cur_or_nonheap,
                                       _scan_older);
  } while (!_gch->no_allocs_since_save_marks(_level));
}

DefNewGeneration::FastEvacuateFollowersClosure::
FastEvacuateFollowersClosure(GenCollectedHeap* gch, int level,
                             DefNewGeneration* gen,
                             FastScanClosure* cur, FastScanClosure* older) :
  _gch(gch), _level(level), _gen(gen),
  _scan_cur_or_nonheap(cur), _scan_older(older)
{}

void DefNewGeneration::FastEvacuateFollowersClosure::do_void() {
  do {
      //_level是年轻代的level
      //单线程GC时，年轻代的saved_mark_word就是top，所以oop_since_save_marks_iterate不做任何护理
      //此方法主要是遍历被promote到老年代的对象，因为是单线程执行，所以执行一遍后老年代的no_allocs_since_save_marks方法就会返回true，从而终止遍历
    _gch->oop_since_save_marks_iterate(_level, _scan_cur_or_nonheap,
                                       _scan_older);
  } while (!_gch->no_allocs_since_save_marks(_level));
  guarantee(_gen->promo_failure_scan_is_complete(), "Failed to finish scan");
}

ScanClosure::ScanClosure(DefNewGeneration* g, bool gc_barrier) :
    OopsInKlassOrGenClosure(g), _g(g), _gc_barrier(gc_barrier)
{
  assert(_g->level() == 0, "Optimized for youngest generation");
  _boundary = _g->reserved().end();
}

void ScanClosure::do_oop(oop* p)       { ScanClosure::do_oop_work(p); }
void ScanClosure::do_oop(narrowOop* p) { ScanClosure::do_oop_work(p); }

FastScanClosure::FastScanClosure(DefNewGeneration* g, bool gc_barrier) :
    OopsInKlassOrGenClosure(g), _g(g), _gc_barrier(gc_barrier)
{
  assert(_g->level() == 0, "Optimized for youngest generation");
    //初始化成年轻代的终止地址
  _boundary = _g->reserved().end();
}

void FastScanClosure::do_oop(oop* p)       { FastScanClosure::do_oop_work(p); }
void FastScanClosure::do_oop(narrowOop* p) { FastScanClosure::do_oop_work(p); }

void KlassScanClosure::do_klass(Klass* klass) {
#ifndef PRODUCT
  if (TraceScavenge) {
    ResourceMark rm;
    gclog_or_tty->print_cr("KlassScanClosure::do_klass %p, %s, dirty: %s",
                           klass,
                           klass->external_name(),
                           klass->has_modified_oops() ? "true" : "false");
  }
#endif

  // If the klass has not been dirtied we know that there's
  // no references into  the young gen and we can skip it.
  if (klass->has_modified_oops()) {
    if (_accumulate_modified_oops) {
        //将_accumulated_modified_oops置为1
      klass->accumulate_modified_oops();
    }

    // Clear this state since we're going to scavenge all the metadata.
      //将_modified_oops恢复成0
    klass->clear_modified_oops();

    // Tell the closure which Klass is being scanned so that it can be dirtied
    // if oops are left pointing into the young gen.
      //通知_scavenge_closure准备扫描klass
    _scavenge_closure->set_scanned_klass(klass);

      //执行的过程中如果该Klass对应的java_mirror还在年轻代则将该klass的_modified_oops再次置为1
      //这样做的目的是确保下一次年轻代GC时还会将该对象作为存活对象处理，直到将其promote到老年代为止
    klass->oops_do(_scavenge_closure);

    _scavenge_closure->set_scanned_klass(NULL);
  }
}

ScanWeakRefClosure::ScanWeakRefClosure(DefNewGeneration* g) :
  _g(g)
{
  assert(_g->level() == 0, "Optimized for youngest generation");
  _boundary = _g->reserved().end();
}

void ScanWeakRefClosure::do_oop(oop* p)       { ScanWeakRefClosure::do_oop_work(p); }
void ScanWeakRefClosure::do_oop(narrowOop* p) { ScanWeakRefClosure::do_oop_work(p); }

void FilteringClosure::do_oop(oop* p)       { FilteringClosure::do_oop_work(p); }
void FilteringClosure::do_oop(narrowOop* p) { FilteringClosure::do_oop_work(p); }

KlassScanClosure::KlassScanClosure(OopsInKlassOrGenClosure* scavenge_closure,
                                   KlassRemSet* klass_rem_set)
    : _scavenge_closure(scavenge_closure),
        //accumulate_modified_oops属性默认为false，老年代GC时会设置该属性
      _accumulate_modified_oops(klass_rem_set->accumulate_modified_oops()) {}


// 构造函数负责初始化年轻代相关属性及三个内存区
DefNewGeneration::DefNewGeneration(ReservedSpace rs,
                                   size_t initial_size,
                                   int level,
                                   const char* policy)
  : Generation(rs, initial_size, level),
    _promo_failure_drain_in_progress(false),
    _should_allocate_from_space(false)
{
  MemRegion cmr((HeapWord*)_virtual_space.low(),
                (HeapWord*)_virtual_space.high());
    //重置bs对应的内存区域
  Universe::heap()->barrier_set()->resize_covered_region(cmr);

    //has_soft_ended_eden方法的返回值取决于属性CMSIncrementalMode，默认为false
    //初始化三个内存区域
  if (GenCollectedHeap::heap()->collector_policy()->has_soft_ended_eden()) {
    _eden_space = new ConcEdenSpace(this);
  } else {
    _eden_space = new EdenSpace(this);
  }
  _from_space = new ContiguousSpace();
  _to_space   = new ContiguousSpace();

  if (_eden_space == NULL || _from_space == NULL || _to_space == NULL)
    vm_exit_during_initialization("Could not allocate a new gen space");

  // Compute the maximum eden and survivor space sizes. These sizes
  // are computed assuming the entire reserved space is committed.
  // These values are exported as performance counters.
    //计算survivor区和eden区的最大空间，即年轻代最大内存时survivor区和eden区的内存空间
  uintx alignment = GenCollectedHeap::heap()->collector_policy()->space_alignment();
  uintx size = _virtual_space.reserved_size();
  _max_survivor_size = compute_survivor_size(size, alignment);
  _max_eden_size = size - (2*_max_survivor_size);

  // allocate the performance counters

  // Generation counters -- generation 0, 3 subspaces
    //初始化性能统计的计数器
  _gen_counters = new GenerationCounters("new", 0, 3, &_virtual_space);
  _gc_counters = new CollectorCounters(policy, 0);

  _eden_counters = new CSpaceCounters("eden", 0, _max_eden_size, _eden_space,
                                      _gen_counters);
  _from_counters = new CSpaceCounters("s0", 1, _max_survivor_size, _from_space,
                                      _gen_counters);
  _to_counters = new CSpaceCounters("s1", 2, _max_survivor_size, _to_space,
                                    _gen_counters);

    //计算三个内存区的大小和边界，并初始化
  compute_space_boundaries(0, SpaceDecorator::Clear, SpaceDecorator::Mangle);
    //初始化计数器
  update_counters();
  _next_gen = NULL;
    //MaxTenuringThreshold表示属性tenuringThreshold的最大值，默认是15
  _tenuring_threshold = MaxTenuringThreshold;
    //PretenureSizeThreshold表示在DefNew代中分配的对象的最大字节数，默认是0，即无限制，这里是将其转换成字宽数
  _pretenure_size_threshold_words = PretenureSizeThreshold >> LogHeapWordSize;

    //初始化计时器
  _gc_timer = new (ResourceObj::C_HEAP, mtGC) STWGCTimer();
}

void DefNewGeneration::compute_space_boundaries(uintx minimum_eden_size,
                                                bool clear_space,
                                                bool mangle_space) {
  uintx alignment =
    GenCollectedHeap::heap()->collector_policy()->space_alignment();

  // If the spaces are being cleared (only done at heap initialization
  // currently), the survivor spaces need not be empty.
  // Otherwise, no care is taken for used areas in the survivor spaces
  // so check.
  assert(clear_space || (to()->is_empty() && from()->is_empty()),
    "Initialization of the survivor spaces assumes these are empty");

  // Compute sizes
    //根据年轻代的当前大小计算survivor与eden区的大小
  uintx size = _virtual_space.committed_size();
  uintx survivor_size = compute_survivor_size(size, alignment);
  uintx eden_size = size - (2*survivor_size);
  assert(eden_size > 0 && survivor_size <= eden_size, "just checking");

  if (eden_size < minimum_eden_size) {
    // May happen due to 64Kb rounding, if so adjust eden size back up
      //minimum_eden_size的值初始化时是0，在GC结束后且eden区不为空时则不为0
      //基于minimum_eden_size重新计算survivor与eden区的大小
    minimum_eden_size = align_size_up(minimum_eden_size, alignment);
    uintx maximum_survivor_size = (size - minimum_eden_size) / 2;
    uintx unaligned_survivor_size =
      align_size_down(maximum_survivor_size, alignment);
    survivor_size = MAX2(unaligned_survivor_size, alignment);
    eden_size = size - (2*survivor_size);
    assert(eden_size > 0 && survivor_size <= eden_size, "just checking");
    assert(eden_size >= minimum_eden_size, "just checking");
  }

    //计算三个内存区的边界
  char *eden_start = _virtual_space.low();
  char *from_start = eden_start + eden_size;
  char *to_start   = from_start + survivor_size;
  char *to_end     = to_start   + survivor_size;

  assert(to_end == _virtual_space.high(), "just checking");
  assert(Space::is_aligned((HeapWord*)eden_start), "checking alignment");
  assert(Space::is_aligned((HeapWord*)from_start), "checking alignment");
  assert(Space::is_aligned((HeapWord*)to_start),   "checking alignment");

  MemRegion edenMR((HeapWord*)eden_start, (HeapWord*)from_start);
  MemRegion fromMR((HeapWord*)from_start, (HeapWord*)to_start);
  MemRegion toMR  ((HeapWord*)to_start, (HeapWord*)to_end);

  // A minimum eden size implies that there is a part of eden that
  // is being used and that affects the initialization of any
  // newly formed eden.
    //minimum_eden_size大于0表示当前eden区有部分空间被使用了
  bool live_in_eden = minimum_eden_size > 0;

  // If not clearing the spaces, do some checking to verify that
  // the space are already mangled.
  if (!clear_space) {
    // Must check mangling before the spaces are reshaped.  Otherwise,
    // the bottom or end of one space may have moved into another
    // a failure of the check may not correctly indicate which space
    // is not properly mangled.
    if (ZapUnusedHeapArea) {
        //校验三个区是否经过mangle填充处理
      HeapWord* limit = (HeapWord*) _virtual_space.high();
      eden()->check_mangled_unused_area(limit);
      from()->check_mangled_unused_area(limit);
        to()->check_mangled_unused_area(limit);
    }
  }

  // Reset the spaces for their new regions.
    //初始化三个区
  eden()->initialize(edenMR,
                     clear_space && !live_in_eden,
                     SpaceDecorator::Mangle);
  // If clear_space and live_in_eden, we will not have cleared any
  // portion of eden above its top. This can cause newly
  // expanded space not to be mangled if using ZapUnusedHeapArea.
  // We explicitly do such mangling here.
  if (ZapUnusedHeapArea && clear_space && live_in_eden && mangle_space) {
    eden()->mangle_unused_area();
  }
  from()->initialize(fromMR, clear_space, mangle_space);
  to()->initialize(toMR, clear_space, mangle_space);

  // Set next compaction spaces.
    //from区作为eden区的下一个compaction_space
  eden()->set_next_compaction_space(from());
  // The to-space is normally empty before a compaction so need
  // not be considered.  The exception is during promotion
  // failure handling when to-space can contain live objects.
    //to区在开始compaction前正常都是空的，所以这里不考虑
  from()->set_next_compaction_space(NULL);
}

void DefNewGeneration::swap_spaces() {
    //交换from区和to区对应的内存区域
  ContiguousSpace* s = from();
  _from_space        = to();
  _to_space          = s;
    //重置eden区的next_compaction_space
  eden()->set_next_compaction_space(from());
  // The to-space is normally empty before a compaction so need
  // not be considered.  The exception is during promotion
  // failure handling when to-space can contain live objects.
    //将原来的to区的next_compaction_space置为null
  from()->set_next_compaction_space(NULL);

  if (UsePerfData) {
    CSpaceCounters* c = _from_counters;
    _from_counters = _to_counters;
    _to_counters = c;
  }
}

bool DefNewGeneration::expand(size_t bytes) {
  MutexLocker x(ExpandHeap_lock);
  HeapWord* prev_high = (HeapWord*) _virtual_space.high();
    //尝试扩容指定大小的内存
  bool success = _virtual_space.expand_by(bytes);
  if (success && ZapUnusedHeapArea) {
    // Mangle newly committed space immediately because it
    // can be done here more simply that after the new
    // spaces have been computed.
      //扩容成功，执行mangle操作
    HeapWord* new_high = (HeapWord*) _virtual_space.high();
    MemRegion mangle_region(prev_high, new_high);
    SpaceMangler::mangle_region(mangle_region);
  }

  // Do not attempt an expand-to-the reserve size.  The
  // request should properly observe the maximum size of
  // the generation so an expand-to-reserve should be
  // unnecessary.  Also a second call to expand-to-reserve
  // value potentially can cause an undue expansion.
  // For example if the first expand fail for unknown reasons,
  // but the second succeeds and expands the heap to its maximum
  // value.
  if (GC_locker::is_active()) {
    if (PrintGC && Verbose) {
      gclog_or_tty->print_cr("Garbage collection disabled, "
        "expanded heap instead");
    }
  }

  return success;
}


// compute_new_size用于在GC结束后根据老年代的大小和NewRatio，NewSizeThreadIncrease两个参数重新计算年轻代的大小，并做适当的扩容或者缩容处理。
void DefNewGeneration::compute_new_size() {
  // This is called after a gc that includes the following generation
  // (which is required to exist.)  So from-space will normally be empty.
  // Note that we check both spaces, since if scavenge failed they revert roles.
  // If not we bail out (otherwise we would have to relocate the objects)
    //正常from区或者to区在GC结束后都是空的，如果非空则说明堆内存已满
  if (!from()->is_empty() || !to()->is_empty()) {
    return;
  }

  int next_level = level() + 1;
  GenCollectedHeap* gch = GenCollectedHeap::heap();
  assert(next_level < gch->_n_gens,
         "DefNewGeneration cannot be an oldest gen");

  Generation* next_gen = gch->_gens[next_level];
  size_t old_size = next_gen->capacity();
  size_t new_size_before = _virtual_space.committed_size();
  size_t min_new_size = spec()->init_size();
  size_t max_new_size = reserved().byte_size();
  assert(min_new_size <= new_size_before &&
         new_size_before <= max_new_size,
         "just checking");
  // All space sizes must be multiples of Generation::GenGrain.
  size_t alignment = Generation::GenGrain;

  // Compute desired new generation size based on NewRatio and
  // NewSizeThreadIncrease
    // 计算年轻代新的大小，基于NewRatio和NewSizeThreadIncrease
  size_t desired_new_size = old_size/NewRatio;
    //获取非后台线程数
  int threads_count = Threads::number_of_non_daemon_threads();
    //NewSizeThreadIncrease表示每个非后台线程增加的年轻代的内存大小，默认是4k
  size_t thread_increase_size = threads_count * NewSizeThreadIncrease;
  desired_new_size = align_size_up(desired_new_size + thread_increase_size, alignment);

  // Adjust new generation size
  desired_new_size = MAX2(MIN2(desired_new_size, max_new_size), min_new_size);
  assert(desired_new_size <= max_new_size, "just checking");

  bool changed = false;
  if (desired_new_size > new_size_before) {
      //如果大于原来的，则需要扩容
    size_t change = desired_new_size - new_size_before;
    assert(change % alignment == 0, "just checking");
      //尝试扩容指定大小的内存
    if (expand(change)) {
        //扩容成功，置为true
       changed = true;
    }
    // If the heap failed to expand to the desired size,
    // "changed" will be false.  If the expansion failed
    // (and at this point it was expected to succeed),
    // ignore the failure (leaving "changed" as false).
  }
  if (desired_new_size < new_size_before && eden()->is_empty()) {
    // bail out of shrinking if objects in eden
      //则eden区是空的情形下才允许缩容，非空的条件下缩容有可能导致对象丢失
    size_t change = new_size_before - desired_new_size;
    assert(change % alignment == 0, "just checking");
    _virtual_space.shrink_by(change);
    changed = true;
  }
  if (changed) {
    // The spaces have already been mangled at this point but
    // may not have been cleared (set top = bottom) and should be.
    // Mangling was done when the heap was being expanded.
      //如果改变，则重新计算三个区的内存边界并初始化，compute_space_boundaries方法会保证eden区的内存足够大
    compute_space_boundaries(eden()->used(),
                             SpaceDecorator::Clear,
                             SpaceDecorator::DontMangle);
    MemRegion cmr((HeapWord*)_virtual_space.low(),
                  (HeapWord*)_virtual_space.high());
      //重置bs对应的覆盖区域
    Universe::heap()->barrier_set()->resize_covered_region(cmr);
    if (Verbose && PrintGC) {
      size_t new_size_after  = _virtual_space.committed_size();
      size_t eden_size_after = eden()->capacity();
      size_t survivor_size_after = from()->capacity();
      gclog_or_tty->print("New generation size " SIZE_FORMAT "K->"
        SIZE_FORMAT "K [eden="
        SIZE_FORMAT "K,survivor=" SIZE_FORMAT "K]",
        new_size_before/K, new_size_after/K,
        eden_size_after/K, survivor_size_after/K);
      if (WizardMode) {
        gclog_or_tty->print("[allowed " SIZE_FORMAT "K extra for %d threads]",
          thread_increase_size/K, threads_count);
      }
      gclog_or_tty->cr();
    }
  }
}

void DefNewGeneration::younger_refs_iterate(OopsInGenClosure* cl) {
  assert(false, "NYI -- are you sure you want to call this?");
}


size_t DefNewGeneration::capacity() const {
  return eden()->capacity()
       + from()->capacity();  // to() is only used during scavenge
}


size_t DefNewGeneration::used() const {
    //to区通常是空的，不计入此处
  return eden()->used()
       + from()->used();      // to() is only used during scavenge
}


size_t DefNewGeneration::free() const {
  return eden()->free()
       + from()->free();      // to() is only used during scavenge
}

size_t DefNewGeneration::max_capacity() const {
  const size_t alignment = GenCollectedHeap::heap()->collector_policy()->space_alignment();
  const size_t reserved_bytes = reserved().byte_size();
  return reserved_bytes - compute_survivor_size(reserved_bytes, alignment);
}

size_t DefNewGeneration::unsafe_max_alloc_nogc() const {
  return eden()->free();
}

size_t DefNewGeneration::capacity_before_gc() const {
  return eden()->capacity();
}

size_t DefNewGeneration::contiguous_available() const {
  return eden()->free();
}


HeapWord** DefNewGeneration::top_addr() const { return eden()->top_addr(); }
HeapWord** DefNewGeneration::end_addr() const { return eden()->end_addr(); }

void DefNewGeneration::object_iterate(ObjectClosure* blk) {
  eden()->object_iterate(blk);
  from()->object_iterate(blk);
}


void DefNewGeneration::space_iterate(SpaceClosure* blk,
                                     bool usedOnly) {
  blk->do_space(eden());
  blk->do_space(from());
  blk->do_space(to());
}

// The last collection bailed out, we are running out of heap space,
// so we try to allocate the from-space, too.
//尝试从from区分配对象
HeapWord* DefNewGeneration::allocate_from_space(size_t size) {
  HeapWord* result = NULL;
  if (Verbose && PrintGCDetails) {
    gclog_or_tty->print("DefNewGeneration::allocate_from_space(%u):"
                        "  will_fail: %s"
                        "  heap_lock: %s"
                        "  free: " SIZE_FORMAT,
                        size,
                        GenCollectedHeap::heap()->incremental_collection_will_fail(false /* don't consult_young */) ?
                          "true" : "false",
                        Heap_lock->is_locked() ? "locked" : "unlocked",
                        from()->free());
  }
  if (should_allocate_from_space() || GC_locker::is_active_and_needs_gc()) {
    if (Heap_lock->owned_by_self() ||
        (SafepointSynchronize::is_at_safepoint() &&
         Thread::current()->is_VM_thread())) {
      // If the Heap_lock is not locked by this thread, this will be called
      // again later with the Heap_lock held.
        //只有在上述条件成立时才允许从from区中分配
      result = from()->allocate(size);
    } else if (PrintGC && Verbose) {
      gclog_or_tty->print_cr("  Heap_lock is not owned by self");
    }
  } else if (PrintGC && Verbose) {
    gclog_or_tty->print_cr("  should_allocate_from_space: NOT");
  }
  if (PrintGC && Verbose) {
    gclog_or_tty->print_cr("  returns %s", result == NULL ? "NULL" : "object");
  }
  return result;
}

HeapWord* DefNewGeneration::expand_and_allocate(size_t size,
                                                bool   is_tlab,
                                                bool   parallel) {
  // We don't attempt to expand the young generation (but perhaps we should.)
  return allocate(size, is_tlab);
}

void DefNewGeneration::adjust_desired_tenuring_threshold(GCTracer &tracer) {
  // Set the desired survivor size to half the real survivor space
    //重置tenuring_threshold，注意此处传入的是to区的容量，因为对象是往to区拷贝的
  _tenuring_threshold =
    age_table()->compute_tenuring_threshold(to()->capacity()/HeapWordSize, tracer);
}

/**
 *  collect就是DefNewGeneration执行GC的核心方法了，其中根节点遍历由GenCollectedHeap::gen_process_roots实现，找到的根节点oop由FastScanClosure处理，
 *  如果该oop是年轻代的则执行promote，拷贝到to区或者老年代，然后让该oop指向新的对象地址。除根节点oop之外，还需要遍历新创建的klass，
 *  由FastScanClosure以同样的方式处理器java_mirror属性，即对应类的Class实例。遍历根节点完成后gen_process_roots还需要遍历老年代对应的脏的卡表项对应的内存区域中的对象，
 *  需要遍历这些对象的同样在脏的内存区域中的引用类型属性，即老年代对象所引用新的年轻代的oop，同样将这些oop拷贝到to区或者老年代，然后让该oop指向新的对象地址；
 *  注意遍历脏的卡表项对应内存区域中的对象时使用的FastScanClosure的第二个参数为true，执行完上述操作后需要将这些oop对应的卡表项置为youngergen_card。
 *
 *
 *  本方法主要完成引用遍历前后的处理逻辑，这里重点关注年轻代三个区的使用。参考DefNewGeneration中分析的allocate方法及其相关方法的实现可知，
 *  年轻代对象分配主要在eden区，只有在eden区和老年代都满了导致promote失败才可能在from区中分配内存，正常情况下to区是空的，GC引用遍历时会遍历eden区和from区的对象，
 *  这个过程中如果存活对象的分代年龄小于tenuring_threshold则会拷贝到to区中并增加分代年龄，大于该阈值的就拷贝到老年代中，遍历结束后如果没有promote失败则认为所有存活对象都已成功复制，
 *  会清空eden区和from区，然后交换from区和to区，即空的from区变成to区，包含有存活对象的to区变成from区，如此循环往复，to区会一直是空的；如果老年代空间不足，出现promote失败的情形，
 *  则eden区和from区存在尚未复制的存活对象，则不能清空此时的eden区和from区，这时需要将eden区和from区中对象的对象头恢复成初始状态，即去掉forward指针，然后交换from区和to区，
 *  并且将交换后的to区作为from区的next_compaction_space（正常是null），从而尽可能的利用剩余的年轻代内存空间，此时的to区因为是原来的包含存活对象的from区，所以不是空的。
 *
 * 此方法的四个参数只有clear_all_soft_refs是有用的参数，如果为true表示会清除所有的软引用，如果是false则按照默认逻辑处理
 *
 */
void DefNewGeneration::collect(bool   full,
                               bool   clear_all_soft_refs,
                               size_t size,
                               bool   is_tlab) {
  slog_debug("进入hotspot/src/share/vm/memory/defNewGeneration.cpp中的DefNewGeneration::collect函数...");
  assert(full || size > 0, "otherwise we don't want to collect");

  GenCollectedHeap* gch = GenCollectedHeap::heap();

    //记录GC的开始时间和原因
  _gc_timer->register_gc_start();
  DefNewTracer gc_tracer;
  gc_tracer.report_gc_start(gch->gc_cause(), _gc_timer->gc_start());

  _next_gen = gch->next_gen(this);

  // If the next generation is too full to accommodate promotion
  // from this generation, pass on collection; let the next generation
  // do it.
    //判断老年代是否有足够的空间保存年轻代复制过去的对象
  if (!collection_attempt_is_safe()) {
    if (Verbose && PrintGCDetails) {
      gclog_or_tty->print(" :: Collection attempt not safe :: ");
    }
      //通知GCH老年代空间不足
    gch->set_incremental_collection_failed(); // Slight lie: we did not even attempt one
      //老年代空间不足，终止年轻代的GC
    return;
  }
  assert(to()->is_empty(), "Else not collection_attempt_is_safe");

    //将_promotion_failed属性置为false，记录promote失败信息的PromotionFailedInfo重置成初始状态
  init_assuming_no_promotion_failure();

  GCTraceTime t1(GCCauseString("GC", gch->gc_cause()), PrintGC && !PrintGCDetails, true, NULL, gc_tracer.gc_id());
  // Capture heap used before collection (for printing).
  size_t gch_prev_used = gch->used();

    //设置GC Tracer
  gch->trace_heap_before_gc(&gc_tracer);

  SpecializationStats::clear();

  // These can be shared for all code paths
    //初始化两个遍历器
  IsAliveClosure is_alive(this);
  ScanWeakRefClosure scan_weak_ref(this);

    //重置ageTable
  age_table()->clear();
    //重置to区
  to()->clear(SpaceDecorator::Mangle);

    //重置cur_youngergen_card_val，并行遍历脏的卡表项时使用
  gch->rem_set()->prepare_for_younger_refs_iterate(false);

  assert(gch->no_allocs_since_save_marks(0),
         "save marks have not been newly set.");

  // Not very pretty.
  CollectorPolicy* cp = gch->collector_policy();

    //FastScanClosure用来遍历年轻代中的存活对象oop，第二个参数为true，表示会将oop对应的卡表项置为youngergen_card
  FastScanClosure fsc_with_no_gc_barrier(this, false);
  FastScanClosure fsc_with_gc_barrier(this, true);

    //KlassScanClosure用来遍历在上一次GC到当前GC之间创建的新的Klass对应的Class实例
  KlassScanClosure klass_scan_closure(&fsc_with_no_gc_barrier,
                                      gch->rem_set()->klass_rem_set());
    //CLDToKlassAndOopClosure用来遍历一个ClassLoader加载的所有类对应的Class实例和依赖等
  CLDToKlassAndOopClosure cld_scan_closure(&klass_scan_closure,
                                           &fsc_with_no_gc_barrier,
                                           false);

    //设置promote失败时的遍历器
  set_promo_failure_scan_stack_closure(&fsc_with_no_gc_barrier);
    //FastEvacuateFollowersClosure主要用来遍历被promote到老年代的对象，恢复其对象头并遍历其引用类型属性
  FastEvacuateFollowersClosure evacuate_followers(gch, _level, this,
                                                  &fsc_with_no_gc_barrier,
                                                  &fsc_with_gc_barrier);

  assert(gch->no_allocs_since_save_marks(0),
         "save marks have not been newly set.");

    //执行根节点遍历以及老年代新应用的oop遍历
  gch->gen_process_roots(_level, //level就是0
                         true,  // Process younger gens, if any, 因为level是0，所以此参数实际无意义
                                // as strong roots.
                         true,  // activate StrongRootsScope StrongRootsScope的active入参为true
                         GenCollectedHeap::SO_ScavengeCodeCache,    //只遍历nmethod中的oop
                         GenCollectedHeap::StrongAndWeakRoots,  //StrongAndWeakRoots是静态常量，值为false，表示会遍历weak root，如StringTable中的String对象
                         &fsc_with_no_gc_barrier,
                         &fsc_with_gc_barrier,
                         &cld_scan_closure);

  // "evacuate followers".
    //遍历所有promote到老年代的对象，恢复其对象头，遍历其引用类型属性
  evacuate_followers.do_void();

  FastKeepAliveClosure keep_alive(this, &scan_weak_ref);
  ReferenceProcessor* rp = ref_processor();
  rp->setup_policy(clear_all_soft_refs);
    //处理do_void方法遍历引用类型属性过程中找到的Reference实例，如果该实例的referent对象是存活的，则从待处理列表中移除，否则将referent属性置为null
  const ReferenceProcessorStats& stats =
  rp->process_discovered_references(&is_alive, &keep_alive, &evacuate_followers,
                                    NULL, _gc_timer, gc_tracer.gc_id());
  gc_tracer.report_gc_reference_stats(stats);

  if (!_promotion_failed) {
    // Swap the survivor spaces.
      //promote没有失败的
      //重置清空eden区和from区
    eden()->clear(SpaceDecorator::Mangle);
    from()->clear(SpaceDecorator::Mangle);
    if (ZapUnusedHeapArea) {
      // This is now done here because of the piece-meal mangling which
      // can check for valid mangling at intermediate points in the
      // collection(s).  When a minor collection fails to collect
      // sufficient space resizing of the young generation can occur
      // an redistribute the spaces in the young generation.  Mangle
      // here so that unzapped regions don't get distributed to
      // other spaces.
      to()->mangle_unused_area();
    }
      //交换from区和to区
    swap_spaces();

      //交换后，原来的from区变成to区，必须是空的
    assert(to()->is_empty(), "to space should be empty now");

      //调整tenuring_threshold
    adjust_desired_tenuring_threshold(gc_tracer);

    // A successful scavenge should restart the GC time limit count which is
    // for full GC's.
    AdaptiveSizePolicy* size_policy = gch->gen_policy()->size_policy();
      //重置gc_overhead_limit_count
    size_policy->reset_gc_overhead_limit_count();
    if (PrintGC && !PrintGCDetails) {
      gch->print_heap_change(gch_prev_used);
    }
    assert(!gch->incremental_collection_failed(), "Should be clear");
  } else {
      //如果存在promote失败的情形
    assert(_promo_failure_scan_stack.is_empty(), "post condition");
      //drain_promo_failure_scan_stack方法会负责处理掉里面保存的oop，遍历其所引用的其他oop，找到的oop的处理逻辑就是fsc_with_no_gc_barrier
      //释放_promo_failure_scan_stack的内存
    _promo_failure_scan_stack.clear(true); // Clear cached segments.

      //移除from区和eden区包含的对象的froward指针
    remove_forwarding_pointers();
    if (PrintGCDetails) {
      gclog_or_tty->print(" (promotion failed) ");
    }
    // Add to-space to the list of space to compact
    // when a promotion failure has occurred.  In that
    // case there can be live objects in to-space
    // as a result of a partial evacuation of eden
    // and from-space.
      //交换from区和to区，注意此时eden区和from区因为promote失败所以不是空的，还有存活对象
    swap_spaces();   // For uniformity wrt ParNewGeneration.
      //将to区作为from区的next_compaction_space，正常为NULL
    from()->set_next_compaction_space(to());
      //将incremental_collection_failed置为true
    gch->set_incremental_collection_failed();

    // Inform the next generation that a promotion failure occurred.
      //通知老年代promote失败，CMS老年代实际不做处理
    _next_gen->promotion_failure_occurred();
    gc_tracer.report_promotion_failed(_promotion_failed_info);

    // Reset the PromotionFailureALot counters.
    NOT_PRODUCT(Universe::heap()->reset_promotion_should_fail();)
  }
  // set new iteration safe limit for the survivor spaces
    //设置并行遍历时的边界
  from()->set_concurrent_iteration_safe_limit(from()->top());
  to()->set_concurrent_iteration_safe_limit(to()->top());
  SpecializationStats::print();

  // We need to use a monotonically non-decreasing time in ms
  // or we will see time-warp warnings and os::javaTimeMillis()
  // does not guarantee monotonicity.
    //更新GC完成时间
  jlong now = os::javaTimeNanos() / NANOSECS_PER_MILLISEC;
  update_time_of_last_gc(now);

  gch->trace_heap_after_gc(&gc_tracer);
  gc_tracer.report_tenuring_threshold(tenuring_threshold());

  _gc_timer->register_gc_end();

    //更新GC日志
  gc_tracer.report_gc_end(_gc_timer->gc_end(), _gc_timer->time_partitions());
}

class RemoveForwardPointerClosure: public ObjectClosure {
public:
  void do_object(oop obj) {
    obj->init_mark();
  }
};

void DefNewGeneration::init_assuming_no_promotion_failure() {
  _promotion_failed = false;
  _promotion_failed_info.reset();
  from()->set_next_compaction_space(NULL);
}

void DefNewGeneration::remove_forwarding_pointers() {
  RemoveForwardPointerClosure rspc;
    //遍历eden区和from区中的对象，将对象头恢复成初始状态，即去掉原来对象头中包含的forward指针，即该对象拷贝的目标地址
  eden()->object_iterate(&rspc);
  from()->object_iterate(&rspc);

  // Now restore saved marks, if any.
  assert(_objs_with_preserved_marks.size() == _preserved_marks_of_objs.size(),
         "should be the same");
    //遍历_objs_with_preserved_marks，恢复其中保存的oop的对象头，里面的对象都是promote失败的对象
  while (!_objs_with_preserved_marks.is_empty()) {
    oop obj   = _objs_with_preserved_marks.pop();
    markOop m = _preserved_marks_of_objs.pop();
    obj->set_mark(m);
  }
    //清空两个栈
  _objs_with_preserved_marks.clear(true);
  _preserved_marks_of_objs.clear(true);
}

//保留指定的对象和对象头
void DefNewGeneration::preserve_mark(oop obj, markOop m) {
  assert(_promotion_failed && m->must_be_preserved_for_promotion_failure(obj),
         "Oversaving!");
  _objs_with_preserved_marks.push(obj);
  _preserved_marks_of_objs.push(m);
}

void DefNewGeneration::preserve_mark_if_necessary(oop obj, markOop m) {
    //是否要保留promotion失败的对象，对象头中包含锁，分代年龄等非初始状态的信息时需要单独保留对象头,否则无法恢复成原来的状态
  if (m->must_be_preserved_for_promotion_failure(obj)) {
    preserve_mark(obj, m);
  }
}

void DefNewGeneration::handle_promotion_failure(oop old) {
  if (PrintPromotionFailure && !_promotion_failed) {
    gclog_or_tty->print(" (promotion failure size = " SIZE_FORMAT ") ",
                        old->size());
  }
  _promotion_failed = true;
  _promotion_failed_info.register_copy_failure(old->size());
  preserve_mark_if_necessary(old, old->mark());
  // forward to self
    //将old的对象头指针指向它自己
  old->forward_to(old);

    //保存promotion失败的对象
  _promo_failure_scan_stack.push(old);

    //_promo_failure_drain_in_progress初始化为false
  if (!_promo_failure_drain_in_progress) {
    // prevent recursion in copy_to_survivor_space()
    _promo_failure_drain_in_progress = true;
    drain_promo_failure_scan_stack();
    _promo_failure_drain_in_progress = false;
  }
}

// copy_to_survivor_space会将对象拷贝到to区或者老年代，如果对象的分代年龄大于tenuring_threshold或者从to区申请内存失败则拷贝到老年代，否则拷贝到to区；
// 如果拷贝到老年代失败，则_promotion_failed置为true，并将该对象保存到_promo_failure_scan_stack栈中。
oop DefNewGeneration::copy_to_survivor_space(oop old) {
  assert(is_in_reserved(old) && !old->is_forwarded(),
         "shouldn't be scavenging this oop");
  size_t s = old->size();
  oop obj = NULL;

  // Try allocating obj in to-space (unless too old)
  if (old->age() < tenuring_threshold()) {
      //如果对象的年龄低于tenuring_threshold，则该在to区申请一块同样大小的内存
    obj = (oop) to()->allocate_aligned(s);
  }

  // Otherwise try allocating obj tenured
  if (obj == NULL) {
      //如果如果对象的年龄大于tenuring_threshold或者to区申请内存失败
      //则尝试将该对象复制到老年代
    obj = _next_gen->promote(old, s);
    if (obj == NULL) {
        //复制失败
      handle_promotion_failure(old);
      return old;
    }
  } else {
    // Prefetch beyond obj
      //to区中申请内存成功
    const intx interval = PrefetchCopyIntervalInBytes;
    Prefetch::write(obj, interval);

    // Copy obj
      //对象复制
    Copy::aligned_disjoint_words((HeapWord*)old, (HeapWord*)obj, s);

    // Increment age if obj still in new generation
      //增加年龄，并修改age_table，增加对应年龄的总对象大小
      //注意此处是增加复制对象而非原来对象的分代年龄
    obj->incr_age();
    age_table()->add(obj, s);
  }

  // Done, insert forward pointer to obj in this header
    //将对象头指针指向新地址
  old->forward_to(obj);

  return obj;
}

//用于移除并处理_promo_failure_scan_stack中保存的对象
void DefNewGeneration::drain_promo_failure_scan_stack() {
  while (!_promo_failure_scan_stack.is_empty()) {
     oop obj = _promo_failure_scan_stack.pop();
      //注意promote失败并不会终止根节点遍历，相反还会遍历其引用类型属性
     obj->oop_iterate(_promo_failure_scan_stack_closure);
  }
}

void DefNewGeneration::save_marks() {
  eden()->set_saved_mark();
  to()->set_saved_mark();
  from()->set_saved_mark();
}


void DefNewGeneration::reset_saved_marks() {
  eden()->reset_saved_mark();
  to()->reset_saved_mark();
  from()->reset_saved_mark();
}


//执行do_collect方法前，会先执行GenCollectedHeap::save_marks方法，因此年轻代的此方法返回true
bool DefNewGeneration::no_allocs_since_save_marks() {
  assert(eden()->saved_mark_at_top(), "Violated spec - alloc in eden");
  assert(from()->saved_mark_at_top(), "Violated spec - alloc in from");
  return to()->saved_mark_at_top();
}

/**
 * 该方法一组通过宏定义的多个方法，用于遍历年轻代三个Space的saved_mark_word属性到top属性之间的对象所引用的对象
 */
#define DefNew_SINCE_SAVE_MARKS_DEFN(OopClosureType, nv_suffix) \
                                                                \
void DefNewGeneration::                                         \
oop_since_save_marks_iterate##nv_suffix(OopClosureType* cl) {   \
  cl->set_generation(this);                                     \
  eden()->oop_since_save_marks_iterate##nv_suffix(cl);          \
  to()->oop_since_save_marks_iterate##nv_suffix(cl);            \
  from()->oop_since_save_marks_iterate##nv_suffix(cl);          \
  cl->reset_generation();                                       \
  save_marks();                                                 \
}

ALL_SINCE_SAVE_MARKS_CLOSURES(DefNew_SINCE_SAVE_MARKS_DEFN)

#undef DefNew_SINCE_SAVE_MARKS_DEFN

// contribute_scratch方法用于老年代从年轻代的to区申请一块指定大小的内存
void DefNewGeneration::contribute_scratch(ScratchBlock*& list, Generation* requestor,
                                         size_t max_alloc_words) {
    //必须是老年代请求，level大于年轻代的level
  if (requestor == this || _promotion_failed) return;
  assert(requestor->level() > level(), "DefNewGeneration must be youngest");

  /* $$$ Assert this?  "trace" is a "MarkSweep" function so that's not appropriate.
  if (to_space->top() > to_space->bottom()) {
    trace("to_space not empty when contribute_scratch called");
  }
  */

  ContiguousSpace* to_space = to();
  assert(to_space->end() >= to_space->top(), "pointers out of order");
  size_t free_words = pointer_delta(to_space->end(), to_space->top());
    //注意此处并未校验free_words是否大于max_alloc_words，也并未在分配结束后修改top属性
  if (free_words >= MinFreeScratchWords) {
      //构造一个ScratchBlock实例
    ScratchBlock* sb = (ScratchBlock*)to_space->top();
    sb->num_words = free_words;
      //将list插入到sb的后面
    sb->next = list;
    list = sb;
  }
}

// reset_scratch用于重置to区，如果ZapUnusedHeapArea为true，则将整个to区重新填充
void DefNewGeneration::reset_scratch() {
  // If contributing scratch in to_space, mangle all of
  // to_space if ZapUnusedHeapArea.  This is needed because
  // top is not maintained while using to-space as scratch.
    //因为top属性未修改，所以是整个to区都被mangle了
  if (ZapUnusedHeapArea) {
    to()->mangle_unused_area_complete();
  }
}

//该方法返回尝试回收垃圾是否是安全的
bool DefNewGeneration::collection_attempt_is_safe() {
  if (!to()->is_empty()) {
    if (Verbose && PrintGCDetails) {
      gclog_or_tty->print(" :: to is not empty :: ");
    }
      //如果to区非空则返回false，正常都是空的
    return false;
  }
  if (_next_gen == NULL) {
      //初始化_next_gen，DefNewGeneration第一次准备垃圾回收前_next_gen一直为null
    GenCollectedHeap* gch = GenCollectedHeap::heap();
    _next_gen = gch->next_gen(this);
  }
    //判断next_gen是否有充足的空间，允许年轻代的对象复制到老年代中
  return _next_gen->promotion_attempt_is_safe(used());
}

// gc_epilogue是在GC结束后调用的尾处理
void DefNewGeneration::gc_epilogue(bool full) {
  DEBUG_ONLY(static bool seen_incremental_collection_failed = false;)

  assert(!GC_locker::is_active(), "We should not be executing here");
  // Check if the heap is approaching full after a collection has
  // been done.  Generally the young generation is empty at
  // a minimum at the end of a collection.  If it is not, then
  // the heap is approaching full.
  GenCollectedHeap* gch = GenCollectedHeap::heap();
  if (full) {
    DEBUG_ONLY(seen_incremental_collection_failed = false;)
      //正常情况下GC结束后eden区是空的，如果非空说明堆内存满了，eden区中的存活对象未拷贝至老年代中
    if (!collection_attempt_is_safe() && !_eden_space->is_empty()) {
      if (Verbose && PrintGCDetails) {
        gclog_or_tty->print("DefNewEpilogue: cause(%s), full, not safe, set_failed, set_alloc_from, clear_seen",
                            GCCause::to_string(gch->gc_cause()));
      }
        //通知GCH promote事件
      gch->set_incremental_collection_failed(); // Slight lie: a full gc left us in that state
        //允许使用from区分配对象
      set_should_allocate_from_space(); // we seem to be running out of space
    } else {
      if (Verbose && PrintGCDetails) {
        gclog_or_tty->print("DefNewEpilogue: cause(%s), full, safe, clear_failed, clear_alloc_from, clear_seen",
                            GCCause::to_string(gch->gc_cause()));
      }
      gch->clear_incremental_collection_failed(); // We just did a full collection
      clear_should_allocate_from_space(); // if set
    }
  } else {
#ifdef ASSERT
    // It is possible that incremental_collection_failed() == true
    // here, because an attempted scavenge did not succeed. The policy
    // is normally expected to cause a full collection which should
    // clear that condition, so we should not be here twice in a row
    // with incremental_collection_failed() == true without having done
    // a full collection in between.
    if (!seen_incremental_collection_failed &&
        gch->incremental_collection_failed()) {
      if (Verbose && PrintGCDetails) {
        gclog_or_tty->print("DefNewEpilogue: cause(%s), not full, not_seen_failed, failed, set_seen_failed",
                            GCCause::to_string(gch->gc_cause()));
      }
      seen_incremental_collection_failed = true;
    } else if (seen_incremental_collection_failed) {
      if (Verbose && PrintGCDetails) {
        gclog_or_tty->print("DefNewEpilogue: cause(%s), not full, seen_failed, will_clear_seen_failed",
                            GCCause::to_string(gch->gc_cause()));
      }
      assert(gch->gc_cause() == GCCause::_scavenge_alot ||
             (gch->gc_cause() == GCCause::_java_lang_system_gc && UseConcMarkSweepGC && ExplicitGCInvokesConcurrent) ||
             !gch->incremental_collection_failed(),
             "Twice in a row");
      seen_incremental_collection_failed = false;
    }
#endif // ASSERT
  }

  if (ZapUnusedHeapArea) {
      //mangle三个区
    eden()->check_mangled_unused_area_complete();
    from()->check_mangled_unused_area_complete();
    to()->check_mangled_unused_area_complete();
  }

  if (!CleanChunkPoolAsync) {
      //清空ChunkPool
    Chunk::clean_chunk_pool();
  }

  // update the generation and space performance counters
    //更新计数器
  update_counters();
  gch->collector_policy()->counters()->update_counters();
}

void DefNewGeneration::record_spaces_top() {
  assert(ZapUnusedHeapArea, "Not mangling unused space");
  eden()->set_top_for_allocations();
  to()->set_top_for_allocations();
  from()->set_top_for_allocations();
}

// ref_processor_init负责初始化父类属性ref_processor
void DefNewGeneration::ref_processor_init() {
  Generation::ref_processor_init();
}


void DefNewGeneration::update_counters() {
  if (UsePerfData) {
    _eden_counters->update_all();
    _from_counters->update_all();
    _to_counters->update_all();
    _gen_counters->update_all();
  }
}

void DefNewGeneration::verify() {
  eden()->verify();
  from()->verify();
    to()->verify();
}

void DefNewGeneration::print_on(outputStream* st) const {
  Generation::print_on(st);
  st->print("  eden");
  eden()->print_on(st);
  st->print("  from");
  from()->print_on(st);
  st->print("  to  ");
  to()->print_on(st);
}


const char* DefNewGeneration::name() const {
  return "def new generation";
}

// Moved from inline file as they are not called inline
CompactibleSpace* DefNewGeneration::first_compaction_space() const {
  return eden();
}

HeapWord* DefNewGeneration::allocate(size_t word_size,
                                     bool is_tlab) {
  // This is the slow-path allocation for the DefNewGeneration.
  // Most allocations are fast-path in compiled code.
  // We try to allocate from the eden.  If that works, we are happy.
  // Note that since DefNewGeneration supports lock-free allocation, we
  // have to use it here, as well.
    //正常情况下只有慢速分配对象时才会进入此方法，此时在GenCollectHeap层已经获取了锁
    //par_allocate不要求调用方获取全局锁，底层使用cmpxchg原子指令，更快
  HeapWord* result = eden()->par_allocate(word_size);
  if (result != NULL) {
      //如果分配成功
      //CMSEdenChunksRecordAlways表示是否记录eden区分配的内存块，默认为true
    if (CMSEdenChunksRecordAlways && _next_gen != NULL) {
      _next_gen->sample_eden_chunk();
    }
    return result;
  }
  do {
    HeapWord* old_limit = eden()->soft_end();
    if (old_limit < eden()->end()) {
      // Tell the next generation we reached a limit.
        //通知老年代，年轻代已经达到了分配限制soft_end，老年代会返回一个新的限制
        //非iCMS模式下，该方法就是返回NULL，就end和soft_end一直
      HeapWord* new_limit =
        next_gen()->allocation_limit_reached(eden(), eden()->top(), word_size);
      if (new_limit != NULL) {
          //原子的修改eden区的soft_end属性
        Atomic::cmpxchg_ptr(new_limit, eden()->soft_end_addr(), old_limit);
      } else {
        assert(eden()->soft_end() == eden()->end(),
               "invalid state after allocation_limit_reached returned null");
      }
    } else {
      // The allocation failed and the soft limit is equal to the hard limit,
      // there are no reasons to do an attempt to allocate
        //soft_end跟end一致了，必须扩容才能继续分配，终止循环
      assert(old_limit == eden()->end(), "sanity check");
      break;
    }
    // Try to allocate until succeeded or the soft limit can't be adjusted
      //再次尝试分配，直到分配成功或者soft_end跟end一致
    result = eden()->par_allocate(word_size);
  } while (result == NULL);

  // If the eden is full and the last collection bailed out, we are running
  // out of heap space, and we try to allocate the from-space, too.
  // allocate_from_space can't be inlined because that would introduce a
  // circular dependency at compile time.
  if (result == NULL) {
    result = allocate_from_space(word_size);
  } else if (CMSEdenChunksRecordAlways && _next_gen != NULL) {
      //while循环重试分配成功
    _next_gen->sample_eden_chunk();
  }
  return result;
}

HeapWord* DefNewGeneration::par_allocate(size_t word_size,
                                         bool is_tlab) {
  HeapWord* res = eden()->par_allocate(word_size);
  if (CMSEdenChunksRecordAlways && _next_gen != NULL) {
    _next_gen->sample_eden_chunk();
  }
  return res;
}

//  gc_prologue方法是在GC开始前调用的预处理
void DefNewGeneration::gc_prologue(bool full) {
  // Ensure that _end and _soft_end are the same in eden space.
  eden()->set_soft_end(eden()->end());
}

size_t DefNewGeneration::tlab_capacity() const {
  return eden()->capacity();
}

size_t DefNewGeneration::tlab_used() const {
  return eden()->used();
}

size_t DefNewGeneration::unsafe_max_tlab_alloc() const {
  return unsafe_max_alloc_nogc();
}
