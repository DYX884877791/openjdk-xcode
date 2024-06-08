/*
 * Copyright (c) 2000, 2016, Oracle and/or its affiliates. All rights reserved.
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
#include "classfile/symbolTable.hpp"
#include "classfile/systemDictionary.hpp"
#include "classfile/vmSymbols.hpp"
#include "code/codeCache.hpp"
#include "code/icBuffer.hpp"
#include "gc_implementation/shared/collectorCounters.hpp"
#include "gc_implementation/shared/gcTrace.hpp"
#include "gc_implementation/shared/gcTraceTime.hpp"
#include "gc_implementation/shared/vmGCOperations.hpp"
#include "gc_interface/collectedHeap.inline.hpp"
#include "memory/filemap.hpp"
#include "memory/gcLocker.inline.hpp"
#include "memory/genCollectedHeap.hpp"
#include "memory/genOopClosures.inline.hpp"
#include "memory/generation.inline.hpp"
#include "memory/generationSpec.hpp"
#include "memory/resourceArea.hpp"
#include "memory/sharedHeap.hpp"
#include "memory/space.hpp"
#include "oops/oop.inline.hpp"
#include "oops/oop.inline2.hpp"
#include "runtime/biasedLocking.hpp"
#include "runtime/fprofiler.hpp"
#include "runtime/handles.hpp"
#include "runtime/handles.inline.hpp"
#include "runtime/java.hpp"
#include "runtime/vmThread.hpp"
#include "services/management.hpp"
#include "services/memoryService.hpp"
#include "utilities/vmError.hpp"
#include "utilities/workgroup.hpp"
#include "utilities/macros.hpp"
#include "utilities/slog.hpp"
#if INCLUDE_ALL_GCS
#include "gc_implementation/concurrentMarkSweep/concurrentMarkSweepThread.hpp"
#include "gc_implementation/concurrentMarkSweep/vmCMSOperations.hpp"
#endif // INCLUDE_ALL_GCS
#if INCLUDE_JFR
#include "jfr/jfr.hpp"
#endif // INCLUDE_JFR

GenCollectedHeap* GenCollectedHeap::_gch;
NOT_PRODUCT(size_t GenCollectedHeap::_skip_header_HeapWords = 0;)

// The set of potentially parallel tasks in root scanning.
// 表示并行执行任务的序号的枚举GCH_strong_roots_tasks
enum GCH_strong_roots_tasks {
  GCH_PS_Universe_oops_do,
  GCH_PS_JNIHandles_oops_do,
  GCH_PS_ObjectSynchronizer_oops_do,
  GCH_PS_FlatProfiler_oops_do,
  GCH_PS_Management_oops_do,
  GCH_PS_SystemDictionary_oops_do,
  GCH_PS_ClassLoaderDataGraph_oops_do,
  GCH_PS_jvmti_oops_do,
  GCH_PS_CodeCache_oops_do,
  GCH_PS_younger_gens,
  // Leave this one last.
  GCH_PS_NumElements
};

GenCollectedHeap::GenCollectedHeap(GenCollectorPolicy *policy) :
        // 调用父类的SharedHeap构造函数
  SharedHeap(policy),
        // _gen_policy 赋值为 policy
  _gen_policy(policy),
        // 创建一个子任务管理类，管理所有子任务，并赋值给_process_strong_tasks
  _process_strong_tasks(new SubTasksDone(GCH_PS_NumElements)),
        // _full_collections_completed 赋值
  _full_collections_completed(0)
{
  assert(policy != NULL, "Sanity check");
}

jint GenCollectedHeap::initialize() {
    // 这一步只是对c2编译器开通使用时，做一些参数赋值操作
  CollectedHeap::pre_initialize();

  int i;
    //_n_gens表示有多少代，通常是2代，年轻代和老年代
  _n_gens = gen_policy()->number_of_generations();

  // While there are no constraints in the GC code that HeapWordSize
  // be any particular value, there are multiple other areas in the
  // system which believe this to be true (e.g. oop->object_size in some
  // cases incorrectly returns the size in wordSize units rather than
  // HeapWordSize).
    // 保证2个值相等wordSize和HeapWordSize分别是在操作系统和Java堆中代表一个字word占用内存的大小，这两个值必然相同，否则出错
  guarantee(HeapWordSize == wordSize, "HeapWordSize must equal wordSize");

  // The heap must be at least as aligned as generations.
    // Java堆的对齐值
  size_t gen_alignment = Generation::GenGrain;

    // 获取分代对象数组，数组元素就2个，索引0元素表示年轻代，索引1元素表示老年代
  _gen_specs = gen_policy()->generations();

  // Make sure the sizes are all aligned.
    // 分别遍历新生代和老年代，并设置各自分代的空间大小（初始值和最大值），同时确保内存对齐
  for (i = 0; i < _n_gens; i++) {
      //将内存的初始值和最大值按照内存分配粒度对齐
    _gen_specs[i]->align(gen_alignment);
  }

  // Allocate space for the heap.
    // 下面才是给Java堆分配空间

  char* heap_address;
  size_t total_reserved = 0;
  int n_covered_regions = 0;
  ReservedSpace heap_rs;

    // 这是最外层Java堆的内存对齐值
  size_t heap_alignment = collector_policy()->heap_alignment();

    // 这是最外层Java堆的内存对齐值
    //根据各GenerationSpec的最大大小计算总的需要保留的内存空间，然后申请指定大小的连续内存空间
  heap_address = allocate(heap_alignment, &total_reserved,
                          &n_covered_regions, &heap_rs);

  if (!heap_rs.is_reserved()) {
      //申请失败
    vm_shutdown_during_initialization(
      "Could not reserve enough space for object heap");
    return JNI_ENOMEM;
  }

    // 将分配的Java堆内存，用 MemRegion 内存区域对象管理起来
  _reserved = MemRegion((HeapWord*)heap_rs.base(),
                        (HeapWord*)(heap_rs.base() + heap_rs.size()));

  // It is important to do this in a way such that concurrent readers can't
  // temporarily think somethings in the heap.  (Seen this happen in asserts.)
    //设置_reserved相关属性
    // 参数赋值
  _reserved.set_word_size(0);
    // Java堆内存的首地址
  _reserved.set_start((HeapWord*)heap_rs.base());
    // Java堆内存大小
  size_t actual_heap_size = heap_rs.size();
    // Java堆内存的限制地址，也就是不能超过这条线
  _reserved.set_end((HeapWord*)(heap_rs.base() + actual_heap_size));

    //初始化GenRemSet
    // 接下来就是创建记忆集、卡表的过程，卡表和记忆集都是为了解决跨代引用的实现方案
  _rem_set = collector_policy()->create_rem_set(_reserved, n_covered_regions);
  set_barrier_set(rem_set()->bs());

  _gch = this;

  for (i = 0; i < _n_gens; i++) {
      //初始化各Generation
    ReservedSpace this_rs = heap_rs.first_part(_gen_specs[i]->max_size(), false, false);
    _gens[i] = _gen_specs[i]->init(this_rs, i, rem_set());
    heap_rs = heap_rs.last_part(_gen_specs[i]->max_size());
  }
    //将_incremental_collection_failed置为false
  clear_incremental_collection_failed();

#if INCLUDE_ALL_GCS
  // If we are running CMS, create the collector responsible
  // for collecting the CMS generations.
  if (collector_policy()->is_concurrent_mark_sweep_policy()) {
      //如果是CMS则创建CMSCollector
    bool success = create_cms_collector();
    if (!success) return JNI_ENOMEM;
  }
#endif // INCLUDE_ALL_GCS

  return JNI_OK;
}


char* GenCollectedHeap::allocate(size_t alignment,
                                 size_t* _total_reserved,
                                 int* _n_covered_regions,
                                 ReservedSpace* heap_rs){
  const char overflow_msg[] = "The size of the object heap + VM data exceeds "
    "the maximum representable size";

  // Now figure out the total size.
  size_t total_reserved = 0;
  int n_covered_regions = 0;
  const size_t pageSize = UseLargePages ?
      os::large_page_size() : os::vm_page_size();

  assert(alignment % pageSize == 0, "Must be");

    //遍历所有的_gen_specs，累加各GenerationSpec的max_size和n_covered_regions
    // 遍历_gen_specs，求得新生代和老年代的分配大小
  for (int i = 0; i < _n_gens; i++) {
    total_reserved += _gen_specs[i]->max_size();
    if (total_reserved < _gen_specs[i]->max_size()) {
      vm_exit_during_initialization(overflow_msg);
    }
      // 最终为2
    n_covered_regions += _gen_specs[i]->n_covered_regions();
  }
    //校验累加后的total_reserved已经是内存对齐的
  assert(total_reserved % alignment == 0,
         err_msg("Gen size; total_reserved=" SIZE_FORMAT ", alignment="
                 SIZE_FORMAT, total_reserved, alignment));

  // Needed until the cardtable is fixed to have the right number
  // of covered regions.
    //加2是为卡表保留的
    // 再加2，就是4，也就是把堆最终分成4个区（新生代、S1、S2、老年代）
  n_covered_regions += 2;

    //赋值
  *_total_reserved = total_reserved;
  *_n_covered_regions = n_covered_regions;

    //申请指定大小的连续内存空间
    // 分配内存
  *heap_rs = Universe::reserve_heap(total_reserved, alignment);
    //返回基地址
  return heap_rs->base();
}


void GenCollectedHeap::post_initialize() {
  SharedHeap::post_initialize();
  TwoGenerationCollectorPolicy *policy =
    (TwoGenerationCollectorPolicy *)collector_policy();
  guarantee(policy->is_two_generation_policy(), "Illegal policy type");
    //校验Generation被正确的设置了
  DefNewGeneration* def_new_gen = (DefNewGeneration*) get_gen(0);
  assert(def_new_gen->kind() == Generation::DefNew ||
         def_new_gen->kind() == Generation::ParNew ||
         def_new_gen->kind() == Generation::ASParNew,
         "Wrong generation kind");

  Generation* old_gen = get_gen(1);
  assert(old_gen->kind() == Generation::ConcurrentMarkSweep ||
         old_gen->kind() == Generation::ASConcurrentMarkSweep ||
         old_gen->kind() == Generation::MarkSweepCompact,
    "Wrong generation kind");

    //初始化TwoGenerationCollectorPolicy
  policy->initialize_size_policy(def_new_gen->eden()->capacity(),
                                 old_gen->capacity(),
                                 def_new_gen->from()->capacity());
  policy->initialize_gc_policy_counters();
}

void GenCollectedHeap::ref_processing_init() {
  SharedHeap::ref_processing_init();
  for (int i = 0; i < _n_gens; i++) {
      //初始化各代的ReferenceProcessor
    _gens[i]->ref_processor_init();
  }
}

size_t GenCollectedHeap::capacity() const {
  size_t res = 0;
  for (int i = 0; i < _n_gens; i++) {
    res += _gens[i]->capacity();
  }
  return res;
}

size_t GenCollectedHeap::used() const {
  size_t res = 0;
    //遍历各Generation累加内存使用量
  for (int i = 0; i < _n_gens; i++) {
    res += _gens[i]->used();
  }
  return res;
}

// Save the "used_region" for generations level and lower.
void GenCollectedHeap::save_used_regions(int level) {
  assert(level < _n_gens, "Illegal level parameter");
  for (int i = level; i >= 0; i--) {
    _gens[i]->save_used_region();
  }
}

size_t GenCollectedHeap::max_capacity() const {
  size_t res = 0;
  for (int i = 0; i < _n_gens; i++) {
    res += _gens[i]->max_capacity();
  }
  return res;
}

// Update the _full_collections_completed counter
// at the end of a stop-world full GC.
unsigned int GenCollectedHeap::update_full_collections_completed() {
  MonitorLockerEx ml(FullGCCount_lock, Mutex::_no_safepoint_check_flag);
  assert(_full_collections_completed <= _total_full_collections,
         "Can't complete more collections than were started");
    //获取锁FullGCCount_lock，更新_full_collections_completed属性
  _full_collections_completed = _total_full_collections;
  ml.notify_all();
  return _full_collections_completed;
}

// Update the _full_collections_completed counter, as appropriate,
// at the end of a concurrent GC cycle. Note the conditional update
// below to allow this method to be called by a concurrent collector
// without synchronizing in any manner with the VM thread (which
// may already have initiated a STW full collection "concurrently").
unsigned int GenCollectedHeap::update_full_collections_completed(unsigned int count) {
  MonitorLockerEx ml(FullGCCount_lock, Mutex::_no_safepoint_check_flag);
  assert((_full_collections_completed <= _total_full_collections) &&
         (count <= _total_full_collections),
         "Can't complete more collections than were started");
  if (count > _full_collections_completed) {
    _full_collections_completed = count;
    ml.notify_all();
  }
  return _full_collections_completed;
}


#ifndef PRODUCT
// Override of memory state checking method in CollectedHeap:
// Some collectors (CMS for example) can't have badHeapWordVal written
// in the first two words of an object. (For instance , in the case of
// CMS these words hold state used to synchronize between certain
// (concurrent) GC steps and direct allocating mutators.)
// The skip_header_HeapWords() method below, allows us to skip
// over the requisite number of HeapWord's. Note that (for
// generational collectors) this means that those many words are
// skipped in each object, irrespective of the generation in which
// that object lives. The resultant loss of precision seems to be
// harmless and the pain of avoiding that imprecision appears somewhat
// higher than we are prepared to pay for such rudimentary debugging
// support.
void GenCollectedHeap::check_for_non_bad_heap_word_value(HeapWord* addr,
                                                         size_t size) {
  if (CheckMemoryInitialization && ZapUnusedHeapArea) {
    // We are asked to check a size in HeapWords,
    // but the memory is mangled in juint words.
    juint* start = (juint*) (addr + skip_header_HeapWords());
    juint* end   = (juint*) (addr + size);
    for (juint* slot = start; slot < end; slot += 1) {
      assert(*slot == badHeapWordVal,
             "Found non badHeapWordValue in pre-allocation check");
    }
  }
}
#endif

HeapWord* GenCollectedHeap::attempt_allocation(size_t size,
                                               bool is_tlab,
                                               bool first_only) {
  HeapWord* res;
  for (int i = 0; i < _n_gens; i++) {
    if (_gens[i]->should_allocate(size, is_tlab)) {
      res = _gens[i]->allocate(size, is_tlab);
      if (res != NULL) return res;
      else if (first_only) break;
    }
  }
  // Otherwise...
  return NULL;
}

HeapWord* GenCollectedHeap::mem_allocate(size_t size,
                                         bool* gc_overhead_limit_was_exceeded) {
    // 根据策略来分配
  return collector_policy()->mem_allocate_work(size,
                                               false /* is_tlab */,
                                               gc_overhead_limit_was_exceeded);
}

bool GenCollectedHeap::must_clear_all_soft_refs() {
  return _gc_cause == GCCause::_last_ditch_collection;
}

bool GenCollectedHeap::should_do_concurrent_full_gc(GCCause::Cause cause) {
    //UseConcMarkSweepGC表示使用CMS GC算法
    //GCLockerInvokesConcurrent的默认值为false
    //ExplicitGCInvokesConcurrent的默认值也是false
  return UseConcMarkSweepGC &&
         ((cause == GCCause::_gc_locker && GCLockerInvokesConcurrent) ||
          (cause == GCCause::_java_lang_system_gc && ExplicitGCInvokesConcurrent));
}

// do_collection是GenCollectedHeap执行垃圾回收的核心方法，其底层核心就是各Genaration的collect方法
void GenCollectedHeap::do_collection(bool  full,
                                     bool   clear_all_soft_refs,
                                     size_t size,
                                     bool   is_tlab,
                                     int    max_level) {
  slog_debug("进入hotspot/src/share/vm/memory/genCollectedHeap.cpp中的GenCollectedHeap::do_collection函数,该函数是GenCollectedHeap执行垃圾回收的核心方法...");
  bool prepared_for_verification = false;
  ResourceMark rm;
  DEBUG_ONLY(Thread* my_thread = Thread::current();)

    //校验处于安全点上
  assert(SafepointSynchronize::is_at_safepoint(), "should be at safepoint");
    //校验调用线程是VMThread 或者CMS Thread
  assert(my_thread->is_VM_thread() ||
         my_thread->is_ConcurrentGC_thread(),
         "incorrect thread type capability");
    //校验获取了Heap_lock锁
  assert(Heap_lock->is_locked(),
         "the requesting thread should have the Heap_lock");
  guarantee(!is_gc_active(), "collection is not reentrant");
  assert(max_level < n_gens(), "sanity check");

    //检查是否有线程处于JNI关键区，check_active_before_gc返回true表示有，则终止当前GC，等待线程退出
    //最后一个退出的线程会负责执行GC
    // 检查是否已经GC锁是否已经激活，并设置需要进行GC的标志为true，这时，通过is_active_and_needs_gc()就可以判断是否已经有线程触发了GC。
  if (GC_locker::check_active_before_gc()) {
    return; // GC is disabled (e.g. JNI GetXXXCritical operation)
  }

    //是否需要清除软引用
    // 检查是否需要回收所有的软引用。
  const bool do_clear_all_soft_refs = clear_all_soft_refs ||
                          collector_policy()->should_clear_all_soft_refs();

    //ClearedAllSoftRefs通过析构函数设置CollectorPolicy的_all_soft_refs_clear属性
  ClearedAllSoftRefs casr(do_clear_all_soft_refs, collector_policy());

    //获取元空间已使用内存
    // 记录永久代已经使用的内存空间大小。
  const size_t metadata_prev_used = MetaspaceAux::used_bytes();

    //打印GC的堆内存使用情况
  print_heap_before_gc();

  {
      //临时设置_is_gc_active为true，表示GC开始了
    FlagSetting fl(_is_gc_active, true);

      // 确定回收类型是否是FullGC以及gc触发类型(GC/Full GC(system)/Full GC，用作Log输出)。
    bool complete = full && (max_level == (n_gens()-1));
    const char* gc_cause_prefix = complete ? "Full GC" : "GC";
    TraceCPUTime tcpu(PrintGCDetails, true, gclog_or_tty);
    // The PrintGCDetails logging starts before we have incremented the GC id. We will do that later
    // so we can assume here that the next GC id is what we want.
    GCTraceTime t(GCCauseString(gc_cause_prefix, gc_cause()), PrintGCDetails, false, NULL, GCId::peek());

      //遍历各Generation执行GC准备工作
    gc_prologue(complete);
      //增加GC次数
      // gc计数加1操作(包括总GC计数和FullGC计数)。
    increment_total_collections(complete);

      //统计总的内存使用量
      // 统计堆已被使用的空间大小。
    size_t gch_prev_used = used();

      /**
       * 如果是FullGC，那么从最高的内存代到最低的内存代，若某个内存代不希望对比其更低的内存代进行单独回收，那么就以该内存代作为GC的起始内存代。
       * 这里说明下什么是单独回收。新生代比如DefNewGeneration的实现将对新生代使用复制算法进行垃圾回收，而老年代TenuredGeneration的垃圾回收则会使用其标记-压缩-清理算法对新生代也进行处理。
       * 所以可以说DefNewGeneration的垃圾回收是对新生代进行单独回收，而TenuredGeneration的垃圾回收则是对老年代和更低的内存代都进行回收。
       */
    int starting_level = 0;
    if (full) {
      // Search for the oldest generation which will collect all younger
      // generations, and start collection loop there.
        //找到老年代对应的level
      for (int i = max_level; i >= 0; i--) {
          //DefNewGeneration采用父类Generation的默认实现，返回false
        if (_gens[i]->full_collects_younger_generations()) {
          starting_level = i;
          break;
        }
      }
    }

    bool must_restore_marks_for_biased_locking = false;

      // 接下来从GC的起始内存代开始，向最老的内存代进行回收 。
    int max_level_collected = starting_level;
    for (int i = starting_level; i <= max_level; i++) {
        // should_collect()将根据该内存代GC条件返回是否应该对该内存代进行GC。若当前回收的内存代是最老的内存代，如果本次gc不是FullGC，将调用increment_total_full_collections()修正之前的FulllGC计数值。
      if (_gens[i]->should_collect(full, size, is_tlab)) {
          //如果需要垃圾回收
        if (i == n_gens() - 1) {  // a major collection is to happen
            //如果是老年代
          if (!complete) {
            // The full_collections increment was missed above.
              //increment_total_collections方法只有在complete为true时才会增加_total_full_collections计数
              //此处complete为false，但还是老年代的GC，所以增加计数
            increment_total_full_collections();
          }
            //根据参数配置dump
          pre_full_gc_dump(NULL);    // do any pre full gc dumps
        }
        // Timer for individual generations. Last argument is false: no CR
        // FIXME: We should try to start the timing earlier to cover more of the GC pause
        // The PrintGCDetails logging starts before we have incremented the GC id. We will do that later
        // so we can assume here that the next GC id is what we want.
          // 统计GC前该内存代使用空间大小以及其他记录工作 。
        GCTraceTime t1(_gens[i]->short_name(), PrintGCDetails, false, NULL, GCId::peek());
        TraceCollectorStats tcs(_gens[i]->counters());
        TraceMemoryManagerStats tmms(_gens[i]->kind(),gc_cause());

        size_t prev_used = _gens[i]->used();
          //增加计数
        _gens[i]->stat_record()->invocations++;
        _gens[i]->stat_record()->accumulated_time.start();

        // Must be done anew before each collection because
        // a previous collection will do mangling and will
        // change top of some spaces.
          //记录各Generation的Space的top指针，生产版本为空实现
        record_gen_tops_before_GC();

        if (PrintGC && Verbose) {
          gclog_or_tty->print("level=%d invoke=%d size=" SIZE_FORMAT,
                     i,
                     _gens[i]->stat_record()->invocations,
                     size*HeapWordSize);
        }

          // 验证工作 。
          // 先调用prepare_for_verify()使各内存代进行验证的准备工作(正常情况下什么都不需要做)，随后调用Universe的verify()进行GC前验证
          // 线程、堆(各内存代)、符号表、字符串表、代码缓冲、系统字典等，如对堆的验证将对堆内的每个oop对象的类型Klass进行验证，验证对象是否是oop，
          // 类型klass是否在永久代，oop的klass域是否是klass 。那么为什么在这里进行GC验证？GC前验证和GC后验证又分别有什么作用？
          // VerifyBeforeGC和VerifyAfterGC都需要和UnlockDiagnosticVMOptions配合使用以用来诊断JVM问题，但是验证过程非常耗时，所以在正常的编译版本中并没有将验证内容进行输出。
        if (VerifyBeforeGC && i >= VerifyGCLevel &&
            total_collections() >= VerifyGCStartAt) {
          HandleMark hm;  // Discard invalid handles created during verification
          if (!prepared_for_verification) {
            prepare_for_verify();
            prepared_for_verification = true;
          }
          Universe::verify(" VerifyBeforeGC:");
        }
        COMPILER2_PRESENT(DerivedPointerTable::clear());

        if (!must_restore_marks_for_biased_locking &&
             //这里的DefNewGeneration返回false，ConcurrentMarkSweepGeneration采用父类默认实现返回true
            _gens[i]->performs_in_place_marking()) {
          // We perform this mark word preservation work lazily
          // because it's only at this point that we know whether we
          // absolutely have to do it; we want to avoid doing it for
          // scavenge-only collections where it's unnecessary
          must_restore_marks_for_biased_locking = true;
            //将各线程的持有偏向锁的oop的对象头保存起来
          BiasedLocking::preserve_marks();
        }

        // Do collection work
        {
          // Note on ref discovery: For what appear to be historical reasons,
          // GCH enables and disabled (by enqueing) refs discovery.
          // In the future this should be moved into the generation's
          // collect method so that ref discovery and enqueueing concerns
          // are local to a generation. The collect method could return
          // an appropriate indication in the case that notification on
          // the ref lock was needed. This will make the treatment of
          // weak refs more uniform (and indeed remove such concerns
          // from GCH). XXX

          HandleMark hm;  // Discard invalid handles created during gc
            // 保存内存代各区域的碰撞指针到该区域的_save_mark_word变量。
          save_marks();   // save marks for all gens
          // We want to discover references, but not process them yet.
          // This mode is disabled in process_discovered_references if the
          // generation does some collection work, or in
          // enqueue_discovered_references if the generation returns
          // without doing any work.
            // 初始化引用处理器。
          ReferenceProcessor* rp = _gens[i]->ref_processor();
          // If the discovery of ("weak") refs in this generation is
          // atomic wrt other collectors in this configuration, we
          // are guaranteed to have empty discovered ref lists.
          if (rp->discovery_is_atomic()) {
            rp->enable_discovery(true /*verify_disabled*/, true /*verify_no_refs*/);
            rp->setup_policy(do_clear_all_soft_refs);
          } else {
            // collect() below will enable discovery as appropriate
              //collect方法会调用enable_discovery方法
          }
            //执行垃圾回收
            // 由各内存代完成gc
          _gens[i]->collect(full, do_clear_all_soft_refs, size, is_tlab);
            // 将不可触及的引用对象加入到Reference的pending链表
            // 其中enqueue_discovered_references根据是否使用压缩指针选择不同的enqueue_discovered_ref_helper()模板函数
          if (!rp->enqueuing_is_done()) {
              //enqueuing_is_done为false
              //将处理后剩余的References实例加入到pending-list中
            rp->enqueue_discovered_references();
          } else {
              //enqueuing_is_done为true，已经加入到pending-list中了，将其恢复成默认值
            rp->set_enqueuing_is_done(false);
          }
          rp->verify_no_references_recorded();
        }
        max_level_collected = i;

        // Determine if allocation request was met.
        if (size > 0) {
          if (!is_tlab || _gens[i]->supports_tlab_allocation()) {
            if (size*HeapWordSize <= _gens[i]->unsafe_max_alloc_nogc()) {
              size = 0;
            }
          }
        }

        COMPILER2_PRESENT(DerivedPointerTable::update_pointers());

        _gens[i]->stat_record()->accumulated_time.stop();

          //更新各Generation的GC统计信息
        update_gc_stats(i, full);

        if (VerifyAfterGC && i >= VerifyGCLevel &&
            total_collections() >= VerifyGCStartAt) {
          HandleMark hm;  // Discard invalid handles created during verification
          Universe::verify(" VerifyAfterGC:");
        }

        if (PrintGCDetails) {
          gclog_or_tty->print(":");
          _gens[i]->print_heap_change(prev_used);
        }
      }
    } //for循环结束

    // Update "complete" boolean wrt what actually transpired --
    // for instance, a promotion failure could have led to
    // a whole heap collection.
      //是否Full GC
    complete = complete || (max_level_collected == n_gens() - 1);

    if (complete) { // We did a "major" collection
      // FIXME: See comment at pre_full_gc_dump call
        //根据配置dump
      post_full_gc_dump(NULL);   // do any post full gc dumps
    }

    if (PrintGCDetails) {
      print_heap_change(gch_prev_used);

      // Print metaspace info for full GC with PrintGCDetails flag.
      if (complete) {
        MetaspaceAux::print_metaspace_change(metadata_prev_used);
      }
    }

    for (int j = max_level_collected; j >= 0; j -= 1) {
      // Adjust generation sizes.
        //调整各Generation的容量
      _gens[j]->compute_new_size();
    }

    if (complete) {
      // Delete metaspaces for unloaded class loaders and clean up loader_data graph
        //删掉被卸载的ClassLoader实例及其相关元数据
      ClassLoaderDataGraph::purge();
      MetaspaceAux::verify_metrics();
      // Resize the metaspace capacity after full collections
        //重置元空间大小
      MetaspaceGC::compute_new_size();
      update_full_collections_completed();
    }

    // Track memory usage and detect low memory after GC finishes
      //跟踪GC后的内存使用
    MemoryService::track_memory_usage();

    gc_epilogue(complete);

    if (must_restore_marks_for_biased_locking) {
      BiasedLocking::restore_marks();
    }
  }

  AdaptiveSizePolicy* sp = gen_policy()->size_policy();
  AdaptiveSizePolicyOutput(sp, total_collections());

  print_heap_after_gc();

#ifdef TRACESPINNING
  ParallelTaskTerminator::print_termination_counts();
#endif
}

HeapWord* GenCollectedHeap::satisfy_failed_allocation(size_t size, bool is_tlab) {
  return collector_policy()->satisfy_failed_allocation(size, is_tlab);
}

void GenCollectedHeap::set_par_threads(uint t) {
  SharedHeap::set_par_threads(t);
  set_n_termination(t);
}

void GenCollectedHeap::set_n_termination(uint t) {
  _process_strong_tasks->set_n_threads(t);
}

#ifdef ASSERT
class AssertNonScavengableClosure: public OopClosure {
public:
  virtual void do_oop(oop* p) {
    assert(!Universe::heap()->is_in_partial_collection(*p),
      "Referent should not be scavengable.");  }
  virtual void do_oop(narrowOop* p) { ShouldNotReachHere(); }
};
static AssertNonScavengableClosure assert_is_non_scavengable_closure;
#endif

void GenCollectedHeap::process_roots(bool activate_scope,
                                     ScanningOption so,
                                     OopClosure* strong_roots,
                                     OopClosure* weak_roots,
                                     CLDClosure* strong_cld_closure,
                                     CLDClosure* weak_cld_closure,
                                     CodeBlobToOopClosure* code_roots) {
    //修改SharedHeap的_strong_roots_parity属性
  StrongRootsScope srs(this, activate_scope);

  // General roots.
  assert(_strong_roots_parity != 0, "must have called prologue code");
  assert(code_roots != NULL, "code root closure should always be set");
  // _n_termination for _process_strong_tasks should be set up stream
  // in a method not running in a GC worker.  Otherwise the GC worker
  // could be trying to change the termination condition while the task
  // is executing in another GC worker.

    //如果这个任务还未执行
  if (!_process_strong_tasks->is_task_claimed(GCH_PS_ClassLoaderDataGraph_oops_do)) {
      //遍历所有的ClassLoaderData，如果ClassLoaderData的keep_alive为true则使用strong_cld_closure处理，否则使用weak_cld_closure
    ClassLoaderDataGraph::roots_cld_do(strong_cld_closure, weak_cld_closure);
  }

  // Some CLDs contained in the thread frames should be considered strong.
  // Don't process them if they will be processed during the ClassLoaderDataGraph phase.
  CLDClosure* roots_from_clds_p = (strong_cld_closure != weak_cld_closure) ? strong_cld_closure : NULL;
  // Only process code roots from thread stacks if we aren't visiting the entire CodeCache anyway
    //so & SO_AllCodeCache为true即so等于SO_AllCodeCache或者SO_ScavengeCodeCache
  CodeBlobToOopClosure* roots_from_code_p = (so & SO_AllCodeCache) ? NULL : code_roots;

    //遍历所有JavaThread和VMThread中包含的oop，包含通过JNI创建的oop和栈上的oop
  Threads::possibly_parallel_oops_do(strong_roots, roots_from_clds_p, roots_from_code_p);

  if (!_process_strong_tasks->is_task_claimed(GCH_PS_Universe_oops_do)) {
      //遍历Universe中包含的oop
    Universe::oops_do(strong_roots);
  }
  // Global (strong) JNI handles
  if (!_process_strong_tasks->is_task_claimed(GCH_PS_JNIHandles_oops_do)) {
      //遍历JNI全局引用中的oop
    JNIHandles::oops_do(strong_roots);
  }

  if (!_process_strong_tasks->is_task_claimed(GCH_PS_ObjectSynchronizer_oops_do)) {
      //遍历ObjectSynchronizer即synchronize关键字的实现中包含的oop，主要是锁对象
    ObjectSynchronizer::oops_do(strong_roots);
  }
  if (!_process_strong_tasks->is_task_claimed(GCH_PS_FlatProfiler_oops_do)) {
    FlatProfiler::oops_do(strong_roots);
  }
  if (!_process_strong_tasks->is_task_claimed(GCH_PS_Management_oops_do)) {
      //遍历MemoryService和ThreadService中的oop
    Management::oops_do(strong_roots);
  }
  if (!_process_strong_tasks->is_task_claimed(GCH_PS_jvmti_oops_do)) {
    JvmtiExport::oops_do(strong_roots);
  }

  if (!_process_strong_tasks->is_task_claimed(GCH_PS_SystemDictionary_oops_do)) {
      //遍历SystemDictionary系统字典中的oop
    SystemDictionary::roots_oops_do(strong_roots, weak_roots);
  }

  // All threads execute the following. A specific chunk of buckets
  // from the StringTable are the individual tasks.
    //所有线程都要执行一遍StringTable的oop遍历
  if (weak_roots != NULL) {
    if (CollectedHeap::use_parallel_gc_threads()) {
      StringTable::possibly_parallel_oops_do(weak_roots);
    } else {
      StringTable::oops_do(weak_roots);
    }
  }

  if (!_process_strong_tasks->is_task_claimed(GCH_PS_CodeCache_oops_do)) {
      //SO_AllCodeCache是包含SO_ScavengeCodeCache的，所以如果是SO_ScavengeCodeCache也会执行此逻辑
    if (so & SO_ScavengeCodeCache) {
      assert(code_roots != NULL, "must supply closure for code cache");

      // We only visit parts of the CodeCache when scavenging.
        //遍历nmethods
      CodeCache::scavenge_root_nmethods_do(code_roots);
    }
    if (so & SO_AllCodeCache) {
      assert(code_roots != NULL, "must supply closure for code cache");

      // CMSCollector uses this to do intermediate-strength collections.
      // We scan the entire code cache, since CodeCache::do_unloading is not called.
        // 遍历整个CodeBlob
      CodeCache::blobs_do(code_roots);
    }
    // Verify that the code cache contents are not subject to
    // movement by a scavenging collection.
    DEBUG_ONLY(CodeBlobToOopClosure assert_code_is_non_scavengable(&assert_is_non_scavengable_closure, !CodeBlobToOopClosure::FixRelocations));
    DEBUG_ONLY(CodeCache::asserted_non_scavengable_nmethods_do(&assert_code_is_non_scavengable));
  }

}

//  gen_process_roots用于遍历处理ClassLoaderDataGraph，Threads，Universe等组件中包含的oop，将这些oop作为根节点遍历其所引用的其他oop，
//  根据参数还能遍历年轻代和老年代中的所有oop，遍历脏的卡表项对应的内存区域中包含的oop。
void GenCollectedHeap::gen_process_roots(int level,
                                         bool younger_gens_as_roots,
                                         bool activate_scope,
                                         ScanningOption so,
                                         bool only_strong_roots,
                                         OopsInGenClosure* not_older_gens,
                                         OopsInGenClosure* older_gens,
                                         CLDClosure* cld_closure) {
  const bool is_adjust_phase = !only_strong_roots && !younger_gens_as_roots;

  bool is_moving_collection = false;
  if (level == 0 || is_adjust_phase) {
    // young collections are always moving
      //is_moving_collection表示垃圾回收过程中是否会移动对象
    is_moving_collection = true;
  }

    //is_moving_collection实际对应_fix_relocations属性，如果为true，则遍历nmethod完后，会调用
    //其fix_oop_relocations方法用于让nmethod中的oop指向对象移动后的地址
  MarkingCodeBlobClosure mark_code_closure(not_older_gens, is_moving_collection);
    //weak_roots主要用于遍历StringTable中保存的String对象和SystemDictionary中保存的ProtectionDomain对象，这些对象如果不遍历且不被其他存活对象引用则会被回收掉
  OopsInGenClosure* weak_roots = only_strong_roots ? NULL : not_older_gens;
    //weak_cld_closure用来遍历keep_live属性为false的ClassLoaderData，同上，如果不遍历则会被回收掉
  CLDClosure* weak_cld_closure = only_strong_roots ? NULL : cld_closure;

    //处理ClassLoaderDataGraph，Threads，Universe等组件中包含的oop，这些都作为根节点
  process_roots(activate_scope, so,
                not_older_gens, weak_roots,
                cld_closure, weak_cld_closure,
                &mark_code_closure);

  if (younger_gens_as_roots) {
      //younger_gens_as_roots为true，表示需要遍历年轻代中的所有对象，将其作为根节点
      //is_task_claimed返回false表示这个任务还未执行
    if (!_process_strong_tasks->is_task_claimed(GCH_PS_younger_gens)) {
        //因为是i小于level，所以对年轻代而言，即使younger_gens_as_roots为true，也不会执行
        //只有老年代，level=1时才会进入此分支
      for (int i = 0; i < level; i++) {
        not_older_gens->set_generation(_gens[i]);
        _gens[i]->oop_iterate(not_older_gens);
      }
      not_older_gens->reset_generation();
    }
  }
  // When collection is parallel, all threads get to cooperate to do
  // older-gen scanning.
    //level最低是0，即年轻代GC会调用老年代的younger_refs_iterate方法，如果level是1则不做任何处理，即老年代GC不会执行此逻辑
  for (int i = level+1; i < _n_gens; i++) {
    older_gens->set_generation(_gens[i]);
      //younger_refs_iterate支持并行遍历，注意年轻代的该方法是一个空实现
    rem_set()->younger_refs_iterate(_gens[i], older_gens);
    older_gens->reset_generation();
  }

    //标识当前线程执行完成
  _process_strong_tasks->all_tasks_completed();
}


// gen_process_weak_roots用于遍历JNI弱引用和Reference弱引用实例。
void GenCollectedHeap::gen_process_weak_roots(OopClosure* root_closure) {
    //遍历JNI弱引用
  JNIHandles::weak_oops_do(root_closure);
  JFR_ONLY(Jfr::weak_oops_do(root_closure));
  for (int i = 0; i < _n_gens; i++) {
      //遍历所有的Reference实例
    _gens[i]->ref_processor()->weak_oops_do(root_closure);
  }
}

#define GCH_SINCE_SAVE_MARKS_ITERATE_DEFN(OopClosureType, nv_suffix)    \
void GenCollectedHeap::                                                 \
oop_since_save_marks_iterate(int level,                                 \
                             OopClosureType* cur,                       \
                             OopClosureType* older) {                   \
  _gens[level]->oop_since_save_marks_iterate##nv_suffix(cur);           \
  for (int i = level+1; i < n_gens(); i++) {                            \
    _gens[i]->oop_since_save_marks_iterate##nv_suffix(older);           \
  }                                                                     \
}

ALL_SINCE_SAVE_MARKS_CLOSURES(GCH_SINCE_SAVE_MARKS_ITERATE_DEFN)

#undef GCH_SINCE_SAVE_MARKS_ITERATE_DEFN

bool GenCollectedHeap::no_allocs_since_save_marks(int level) {
    //如果level为0，则处理年轻代和老年代，如果level为1则只处理老年代
  for (int i = level; i < _n_gens; i++) {
    if (!_gens[i]->no_allocs_since_save_marks()) return false;
  }
  return true;
}

bool GenCollectedHeap::supports_inline_contig_alloc() const {
  return _gens[0]->supports_inline_contig_alloc();
}

HeapWord** GenCollectedHeap::top_addr() const {
  return _gens[0]->top_addr();
}

HeapWord** GenCollectedHeap::end_addr() const {
  return _gens[0]->end_addr();
}

// public collection interfaces

// collect方法是对JVM外部如JNI接口使用的触发垃圾回收的方法
void GenCollectedHeap::collect(GCCause::Cause cause) {
  if (should_do_concurrent_full_gc(cause)) {
#if INCLUDE_ALL_GCS
    // mostly concurrent full collection
    collect_mostly_concurrent(cause);
#else  // INCLUDE_ALL_GCS
    ShouldNotReachHere();
#endif // INCLUDE_ALL_GCS
  } else if ((cause == GCCause::_wb_young_gc) ||
             (cause == GCCause::_gc_locker)) {
    // minor collection for WhiteBox or GCLocker.
    // _gc_locker collections upgraded by GCLockerInvokesConcurrent
    // are handled above and never discarded.
    collect(cause, 0);
  } else {
#ifdef ASSERT
  if (cause == GCCause::_scavenge_alot) {
    // minor collection only
    collect(cause, 0);
  } else {
    // Stop-the-world full collection
    collect(cause, n_gens() - 1);
  }
#else
    // Stop-the-world full collection
    collect(cause, n_gens() - 1);
#endif
  }
}

void GenCollectedHeap::collect(GCCause::Cause cause, int max_level) {
  // The caller doesn't have the Heap_lock
  assert(!Heap_lock->owned_by_self(), "this thread should not own the Heap_lock");
  MutexLocker ml(Heap_lock);
  collect_locked(cause, max_level);
}

void GenCollectedHeap::collect_locked(GCCause::Cause cause) {
  // The caller has the Heap_lock
  assert(Heap_lock->owned_by_self(), "this thread should own the Heap_lock");
  collect_locked(cause, n_gens() - 1);
}

// this is the private collection interface
// The Heap_lock is expected to be held on entry.

void GenCollectedHeap::collect_locked(GCCause::Cause cause, int max_level) {
  // Read the GC count while holding the Heap_lock
  unsigned int gc_count_before      = total_collections();
  unsigned int full_gc_count_before = total_full_collections();

  if (GC_locker::should_discard(cause, gc_count_before)) {
    return;
  }

  {
      //获取Heap_lock锁
    MutexUnlocker mu(Heap_lock);  // give up heap lock, execute gets it back
      //底层还是调用do_full_collection
    VM_GenCollectFull op(gc_count_before, full_gc_count_before,
                         cause, max_level);
    VMThread::execute(&op);
  }
}

#if INCLUDE_ALL_GCS
bool GenCollectedHeap::create_cms_collector() {

  assert(((_gens[1]->kind() == Generation::ConcurrentMarkSweep) ||
         (_gens[1]->kind() == Generation::ASConcurrentMarkSweep)),
         "Unexpected generation kinds");
  // Skip two header words in the block content verification
  NOT_PRODUCT(_skip_header_HeapWords = CMSCollector::skip_header_HeapWords();)
  CMSCollector* collector = new CMSCollector(
    (ConcurrentMarkSweepGeneration*)_gens[1],
    _rem_set->as_CardTableRS(),
    (ConcurrentMarkSweepPolicy*) collector_policy());

  if (collector == NULL || !collector->completed_initialization()) {
    if (collector) {
      delete collector;  // Be nice in embedded situation
    }
    vm_shutdown_during_initialization("Could not create CMS collector");
    return false;
  }
  return true;  // success
}

void GenCollectedHeap::collect_mostly_concurrent(GCCause::Cause cause) {
  assert(!Heap_lock->owned_by_self(), "Should not own Heap_lock");

  MutexLocker ml(Heap_lock);
  // Read the GC counts while holding the Heap_lock
  unsigned int full_gc_count_before = total_full_collections();
  unsigned int gc_count_before      = total_collections();
  {
    MutexUnlocker mu(Heap_lock);
      //底层调用do_full_collection
    VM_GenCollectFullConcurrent op(gc_count_before, full_gc_count_before, cause);
    VMThread::execute(&op);
  }
}
#endif // INCLUDE_ALL_GCS

// do_full_collection有两个重载版本，用来执行年轻代或者老年代的“full gc”，即底层调用do_collection方法时，full参数是true
void GenCollectedHeap::do_full_collection(bool clear_all_soft_refs) {
   do_full_collection(clear_all_soft_refs, _n_gens - 1);
}

void GenCollectedHeap::do_full_collection(bool clear_all_soft_refs,
                                          int max_level) {

  do_collection(true                 /* full */,
                clear_all_soft_refs  /* clear_all_soft_refs */,
                0                    /* size */,
                false                /* is_tlab */,
                max_level            /* max_level */);
  // Hack XXX FIX ME !!!
  // A scavenge may not have been attempted, or may have
  // been attempted and failed, because the old gen was too full
    //如果只执行年轻代的GC，但是因为老年满了导致GC失败，则重试
  if (gc_cause() == GCCause::_gc_locker && incremental_collection_failed()) {
    if (PrintGCDetails) {
      gclog_or_tty->print_cr("GC locker: Trying a full collection "
                             "because scavenge failed");
    }
    // This time allow the old gen to be collected as well
      //会执行老年代和年轻代的GC
    do_collection(true                 /* full */,
                  clear_all_soft_refs  /* clear_all_soft_refs */,
                  0                    /* size */,
                  false                /* is_tlab */,
                  n_gens() - 1         /* max_level */);
  }
}

bool GenCollectedHeap::is_in_young(oop p) {
  bool result = ((HeapWord*)p) < _gens[_n_gens - 1]->reserved().start();
  assert(result == _gens[0]->is_in_reserved(p),
         err_msg("incorrect test - result=%d, p=" PTR_FORMAT, result, p2i((void*)p)));
  return result;
}

// Returns "TRUE" iff "p" points into the committed areas of the heap.
bool GenCollectedHeap::is_in(const void* p) const {
  #ifndef ASSERT
  guarantee(VerifyBeforeGC      ||
            VerifyDuringGC      ||
            VerifyBeforeExit    ||
            VerifyDuringStartup ||
            PrintAssembly       ||
            tty->count() != 0   ||   // already printing
            VerifyAfterGC       ||
    VMError::fatal_error_in_progress(), "too expensive");

  #endif
  // This might be sped up with a cache of the last generation that
  // answered yes.
  for (int i = 0; i < _n_gens; i++) {
    if (_gens[i]->is_in(p)) return true;
  }
  // Otherwise...
  return false;
}

#ifdef ASSERT
// Don't implement this by using is_in_young().  This method is used
// in some cases to check that is_in_young() is correct.
bool GenCollectedHeap::is_in_partial_collection(const void* p) {
  assert(is_in_reserved(p) || p == NULL,
    "Does not work if address is non-null and outside of the heap");
  return p < _gens[_n_gens - 2]->reserved().end() && p != NULL;
}
#endif

void GenCollectedHeap::oop_iterate(ExtendedOopClosure* cl) {
  for (int i = 0; i < _n_gens; i++) {
    _gens[i]->oop_iterate(cl);
  }
}

void GenCollectedHeap::object_iterate(ObjectClosure* cl) {
  for (int i = 0; i < _n_gens; i++) {
    _gens[i]->object_iterate(cl);
  }
}

void GenCollectedHeap::safe_object_iterate(ObjectClosure* cl) {
  for (int i = 0; i < _n_gens; i++) {
    _gens[i]->safe_object_iterate(cl);
  }
}

Space* GenCollectedHeap::space_containing(const void* addr) const {
  for (int i = 0; i < _n_gens; i++) {
    Space* res = _gens[i]->space_containing(addr);
    if (res != NULL) return res;
  }
  // Otherwise...
  assert(false, "Could not find containing space");
  return NULL;
}


HeapWord* GenCollectedHeap::block_start(const void* addr) const {
  assert(is_in_reserved(addr), "block_start of address outside of heap");
  for (int i = 0; i < _n_gens; i++) {
    if (_gens[i]->is_in_reserved(addr)) {
      assert(_gens[i]->is_in(addr),
             "addr should be in allocated part of generation");
      return _gens[i]->block_start(addr);
    }
  }
  assert(false, "Some generation should contain the address");
  return NULL;
}

size_t GenCollectedHeap::block_size(const HeapWord* addr) const {
  assert(is_in_reserved(addr), "block_size of address outside of heap");
  for (int i = 0; i < _n_gens; i++) {
    if (_gens[i]->is_in_reserved(addr)) {
      assert(_gens[i]->is_in(addr),
             "addr should be in allocated part of generation");
      return _gens[i]->block_size(addr);
    }
  }
  assert(false, "Some generation should contain the address");
  return 0;
}

bool GenCollectedHeap::block_is_obj(const HeapWord* addr) const {
  assert(is_in_reserved(addr), "block_is_obj of address outside of heap");
  assert(block_start(addr) == addr, "addr must be a block start");
  for (int i = 0; i < _n_gens; i++) {
    if (_gens[i]->is_in_reserved(addr)) {
      return _gens[i]->block_is_obj(addr);
    }
  }
  assert(false, "Some generation should contain the address");
  return false;
}

bool GenCollectedHeap::supports_tlab_allocation() const {
  for (int i = 0; i < _n_gens; i += 1) {
    if (_gens[i]->supports_tlab_allocation()) {
      return true;
    }
  }
  return false;
}

size_t GenCollectedHeap::tlab_capacity(Thread* thr) const {
  size_t result = 0;
  for (int i = 0; i < _n_gens; i += 1) {
    if (_gens[i]->supports_tlab_allocation()) {
      result += _gens[i]->tlab_capacity();
    }
  }
  return result;
}

size_t GenCollectedHeap::tlab_used(Thread* thr) const {
  size_t result = 0;
  for (int i = 0; i < _n_gens; i += 1) {
    if (_gens[i]->supports_tlab_allocation()) {
      result += _gens[i]->tlab_used();
    }
  }
  return result;
}

size_t GenCollectedHeap::unsafe_max_tlab_alloc(Thread* thr) const {
  size_t result = 0;
  for (int i = 0; i < _n_gens; i += 1) {
    if (_gens[i]->supports_tlab_allocation()) {
      result += _gens[i]->unsafe_max_tlab_alloc();
    }
  }
  return result;
}

HeapWord* GenCollectedHeap::allocate_new_tlab(size_t size) {
  bool gc_overhead_limit_was_exceeded;
  return collector_policy()->mem_allocate_work(size /* size */,
                                               true /* is_tlab */,
                                               &gc_overhead_limit_was_exceeded);
}

// Requires "*prev_ptr" to be non-NULL.  Deletes and a block of minimal size
// from the list headed by "*prev_ptr".
static ScratchBlock *removeSmallestScratch(ScratchBlock **prev_ptr) {
  bool first = true;
  size_t min_size = 0;   // "first" makes this conceptually infinite.
  ScratchBlock **smallest_ptr, *smallest;
  ScratchBlock  *cur = *prev_ptr;
  while (cur) {
    assert(*prev_ptr == cur, "just checking");
    if (first || cur->num_words < min_size) {
      smallest_ptr = prev_ptr;
      smallest     = cur;
      min_size     = smallest->num_words;
      first        = false;
    }
    prev_ptr = &cur->next;
    cur     =  cur->next;
  }
  smallest      = *smallest_ptr;
  *smallest_ptr = smallest->next;
  return smallest;
}

// Sort the scratch block list headed by res into decreasing size order,
// and set "res" to the result.
static void sort_scratch_list(ScratchBlock*& list) {
  ScratchBlock* sorted = NULL;
  ScratchBlock* unsorted = list;
  while (unsorted) {
    ScratchBlock *smallest = removeSmallestScratch(&unsorted);
    smallest->next  = sorted;
    sorted          = smallest;
  }
  list = sorted;
}

ScratchBlock* GenCollectedHeap::gather_scratch(Generation* requestor,
                                               size_t max_alloc_words) {
  ScratchBlock* res = NULL;
  for (int i = 0; i < _n_gens; i++) {
    _gens[i]->contribute_scratch(res, requestor, max_alloc_words);
  }
  sort_scratch_list(res);
  return res;
}

void GenCollectedHeap::release_scratch() {
  for (int i = 0; i < _n_gens; i++) {
    _gens[i]->reset_scratch();
  }
}

class GenPrepareForVerifyClosure: public GenCollectedHeap::GenClosure {
  void do_generation(Generation* gen) {
    gen->prepare_for_verify();
  }
};

void GenCollectedHeap::prepare_for_verify() {
  ensure_parsability(false);        // no need to retire TLABs
  GenPrepareForVerifyClosure blk;
  generation_iterate(&blk, false);
}


// old_to_young决定了遍历的顺序，如果为true则先遍历老年代再遍历年轻代
void GenCollectedHeap::generation_iterate(GenClosure* cl,
                                          bool old_to_young) {
  if (old_to_young) {
    for (int i = _n_gens-1; i >= 0; i--) {
      cl->do_generation(_gens[i]);
    }
  } else {
    for (int i = 0; i < _n_gens; i++) {
      cl->do_generation(_gens[i]);
    }
  }
}

void GenCollectedHeap::space_iterate(SpaceClosure* cl) {
  for (int i = 0; i < _n_gens; i++) {
    _gens[i]->space_iterate(cl, true);
  }
}

bool GenCollectedHeap::is_maximal_no_gc() const {
  for (int i = 0; i < _n_gens; i++) {
    if (!_gens[i]->is_maximal_no_gc()) {
      return false;
    }
  }
  return true;
}

void GenCollectedHeap::save_marks() {
  for (int i = 0; i < _n_gens; i++) {
    _gens[i]->save_marks();
  }
}

GenCollectedHeap* GenCollectedHeap::heap() {
  assert(_gch != NULL, "Uninitialized access to GenCollectedHeap::heap()");
  assert(_gch->kind() == CollectedHeap::GenCollectedHeap, "not a generational heap");
  return _gch;
}


void GenCollectedHeap::prepare_for_compaction() {
  guarantee(_n_gens == 2, "Wrong number of generations");
  Generation* old_gen = _gens[1];
  // Start by compacting into same gen.
  CompactPoint cp(old_gen);
  old_gen->prepare_for_compaction(&cp);
  Generation* young_gen = _gens[0];
  young_gen->prepare_for_compaction(&cp);
}

GCStats* GenCollectedHeap::gc_stats(int level) const {
  return _gens[level]->gc_stats();
}

void GenCollectedHeap::verify(bool silent, VerifyOption option /* ignored */) {
  for (int i = _n_gens-1; i >= 0; i--) {
    Generation* g = _gens[i];
    if (!silent) {
      gclog_or_tty->print("%s", g->name());
      gclog_or_tty->print(" ");
    }
    g->verify();
  }
  if (!silent) {
    gclog_or_tty->print("remset ");
  }
  rem_set()->verify();
}

void GenCollectedHeap::print_on(outputStream* st) const {
  for (int i = 0; i < _n_gens; i++) {
    _gens[i]->print_on(st);
  }
  MetaspaceAux::print_on(st);
}

void GenCollectedHeap::gc_threads_do(ThreadClosure* tc) const {
  if (workers() != NULL) {
    workers()->threads_do(tc);
  }
#if INCLUDE_ALL_GCS
  if (UseConcMarkSweepGC) {
    ConcurrentMarkSweepThread::threads_do(tc);
  }
#endif // INCLUDE_ALL_GCS
}

void GenCollectedHeap::print_gc_threads_on(outputStream* st) const {
#if INCLUDE_ALL_GCS
  if (UseParNewGC) {
    workers()->print_worker_threads_on(st);
  }
  if (UseConcMarkSweepGC) {
    ConcurrentMarkSweepThread::print_all_on(st);
  }
#endif // INCLUDE_ALL_GCS
}

void GenCollectedHeap::print_on_error(outputStream* st) const {
  this->CollectedHeap::print_on_error(st);

#if INCLUDE_ALL_GCS
  if (UseConcMarkSweepGC) {
    st->cr();
    CMSCollector::print_on_error(st);
  }
#endif // INCLUDE_ALL_GCS
}

void GenCollectedHeap::print_tracing_info() const {
  if (TraceGen0Time) {
    get_gen(0)->print_summary_info();
  }
  if (TraceGen1Time) {
    get_gen(1)->print_summary_info();
  }
}

void GenCollectedHeap::print_heap_change(size_t prev_used) const {
  if (PrintGCDetails && Verbose) {
    gclog_or_tty->print(" "  SIZE_FORMAT
                        "->" SIZE_FORMAT
                        "("  SIZE_FORMAT ")",
                        prev_used, used(), capacity());
  } else {
    gclog_or_tty->print(" "  SIZE_FORMAT "K"
                        "->" SIZE_FORMAT "K"
                        "("  SIZE_FORMAT "K)",
                        prev_used / K, used() / K, capacity() / K);
  }
}

class GenGCPrologueClosure: public GenCollectedHeap::GenClosure {
 private:
  bool _full;
 public:
  void do_generation(Generation* gen) {
    gen->gc_prologue(_full);
  }
  GenGCPrologueClosure(bool full) : _full(full) {};
};

void GenCollectedHeap::gc_prologue(bool full) {
  assert(InlineCacheBuffer::is_empty(), "should have cleaned up ICBuffer");

  always_do_update_barrier = false;
  // Fill TLAB's and such
    //收集GC前的TLAB的统计数据
  CollectedHeap::accumulate_statistics_all_tlabs();
    //遍历各Generation执行ensure_parsability
  ensure_parsability(true);   // retire TLABs

  // Walk generations
    //遍历各Generation执行gc_prologue
  GenGCPrologueClosure blk(full);
  generation_iterate(&blk, false);  // not old-to-young.
};

class GenGCEpilogueClosure: public GenCollectedHeap::GenClosure {
 private:
  bool _full;
 public:
  void do_generation(Generation* gen) {
    gen->gc_epilogue(_full);
  }
  GenGCEpilogueClosure(bool full) : _full(full) {};
};

void GenCollectedHeap::gc_epilogue(bool full) {
#ifdef COMPILER2
  assert(DerivedPointerTable::is_empty(), "derived pointer present");
  size_t actual_gap = pointer_delta((HeapWord*) (max_uintx-3), *(end_addr()));
  guarantee(actual_gap > (size_t)FastAllocateSizeLimit, "inline allocation wraps");
#endif /* COMPILER2 */

    //重新计算各线程的TLAB的分配大小
  resize_all_tlabs();

    //遍历各Generation执行gc_epilogue方法
  GenGCEpilogueClosure blk(full);
  generation_iterate(&blk, false);  // not old-to-young.

    //CleanChunkPoolAsync默认为false
  if (!CleanChunkPoolAsync) {
      //清理ChunkPool
    Chunk::clean_chunk_pool();
  }

    //更新计数器
  MetaspaceCounters::update_performance_counters();
  CompressedClassSpaceCounters::update_performance_counters();

  always_do_update_barrier = UseConcMarkSweepGC;
};

#ifndef PRODUCT
class GenGCSaveTopsBeforeGCClosure: public GenCollectedHeap::GenClosure {
 private:
 public:
  void do_generation(Generation* gen) {
    gen->record_spaces_top();
  }
};

void GenCollectedHeap::record_gen_tops_before_GC() {
  if (ZapUnusedHeapArea) {
    GenGCSaveTopsBeforeGCClosure blk;
    generation_iterate(&blk, false);  // not old-to-young.
  }
}
#endif  // not PRODUCT

class GenEnsureParsabilityClosure: public GenCollectedHeap::GenClosure {
 public:
  void do_generation(Generation* gen) {
    gen->ensure_parsability();
  }
};

void GenCollectedHeap::ensure_parsability(bool retire_tlabs) {
  CollectedHeap::ensure_parsability(retire_tlabs);
  GenEnsureParsabilityClosure ep_cl;
  generation_iterate(&ep_cl, false);
}

oop GenCollectedHeap::handle_failed_promotion(Generation* old_gen,
                                              oop obj,
                                              size_t obj_size) {
  guarantee(old_gen->level() == 1, "We only get here with an old generation");
  assert(obj_size == (size_t)obj->size(), "bad obj_size passed in");
  HeapWord* result = NULL;

  result = old_gen->expand_and_allocate(obj_size, false);

  if (result != NULL) {
    Copy::aligned_disjoint_words((HeapWord*)obj, result, obj_size);
  }
  return oop(result);
}

class GenTimeOfLastGCClosure: public GenCollectedHeap::GenClosure {
  jlong _time;   // in ms
  jlong _now;    // in ms

 public:
  GenTimeOfLastGCClosure(jlong now) : _time(now), _now(now) { }

  jlong time() { return _time; }

  void do_generation(Generation* gen) {
    _time = MIN2(_time, gen->time_of_last_gc(_now));
  }
};

jlong GenCollectedHeap::millis_since_last_gc() {
  // We need a monotonically non-deccreasing time in ms but
  // os::javaTimeMillis() does not guarantee monotonicity.
  jlong now = os::javaTimeNanos() / NANOSECS_PER_MILLISEC;
  GenTimeOfLastGCClosure tolgc_cl(now);
  // iterate over generations getting the oldest
  // time that a generation was collected
  generation_iterate(&tolgc_cl, false);

  // javaTimeNanos() is guaranteed to be monotonically non-decreasing
  // provided the underlying platform provides such a time source
  // (and it is bug free). So we still have to guard against getting
  // back a time later than 'now'.
  jlong retVal = now - tolgc_cl.time();
  if (retVal < 0) {
    NOT_PRODUCT(warning("time warp: "INT64_FORMAT, (int64_t) retVal);)
    return 0;
  }
  return retVal;
}
