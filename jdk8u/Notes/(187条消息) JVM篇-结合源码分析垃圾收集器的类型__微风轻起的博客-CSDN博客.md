---
source: https://blog.csdn.net/qq_25179481/article/details/114782537?ops_request_misc=%257B%2522request%255Fid%2522%253A%2522168994945516800188549628%2522%252C%2522scm%2522%253A%252220140713.130102334.pc%255Fall.%2522%257D&request_id=168994945516800188549628&biz_id=0&utm_medium=distribute.pc_search_result.none-task-blog-2~all~first_rank_ecpm_v1~rank_v31_ecpm-8-114782537-null-null.142^v90^chatsearch,239^v3^control&utm_term=GCTaskManager&spm=1018.2226.3001.4187
---
这一篇我们来看下JVM中的垃圾收集器，看下这些垃圾收集器是怎样选择以及初始化的一些信息。**建议在看本篇文章的时候看下前面两篇**

## 一、基本介绍

## 1、方法调用链

![在这里插入图片描述](https://img-blog.csdnimg.cn/20210314121814334.png?x-oss-process=image/watermark,type_ZmFuZ3poZW5naGVpdGk,shadow_10,text_aHR0cHM6Ly9ibG9nLmNzZG4ubmV0L3FxXzI1MTc5NDgx,size_16,color_FFFFFF,t_70#pic_center)

关于垃圾收集器选择初始化就是在`initialized_heap()`方法的`create_heap()`方法:

```
CollectedHeap*  Universe::_collectedHeap = NULL;
```

```
jint Universe::initialize_heap() {
  jint status = JNI_ERR;

  _collectedHeap = create_heap_ext();
  if (_collectedHeap == NULL) {
    _collectedHeap = create_heap();
  }
  status = _collectedHeap->initialize();
  if (status != JNI_OK) {
    return status;
  }
.......
}
```

## 2、垃圾的种类

```
CollectedHeap* Universe::create_heap() {
  assert(_collectedHeap == NULL, "Heap already created");
..........
  if (UseParallelGC) {
    return Universe::create_heap_with_policy<ParallelScavengeHeap, GenerationSizer>();
  } else if (UseG1GC) {
    return Universe::create_heap_with_policy<G1CollectedHeap, G1CollectorPolicy>();
  } else if (UseConcMarkSweepGC) {
    return Universe::create_heap_with_policy<GenCollectedHeap, ConcurrentMarkSweepPolicy>();
#endif
  } else if (UseSerialGC) {
    return Universe::create_heap_with_policy<GenCollectedHeap, MarkSweepPolicy>();
  }

  ShouldNotReachHere();
  return NULL;
}
```

 可以看到这里就是根据选择的垃圾收集器然后决定对应的`Heap`&`Policy`。这里一共有四种：`UseParallelGC`,`UseG1GC`,`UseConcMarkSweepGC`,`UseSerialGC`：

![在这里插入图片描述](https://img-blog.csdnimg.cn/20210314121828581.png?x-oss-process=image/watermark,type_ZmFuZ3poZW5naGVpdGk,shadow_10,text_aHR0cHM6Ly9ibG9nLmNzZG4ubmV0L3FxXzI1MTc5NDgx,size_16,color_FFFFFF,t_70#pic_center)

及对应源码中的四种。

## 3、总括

```
template <class Heap, class Policy>
CollectedHeap* Universe::create_heap_with_policy() {
  Policy* policy = new Policy();
  policy->initialize_all();
  return new Heap(policy);
}
```

 这个就是创建`CollectedHeap`的过程，上面的例如`GenCollectedHeap`、`MarkSweepPolicy`都是`Heap`、`Policy`的子类。

```
virtual void initialize_all() {
  CollectorPolicy::initialize_all();
  initialize_generations();
}
```

 然后这里不同的`Policy`对`initialize_generations()`有不同的实现：

1）、`MarkSweepPolicy`：

```
void MarkSweepPolicy::initialize_generations() {
  _young_gen_spec = new GenerationSpec(Generation::DefNew, _initial_young_size, _max_young_size, _gen_alignment);
  _old_gen_spec   = new GenerationSpec(Generation::MarkSweepCompact, _initial_old_size, _max_old_size, _gen_alignment);
}
```

2）、ConcurrentMarkSweepPolicy

```
void ConcurrentMarkSweepPolicy::initialize_generations() {
  _young_gen_spec = new GenerationSpec(Generation::ParNew, _initial_young_size,
                                       _max_young_size, _gen_alignment);
  _old_gen_spec   = new GenerationSpec(Generation::ConcurrentMarkSweep,
                                       _initial_old_size, _max_old_size, _gen_alignment);
}
```

3）、继承关系

 不过`MarkSweepPolicy`、`ConcurrentMarkSweepPolicy`都是继承的`GenCollectorPolicy`：

```
class MarkSweepPolicy : public GenCollectorPolicy {
```

```
class ConcurrentMarkSweepPolicy : public GenCollectorPolicy {
```

 然后`GenCollectorPolicy`是继承的`CollectorPolicy`

```
class GenCollectorPolicy : public CollectorPolicy {
```

 其他的两种`Policy`：`G1CollectorPolicy`、`GenerationSizer`：

```
class GenerationSizer : public GenCollectorPolicy {
```

```
class G1CollectorPolicy: public CollectorPolicy {
protected:
  void initialize_alignments();

public:
  G1CollectorPolicy();
};
```

## 二、serial垃圾收集器(UseSerialGC)空间管理

```
Universe::create_heap_with_policy<GenCollectedHeap, MarkSweepPolicy>();
```

## 1、GenCollectedHeap

```
/ A "GenCollectedHeap" is a CollectedHeap that uses generational
// collection.  It has two generations, young and old.
class GenCollectedHeap : public CollectedHeap {
  friend class GenCollectorPolicy;
  friend class Generation;
  friend class DefNewGeneration;
  friend class TenuredGeneration;
  friend class ConcurrentMarkSweepGeneration;
  friend class CMSCollector;
  friend class GenMarkSweep;
  friend class VM_GenCollectForAllocation;
  friend class VM_GenCollectFull;
  friend class VM_GenCollectFullConcurrent;
  ..............

  enum GenerationType {
    YoungGen,
    OldGen
  };

private:
  Generation* _young_gen;
  Generation* _old_gen;
......
  // The generational collector policy.
  GenCollectorPolicy* _gen_policy;
```

这个是这个`Heap`的基本属性，下面我们来看下其创建后调用的`_collectedHeap->initialize()`方法。

### 1）、initialize()

```
jint GenCollectedHeap::initialize() {
......
  char* heap_address;
  ReservedSpace heap_rs;

  size_t heap_alignment = collector_policy()->heap_alignment();

  heap_address = allocate(heap_alignment, &heap_rs);
........
  _rem_set = collector_policy()->create_rem_set(reserved_region());
  set_barrier_set(rem_set()->bs());

  ReservedSpace young_rs = heap_rs.first_part(gen_policy()->young_gen_spec()->max_size(), false, false);
  _young_gen = gen_policy()->young_gen_spec()->init(young_rs, rem_set());
  heap_rs = heap_rs.last_part(gen_policy()->young_gen_spec()->max_size());

  ReservedSpace old_rs = heap_rs.first_part(gen_policy()->old_gen_spec()->max_size(), false, false);
  _old_gen = gen_policy()->old_gen_spec()->init(old_rs, rem_set());
  clear_incremental_collection_failed();
........
  return JNI_OK;
}
```

 这个方法其实在前面的文章也提到过，就是年轻代&老年代的初始化分配。然后这里主要是`gen_policy()->young_gen_spec()->init(young_rs, rem_set())`，也就是`young_gen_spec()->init(young_rs, rem_set())`，即`GenerationSpec`的`init`方法。

```
GenerationSpec* young_gen_spec() const {
  assert(_young_gen_spec != NULL, "_young_gen_spec should have been initialized");
  return _young_gen_spec;
}
```

```
public:
 // The set of possible generation kinds.
 enum Name {
   DefNew,
   ParNew,
   MarkSweepCompact,
   ConcurrentMarkSweep,
   Other
 };
```

```
Generation* GenerationSpec::init(ReservedSpace rs, CardTableRS* remset) {
  switch (name()) {
    case Generation::DefNew:
      return new DefNewGeneration(rs, init_size());
    case Generation::MarkSweepCompact:
      return new TenuredGeneration(rs, init_size(), remset);
#if INCLUDE_ALL_GCS
    case Generation::ParNew:
      return new ParNewGeneration(rs, init_size());
    case Generation::ConcurrentMarkSweep: {
   .....
      ConcurrentMarkSweepGeneration* g = NULL;
      g = new ConcurrentMarkSweepGeneration(rs, init_size(), remset);
......
    }
#endif // INCLUDE_ALL_GCS
    default:
      guarantee(false, "unrecognized GenerationName");
      return NULL;
  }
}
```

 我们当前是`serial`，所以这里的`name()`就会是`Generation::DefNew`、`Generation::MarkSweepCompact`，即年轻代使用`Generation::DefNew`、老年代使用`Generation::MarkSweepCompact`。但我们将这里的四种都看下，**也就是我们使用`UseSerialGC`&`UseConcMarkSweepGC`垃圾收集器的时候**，**会将年轻代初始为`DefNewGeneration`或`ParNewGeneration`，老年代初始化为`MarkSweepCompact`或`ConcurrentMarkSweepGeneration`。这些`Generation`的实现都是用来空间管理的**

## 2、DefNewGeneration(Generation::DefNew)

```
// DefNewGeneration is a young generation containing eden, from- and
// to-space.

class DefNewGeneration: public Generation {
  friend class VMStructs;

protected:
  Generation* _old_gen;
  uint        _tenuring_threshold;   // Tenuring threshold for next collection.
  AgeTable    _age_table;
  // Size of object to pretenure in words; command line provides bytes
  size_t      _pretenure_size_threshold_words;

  AgeTable*   age_table() { return &_age_table; }
  ......
      // Spaces
  ContiguousSpace* _eden_space;
  ContiguousSpace* _from_space;
  ContiguousSpace* _to_space;
    ......
        // Printing
  virtual const char* name() const;
  virtual const char* short_name() const { return "DefNew"; }
    ......
```

 这个是年轻代`Generation`，可以看到其有包含年龄表`AgeTable`，能找到老年代的指针`_old_gen`，对象晋升到老年代的阈值`_tenuring_threshold`。

## 3、TenuredGeneration

```
// TenuredGeneration models the heap containing old (promoted/tenured) objects
// contained in a single contiguous space.
//
// Garbage collection is performed using mark-compact.

class TenuredGeneration: public CardGeneration {
  friend class VMStructs;
  // Abstractly, this is a subtype that gets access to protected fields.
  friend class VM_PopulateDumpSharedSpace;

 protected:
  ContiguousSpace*    _the_space;       // Actual space holding objects

  GenerationCounters* _gen_counters;
  CSpaceCounters*     _space_counters;
```

 这个就是用来解析老年代的空间管理，通过注释可以知道其主要有两个信息：其是用来管理老年代的，同时其是一块连续的空间。

## 4、ConcurrentMarkSweepGeneration(Generation::ConcurrentMarkSweep)

```
class ConcurrentMarkSweepGeneration: public CardGeneration {
  friend class VMStructs;
  friend class ConcurrentMarkSweepThread;
  friend class ConcurrentMarkSweep;
  friend class CMSCollector;
 protected:
  static CMSCollector*       _collector; // the collector that collects us
  CompactibleFreeListSpace*  _cmsSpace;  // underlying space (only one for now)

  // Performance Counters
  GenerationCounters*      _gen_counters;
  GSpaceCounters*          _space_counters;
```

## 三、CMS垃圾收集器(UseConcMarkSweepGC)空间管理

## 1、GenCollectedHeap

 CMS与前面的serial用的是一样的GenCollectedHeap。

## 2、ParNewGeneration

```
// A Generation that does parallel young-gen collection.

class ParNewGeneration: public DefNewGeneration {
  friend class ParNewGenTask;
  friend class ParNewRefProcTask;
  friend class ParNewRefProcTaskExecutor;
  friend class ParScanThreadStateSet;
  friend class ParEvacuateFollowersClosure;

 private:
  // The per-worker-thread work queues
  ObjToScanQueueSet* _task_queues;

  // Per-worker-thread local overflow stacks
  Stack<oop, mtGC>* _overflow_stacks;
......
  // GC tracer that should be used during collection.
  ParNewTracer _gc_tracer;
    ......
  virtual const char* name() const;
  virtual const char* short_name() const { return "ParNew"; }
```

 其是多线程的用来处理年轻代的垃圾收集，同时其继承`DefNewGeneration`，也就是说`ParNewGeneration`是使用多线程去处理原来的`DefNewGeneration`单线程处理的内容。

## 3、ConcurrentMarkSweepPolicy

```
class ConcurrentMarkSweepPolicy : public GenCollectorPolicy {
 protected:
  void initialize_alignments();
  void initialize_generations();

 public:
  ConcurrentMarkSweepPolicy() {}

  ConcurrentMarkSweepPolicy* as_concurrent_mark_sweep_policy() { return this; }

  void initialize_gc_policy_counters();

  virtual void initialize_size_policy(size_t init_eden_size,
                                      size_t init_promo_size,
                                      size_t init_survivor_size);
};
```

同时`ConcurrentMarkSweepPolicy`也是直接继承原来的`GenCollectorPolicy`。

## 四、serial&CMS的垃圾收集

## 1、基本对象

```
class VM_Operation: public CHeapObj<mtInternal> {
 public:
  enum Mode {
    _safepoint,       // blocking,        safepoint, vm_op C-heap allocated
    _no_safepoint,    // blocking,     no safepoint, vm_op C-Heap allocated
    _concurrent,      // non-blocking, no safepoint, vm_op C-Heap allocated
    _async_safepoint  // non-blocking,    safepoint, vm_op C-Heap allocated
  };

  enum VMOp_Type {
    VM_OPS_DO(VM_OP_ENUM)
    VMOp_Terminating
  };
....
  // Called by VM thread - does in turn invoke doit(). Do not override this
  void evaluate();

  // evaluate() is called by the VMThread and in turn calls doit().
  // If the thread invoking VMThread::execute((VM_Operation*) is a JavaThread,
  // doit_prologue() is called in that thread before transferring control to
  // the VMThread.
  // If doit_prologue() returns true the VM operation will proceed, and
  // doit_epilogue() will be called by the JavaThread once the VM operation
  // completes. If doit_prologue() returns false the VM operation is cancelled.
  virtual void doit()                            = 0;
  virtual bool doit_prologue()                   { return true; };
  virtual void doit_epilogue()                   {}; // Note: Not called if mode is: _concurrent
```

```
class VM_GC_Operation: public VM_Operation {
 private:
  ReferencePendingListLocker _pending_list_locker;

 protected:
  uint           _gc_count_before;         // gc count before acquiring PLL
  uint           _full_gc_count_before;    // full gc count before acquiring PLL
  bool           _full;                    // whether a "full" collection
  bool           _prologue_succeeded;      // whether doit_prologue succeeded
  GCCause::Cause _gc_cause;                // the putative cause for this gc op
  bool           _gc_locked;               // will be set if gc was locked
    ......
```

```
class VM_CollectForAllocation : public VM_GC_Operation {
 protected:
  size_t    _word_size; // Size of object to be allocated (in number of words)
  HeapWord* _result;    // Allocation result (NULL if allocation failed)

 public:
  VM_CollectForAllocation(size_t word_size, uint gc_count_before, GCCause::Cause cause);

  HeapWord* result() const {
    return _result;
  }
};
```

以上三个类是父子关系，在`VM_Operation`中是有定义`Mode`4种状态。在垃圾收集的时候是另外的线程，线程的任务对象一般就是`VM_CollectForAllocation`的子类，然后过程就是通过`evaluate()`方法来调用`doit*()`方法来具体处理。

![在这里插入图片描述](https://img-blog.csdnimg.cn/20210314121856415.png#pic_center)

 然后不同的垃圾收集器即`VM_CollectForAllocation`的子类主要是对这两个方法的实现：

![在这里插入图片描述](https://img-blog.csdnimg.cn/20210314121908641.png?x-oss-process=image/watermark,type_ZmFuZ3poZW5naGVpdGk,shadow_10,text_aHR0cHM6Ly9ibG9nLmNzZG4ubmV0L3FxXzI1MTc5NDgx,size_16,color_FFFFFF,t_70#pic_center)

## 2、垃圾收集触发逻辑

![在这里插入图片描述](https://img-blog.csdnimg.cn/20210314121921360.png#pic_center)

```
HeapWord* GenCollectorPolicy::mem_allocate_work(size_t size,
                                        bool is_tlab,
                                        bool* gc_overhead_limit_was_exceeded) {
  GenCollectedHeap *gch = GenCollectedHeap::heap();
.....
  HeapWord* result = NULL;

  // Loop until the allocation is satisfied, or unsatisfied after GC.
  for (uint try_count = 1, gclocker_stalled_count = 0; /* return or throw */; try_count += 1) {
    HandleMark hm; // Discard any handles allocated in each iteration.

    // First allocation attempt is lock-free.
    Generation *young = gch->young_gen();
    assert(young->supports_inline_contig_alloc(),
      "Otherwise, must do alloc within heap lock");
    if (young->should_allocate(size, is_tlab)) {
      result = young->par_allocate(size, is_tlab);
      if (result != NULL) {
        assert(gch->is_in_reserved(result), "result not in heap");
        return result;
      }
    }
    uint gc_count_before;  // Read inside the Heap_lock locked region.
    {
      MutexLocker ml(Heap_lock);
      log_trace(gc, alloc)("GenCollectorPolicy::mem_allocate_work: attempting locked slow path allocation");
      // Note that only large objects get a shot at being
      // allocated in later generations.
      bool first_only = ! should_try_older_generation_allocation(size);

      result = gch->attempt_allocation(size, is_tlab, first_only);
      if (result != NULL) {
        assert(gch->is_in_reserved(result), "result not in heap");
        return result;
      }
..........
      // Read the gc count while the heap lock is held.
      gc_count_before = gch->total_collections();
    }

    VM_GenCollectForAllocation op(size, is_tlab, gc_count_before);
    VMThread::execute(&op);
    ..........
}
```

 我们以`VM_GenCollectForAllocation`为例，在进行内存申请的时候`gch->attempt_allocatio`我们看到如果是申请成功了，`result != NULL`，就直接返回了，如果失败了，就会进行垃圾收集的线程`VMThread::execute(&op)`：

![在这里插入图片描述](https://img-blog.csdnimg.cn/20210314121933986.png?x-oss-process=image/watermark,type_ZmFuZ3poZW5naGVpdGk,shadow_10,text_aHR0cHM6Ly9ibG9nLmNzZG4ubmV0L3FxXzI1MTc5NDgx,size_16,color_FFFFFF,t_70#pic_center)

 然后在垃圾收集的线程中，就通过`evaluate()`方法调用`doit()`。下面我们就来具体分析下`VM_GenCollectForAllocation`，因为其是用来处理`GenCollectedHeap`的，同时`CMS`&serial都用的`GenCollectedHeap`。

## 3、VM_GenCollectForAllocation

```
class VM_GenCollectForAllocation : public VM_CollectForAllocation {
 private:
  bool        _tlab;                       // alloc is of a tlab.
```

```
void VM_GenCollectForAllocation::doit() {
  SvcGCMarker sgcm(SvcGCMarker::MINOR);

  GenCollectedHeap* gch = GenCollectedHeap::heap();
  GCCauseSetter gccs(gch, _gc_cause);
  _result = gch->satisfy_failed_allocation(_word_size, _tlab);
  assert(gch->is_in_reserved_or_null(_result), "result not in heap");

  if (_result == NULL && GCLocker::is_active_and_needs_gc()) {
    set_gc_locked();
  }
}
```

 这里实现是获取`GenCollectedHeap`，然后通过`satisfy_failed_allocation`处理：

```
HeapWord* GenCollectedHeap::satisfy_failed_allocation(size_t size, bool is_tlab) {
  return gen_policy()->satisfy_failed_allocation(size, is_tlab);
}
```

 可以看到这里是先获取`policy`，而`policy` 两个垃圾收集器是不同的，`CMS`是`ConcurrentMarkSweepPolicy`、`serial`是`MarkSweepPolicy`。

```
HeapWord* GenCollectorPolicy::satisfy_failed_allocation(size_t size,
                                                        bool   is_tlab) {
  GenCollectedHeap *gch = GenCollectedHeap::heap();
  GCCauseSetter x(gch, GCCause::_allocation_failure);
  HeapWord* result = NULL;
....
    log_trace(gc)(" :: Trying full because partial may fail :: ");
    // Try a full collection; see delta for bug id 6266275
    // for the original code and why this has been simplified
    // with from-space allocation criteria modified and
    // such allocation moved out of the safepoint path.
    gch->do_collection(true,                      // full
                       false,                     // clear_all_soft_refs
                       size,                      // size
                       is_tlab,                   // is_tlab
                       GenCollectedHeap::OldGen); // max_generation
  }

  result = gch->attempt_allocation(size, is_tlab, false /*first_only*/);
......
}
```

```
void GenCollectedHeap::do_collection(bool           full,
                                     bool           clear_all_soft_refs,
                                     size_t         size,
                                     bool           is_tlab,
                                     GenerationType max_generation) {
 ....
  {
    FlagSetting fl(_is_gc_active, true);
    bool complete = full && (max_generation == OldGen);
    bool old_collects_young = complete && !ScavengeBeforeFullGC;
    bool do_young_collection = !old_collects_young && _young_gen->should_collect(full, size, is_tlab);
    FormatBuffer<> gc_string("%s", "Pause ");
    if (do_young_collection) {
      gc_string.append("Young");
    } else {
      gc_string.append("Full");
    }
.....
    bool collected_old = false;
....
      if (do_young_collection) {
      ........
        collect_generation(_old_gen, full, size, is_tlab, run_verification && VerifyGCLevel <= 1, do_clear_all_soft_refs, true);
      } else {
        // No young GC done. Use the same GC id as was set up earlier in this method.
        collect_generation(_old_gen, full, size, is_tlab, run_verification && VerifyGCLevel <= 1, do_clear_all_soft_refs, true);
      }
      must_restore_marks_for_biased_locking = true;
      collected_old = true;
    }
......
}
```

```
void GenCollectedHeap::collect_generation(Generation* gen, bool full, size_t size,
                                          bool is_tlab, bool run_verification, bool clear_soft_refs,
                                          bool restore_marks_for_biased_locking) {
  .........
    gen->collect(full, clear_soft_refs, size, is_tlab);
   ........
}
```

 这里两种不同的垃圾处理器的划分关键就是这个，因为其使用的`Generation`是不同的，目前这个是老年代，所以`serial`收集器是用的`TenuredGeneration`、`CMS`用的是`ConcurrentMarkSweepGeneration`。下面我们就来看下这两个的`collect()`方法实现：

### 1）、TenuredGeneration

```
void TenuredGeneration::collect(bool   full,
                                bool   clear_all_soft_refs,
                                size_t size,
                                bool   is_tlab) {
  GenCollectedHeap* gch = GenCollectedHeap::heap();

  // Temporarily expand the span of our ref processor, so
  // refs discovery is over the entire heap, not just this generation
  ReferenceProcessorSpanMutator
    x(ref_processor(), gch->reserved_region());

  STWGCTimer* gc_timer = GenMarkSweep::gc_timer();
  gc_timer->register_gc_start();

  SerialOldTracer* gc_tracer = GenMarkSweep::gc_tracer();
  gc_tracer->report_gc_start(gch->gc_cause(), gc_timer->gc_start());

  gch->pre_full_gc_dump(gc_timer);

  GenMarkSweep::invoke_at_safepoint(ref_processor(), clear_all_soft_refs);

  gch->post_full_gc_dump(gc_timer);

  gc_timer->register_gc_end();

  gc_tracer->report_gc_end(gc_timer->gc_end(), gc_timer->time_partitions());
}
```

```
void GenMarkSweep::invoke_at_safepoint(ReferenceProcessor* rp, bool clear_all_softrefs) {
  assert(SafepointSynchronize::is_at_safepoint(), "must be at a safepoint");

  GenCollectedHeap* gch = GenCollectedHeap::heap();
#ifdef ASSERT
  if (gch->collector_policy()->should_clear_all_soft_refs()) {
    assert(clear_all_softrefs, "Policy should have been checked earlier");
  }
#endif

  // hook up weak ref data so it can be used during Mark-Sweep
  assert(ref_processor() == NULL, "no stomping");
  assert(rp != NULL, "should be non-NULL");
  set_ref_processor(rp);
  rp->setup_policy(clear_all_softrefs);

  gch->trace_heap_before_gc(_gc_tracer);

  // When collecting the permanent generation Method*s may be moving,
  // so we either have to flush all bcp data or convert it into bci.
  CodeCache::gc_prologue();

  // Increment the invocation count
  _total_invocations++;

  // Capture used regions for each generation that will be
  // subject to collection, so that card table adjustments can
  // be made intelligently (see clear / invalidate further below).
  gch->save_used_regions();

  allocate_stacks();

  mark_sweep_phase1(clear_all_softrefs);

  mark_sweep_phase2();

  // Don't add any more derived pointers during phase3
#if defined(COMPILER2) || INCLUDE_JVMCI
  assert(DerivedPointerTable::is_active(), "Sanity");
  DerivedPointerTable::set_active(false);
#endif

  mark_sweep_phase3();

  mark_sweep_phase4();

  restore_marks();

  // Set saved marks for allocation profiler (and other things? -- dld)
  // (Should this be in general part?)
  gch->save_marks();

  deallocate_stacks();

  // If compaction completely evacuated the young generation then we
  // can clear the card table.  Otherwise, we must invalidate
  // it (consider all cards dirty).  In the future, we might consider doing
  // compaction within generations only, and doing card-table sliding.
  CardTableRS* rs = gch->rem_set();
  Generation* old_gen = gch->old_gen();

  // Clear/invalidate below make use of the "prev_used_regions" saved earlier.
  if (gch->young_gen()->used() == 0) {
    // We've evacuated the young generation.
    rs->clear_into_younger(old_gen);
  } else {
    // Invalidate the cards corresponding to the currently used
    // region and clear those corresponding to the evacuated region.
    rs->invalidate_or_clear(old_gen);
  }

  CodeCache::gc_epilogue();
  JvmtiExport::gc_epilogue();

  // refs processing: clean slate
  set_ref_processor(NULL);

  // Update heap occupancy information which is used as
  // input to soft ref clearing policy at the next gc.
  Universe::update_heap_info_at_gc();

  // Update time of last gc for all generations we collected
  // (which currently is all the generations in the heap).
  // We need to use a monotonically non-decreasing time in ms
  // or we will see time-warp warnings and os::javaTimeMillis()
  // does not guarantee monotonicity.
  jlong now = os::javaTimeNanos() / NANOSECS_PER_MILLISEC;
  gch->update_time_of_last_gc(now);

  gch->trace_heap_after_gc(_gc_tracer);
}
```

 最终通过`invoke_at_safepoint`来进行垃圾清理。这个具体过程我们就先跳过。

### 2）、ConcurrentMarkSweepGeneration

```
bool GenCollectedHeap::create_cms_collector() {
........
  CMSCollector* collector =
    new CMSCollector((ConcurrentMarkSweepGeneration*)_old_gen,
                     _rem_set,
                     _gen_policy->as_concurrent_mark_sweep_policy());
 ..........
  return true;  // success
}
```

```
void ConcurrentMarkSweepGeneration::collect(bool   full,
                                            bool   clear_all_soft_refs,
                                            size_t size,
                                            bool   tlab)
{
  collector()->collect(full, clear_all_soft_refs, size, tlab);
}
```

 可以看到这两种`Generation`对于`collect()`方法订单实现是不同的，`CMS`是通过`CMSCollector`去处理的。

```
void CMSCollector::collect(bool   full,
                           bool   clear_all_soft_refs,
                           size_t size,
                           bool   tlab)
{
  .......
  acquire_control_and_collect(full, clear_all_soft_refs);
}
```

```
void CMSCollector::acquire_control_and_collect(bool full,
        bool clear_all_soft_refs) {
 ......
  CollectorState first_state = _collectorState;
........
  // Inform cms gen if this was due to partial collection failing.
  // The CMS gen may use this fact to determine its expansion policy.
  GenCollectedHeap* gch = GenCollectedHeap::heap();
..........
  do_compaction_work(clear_all_soft_refs);
 ......
  return;
}
```

```
// A work method used by the foreground collector to do
// a mark-sweep-compact.
void CMSCollector::do_compaction_work(bool clear_all_soft_refs) {
  GenCollectedHeap* gch = GenCollectedHeap::heap();
  ..........
  GenMarkSweep::invoke_at_safepoint(ref_processor(), clear_all_soft_refs);
 ......
  // For a mark-sweep-compact, compute_new_size() will be called
  // in the heap's do_collection() method.
}
```

 可以看到其最终还是调用的`GenMarkSweep::invoke_at_safepoint(ref_processor(), clear_all_soft_refs)`,与前面的`TenuredGeneration`是一样的。

## 五、ParallelGC

 下面我们按照上面的思路来梳理下`ParallelGC`的初始化创建及垃圾收集过程。

按照前面对`Heap`的创建过程，在使用`UseParallelGC`的时候：

```
if (UseParallelGC) {
  return Universe::create_heap_with_policy<ParallelScavengeHeap, GenerationSizer>();
}
```

## 1、ParallelScavengeHeap

```
class ParallelScavengeHeap : public CollectedHeap {
  friend class VMStructs;
 private:
  static PSYoungGen* _young_gen;
  static PSOldGen*   _old_gen;

  // Sizing policy for entire heap
  static PSAdaptiveSizePolicy*       _size_policy;
  static PSGCAdaptivePolicyCounters* _gc_policy_counters;
  GenerationSizer* _collector_policy;

  // Collection of generations that are adjacent in the
  // space reserved for the heap.
  AdjoiningGenerations* _gens;
  unsigned int _death_march_count;
.......
  // For use by VM operations
  enum CollectionType {
    Scavenge,
    MarkSweep
  };
.....
  virtual const char* name() const {
    return "Parallel";
  }
    ......
```

这里的年轻代是`PSYoungGen`、老年代时候`PSOldGen`：

### 1）、PSYoungGen

```
class PSYoungGen : public CHeapObj<mtGC> {
  friend class VMStructs;
  friend class ParallelScavengeHeap;
  friend class AdjoiningGenerations;
 protected:
  MemRegion       _reserved;
  PSVirtualSpace* _virtual_space;
  // Spaces
  MutableSpace* _eden_space;
  MutableSpace* _from_space;
  MutableSpace* _to_space;
  // MarkSweep Decorators
  PSMarkSweepDecorator* _eden_mark_sweep;
  PSMarkSweepDecorator* _from_mark_sweep;
  PSMarkSweepDecorator* _to_mark_sweep;

  // Sizing information, in bytes, set in constructor
  const size_t _init_gen_size;
  const size_t _min_gen_size;
  const size_t _max_gen_size;

  // Performance counters
  PSGenerationCounters*     _gen_counters;
  SpaceCounters*            _eden_counters;
  SpaceCounters*            _from_counters;
  SpaceCounters*            _to_counters;
```

 这些属性看名称就大概了解其是用来记录什么信息的了。

### 2）、PSOldGen

```
class PSOldGen : public CHeapObj<mtGC> {
  friend class VMStructs;
  friend class PSPromotionManager; // Uses the cas_allocate methods
  friend class ParallelScavengeHeap;
  friend class AdjoiningGenerations;

 protected:
  MemRegion                _reserved;          // Used for simple containment tests
  PSVirtualSpace*          _virtual_space;     // Controls mapping and unmapping of virtual mem
  ObjectStartArray         _start_array;       // Keeps track of where objects start in a 512b block
  MutableSpace*            _object_space;      // Where all the objects live
  PSMarkSweepDecorator*    _object_mark_sweep; // The mark sweep view of _object_space
  const char* const        _name;              // Name of this generation.

  // Performance Counters
  PSGenerationCounters*    _gen_counters;
  SpaceCounters*           _space_counters;
```

### 3）、ASPSOldGen

```
class ASPSOldGen : public PSOldGen {
    .......
        // Debugging support
  virtual const char* short_name() const { return "ASPSOldGen"; }
    .........
```

### 4)、ASPSYoungGen

```
class ASPSYoungGen : public PSYoungGen {
    ........
 // Printing support
  virtual const char* short_name() const { return "ASPSYoungGen"; }
    ........
```

 可以看到`ASPSYoungGen`&`ASPSOldGen`都是直接继承的`PSYoungGen`&`PSOldGen`。这两个的区别我们后面会讲

### 5）、ParallelScavengeHeap的initialize方法

```
jint ParallelScavengeHeap::initialize() {
  CollectedHeap::pre_initialize();

  const size_t heap_size = _collector_policy->max_heap_byte_size();

  ReservedSpace heap_rs = Universe::reserve_heap(heap_size, _collector_policy->heap_alignment());
.........

  initialize_reserved_region((HeapWord*)heap_rs.base(), (HeapWord*)(heap_rs.base() + heap_rs.size()));

  CardTableExtension* const barrier_set = new CardTableExtension(reserved_region());
  barrier_set->initialize();
  set_barrier_set(barrier_set);

  // Make up the generations
  // Calculate the maximum size that a generation can grow.  This
  // includes growth into the other generation.  Note that the
  // parameter _max_gen_size is kept as the maximum
  // size of the generation as the boundaries currently stand.
  ..........
  _gens = new AdjoiningGenerations(heap_rs, _collector_policy, generation_alignment());

  _old_gen = _gens->old_gen();
  _young_gen = _gens->young_gen();
.......
  _size_policy =
    new PSAdaptiveSizePolicy(eden_capacity,
                             initial_promo_size,
                             young_gen()->to_space()->capacity_in_bytes(),
                             _collector_policy->gen_alignment(),
                             max_gc_pause_sec,
                             max_gc_minor_pause_sec,
                             GCTimeRatio
                             );
  _gc_policy_counters =
    new PSGCAdaptivePolicyCounters("ParScav:MSC", 2, 3, _size_policy);

  // Set up the GCTaskManager
  _gc_task_manager = GCTaskManager::create(ParallelGCThreads);
.......
  return JNI_OK;
}
```

 这里主要是初始化了对空间处理的对象`AdjoiningGenerations`。

### 6）、AdjoiningGenerations

```
AdjoiningGenerations::AdjoiningGenerations(ReservedSpace old_young_rs,
                                           GenerationSizer* policy,
                                           size_t alignment) :
 .......
  // Create the generations differently based on the option to
  // move the boundary.
  if (UseAdaptiveGCBoundary) {
    // Initialize the adjoining virtual spaces.  Then pass the
    // a virtual to each generation for initialization of the
    // generation.

    // Does the actual creation of the virtual spaces
    _virtual_spaces.initialize(max_low_byte_size,
                               init_low_byte_size,
                               init_high_byte_size);

    // Place the young gen at the high end.  Passes in the virtual space.
    _young_gen = new ASPSYoungGen(_virtual_spaces.high(),
                                  _virtual_spaces.high()->committed_size(),
                                  min_high_byte_size,
                                  _virtual_spaces.high_byte_size_limit());

    // Place the old gen at the low end. Passes in the virtual space.
    _old_gen = new ASPSOldGen(_virtual_spaces.low(),
                              _virtual_spaces.low()->committed_size(),
                              min_low_byte_size,
                              _virtual_spaces.low_byte_size_limit(),
                              "old", 1);
    young_gen()->initialize_work();
    old_gen()->initialize_work("old", 1);
  } else {
    // Layout the reserved space for the generations.
    ReservedSpace old_rs   =
      virtual_spaces()->reserved_space().first_part(max_low_byte_size);
    ReservedSpace heap_rs  =
      virtual_spaces()->reserved_space().last_part(max_low_byte_size);
    ReservedSpace young_rs = heap_rs.first_part(max_high_byte_size);
    assert(young_rs.size() == heap_rs.size(), "Didn't reserve all of the heap");
    // Create the generations.  Virtual spaces are not passed in.
    _young_gen = new PSYoungGen(init_high_byte_size,
                                min_high_byte_size,
                                max_high_byte_size);
    _old_gen = new PSOldGen(init_low_byte_size,
                            min_low_byte_size,
                            max_low_byte_size,
                            "old", 1);
    // The virtual spaces are created by the initialization of the gens.
    _young_gen->initialize(young_rs, alignment);
  ......
  }
}
```

 这里主要是有一个参数`UseAdaptiveGCBoundary`，如果在JVM启动的时候有设置这个参数，就表示是由JVM自动决定年轻代、老年代的内存分配，可以看到如果有这个参数，其使用的是`ASPSYoungGen`&`ASPSOldGen`，没有的话就是使用`PSYoungGen`&`PSOldGen`。

```
PSOldGen::PSOldGen(ReservedSpace rs, size_t alignment,
                   size_t initial_size, size_t min_size, size_t max_size,
                   const char* perf_data_name, int level):
  _name(select_name()), _init_gen_size(initial_size), _min_gen_size(min_size),
  _max_gen_size(max_size)
{
  initialize(rs, alignment, perf_data_name, level);
}
```

 我们可以看到在初始化的时候，这里是会有设置名称的：

```
inline const char* PSOldGen::select_name() {
  return UseParallelOldGC ? "ParOldGen" : "PSOldGen";
}
```

也就是如果打印GC日志的话，如果使用到的`UseParallelOldGC`，老年代名称就会是`ParOldGen`，不然就会是`PSOldGen`，这里我最开始搞错了，因为使用的是`short_name()`的字符名称。

### 7）、Gen的初始化

```
void PSYoungGen::initialize(ReservedSpace rs, size_t alignment) {
  initialize_virtual_space(rs, alignment);
  initialize_work();
}
```

```
void PSYoungGen::initialize_work() {

  _reserved = MemRegion((HeapWord*)virtual_space()->low_boundary(),
                        (HeapWord*)virtual_space()->high_boundary());
.......
  if (UseNUMA) {
    _eden_space = new MutableNUMASpace(virtual_space()->alignment());
  } else {
    _eden_space = new MutableSpace(virtual_space()->alignment());
  }
  _from_space = new MutableSpace(virtual_space()->alignment());
  _to_space   = new MutableSpace(virtual_space()->alignment());

  if (_eden_space == NULL || _from_space == NULL || _to_space == NULL) {
    vm_exit_during_initialization("Could not allocate a young gen space");
  }
  // Allocate the mark sweep views of spaces
  _eden_mark_sweep =
      new PSMarkSweepDecorator(_eden_space, NULL, MarkSweepDeadRatio);
  _from_mark_sweep =
      new PSMarkSweepDecorator(_from_space, NULL, MarkSweepDeadRatio);
  _to_mark_sweep =
      new PSMarkSweepDecorator(_to_space, NULL, MarkSweepDeadRatio);
  // Generation Counters - generation 0, 3 subspaces
  _gen_counters = new PSGenerationCounters("new", 0, 3, _min_gen_size,
                                           _max_gen_size, _virtual_space);

  // Compute maximum space sizes for performance counters
  ParallelScavengeHeap* heap = ParallelScavengeHeap::heap();
.......
  if (UseAdaptiveSizePolicy) {
    max_survivor_size = size / MinSurvivorRatio;

    // round the survivor space size down to the nearest alignment
    // and make sure its size is greater than 0.
    max_survivor_size = align_size_down(max_survivor_size, alignment);
    max_survivor_size = MAX2(max_survivor_size, alignment);

    // set the maximum size of eden to be the size of the young gen
    // less two times the minimum survivor size. The minimum survivor
    // size for UseAdaptiveSizePolicy is one alignment.
    max_eden_size = size - 2 * alignment;
  } else {
    max_survivor_size = size / InitialSurvivorRatio;

    // round the survivor space size down to the nearest alignment
    // and make sure its size is greater than 0.
    max_survivor_size = align_size_down(max_survivor_size, alignment);
    max_survivor_size = MAX2(max_survivor_size, alignment);

    // set the maximum size of eden to be the size of the young gen
    // less two times the survivor size when the generation is 100%
    // committed. The minimum survivor size for -UseAdaptiveSizePolicy
    // is dependent on the committed portion (current capacity) of the
    // generation - the less space committed, the smaller the survivor
    // space, possibly as small as an alignment. However, we are interested
    // in the case where the young generation is 100% committed, as this
    // is the point where eden reaches its maximum size. At this point,
    // the size of a survivor space is max_survivor_size.
    max_eden_size = size - 2 * max_survivor_size;
  }
  _eden_counters = new SpaceCounters("eden", 0, max_eden_size, _eden_space,
                                     _gen_counters);
  _from_counters = new SpaceCounters("s0", 1, max_survivor_size, _from_space,
                                     _gen_counters);
  _to_counters = new SpaceCounters("s1", 2, max_survivor_size, _to_space,
                                   _gen_counters);
}
```

```
void PSOldGen::initialize_work(const char* perf_data_name, int level) {
  MemRegion limit_reserved((HeapWord*)virtual_space()->low_boundary(),
    heap_word_size(_max_gen_size));
 ......
  ParallelScavengeHeap* heap = ParallelScavengeHeap::heap();
 ......
  _object_space = new MutableSpace(virtual_space()->alignment());
....
  object_space()->initialize(cmr,
                             SpaceDecorator::Clear,
                             SpaceDecorator::Mangle);

  _object_mark_sweep = new PSMarkSweepDecorator(_object_space, start_array(), MarkSweepDeadRatio);
...........
}
```

## 2、VM_ParallelGCFailedAllocation

同样我们来看下`VM_CollectForAllocation`的子类`VM_ParallelGCFailedAllocation`

```
class VM_ParallelGCFailedAllocation : public VM_CollectForAllocation {
 public:
  VM_ParallelGCFailedAllocation(size_t word_size, uint gc_count);

  virtual VMOp_Type type() const {
    return VMOp_ParallelGCFailedAllocation;
  }
  virtual void doit();
};
```

 我们再来看下其`doit()`方法的实现：

![在这里插入图片描述](https://img-blog.csdnimg.cn/20210314122003379.png?x-oss-process=image/watermark,type_ZmFuZ3poZW5naGVpdGk,shadow_10,text_aHR0cHM6Ly9ibG9nLmNzZG4ubmV0L3FxXzI1MTc5NDgx,size_16,color_FFFFFF,t_70#pic_center)

### 1）、doit()

```
oid VM_ParallelGCFailedAllocation::doit() {
  SvcGCMarker sgcm(SvcGCMarker::MINOR);

  ParallelScavengeHeap* heap = ParallelScavengeHeap::heap();

  GCCauseSetter gccs(heap, _gc_cause);
  _result = heap->failed_mem_allocate(_word_size);

  if (_result == NULL && GCLocker::is_active_and_needs_gc()) {
    set_gc_locked();
  }
}
```

### 2）、failed_mem_allocate(_word_size)

```
HeapWord* ParallelScavengeHeap::failed_mem_allocate(size_t size) {
....
  const bool invoked_full_gc = PSScavenge::invoke();
  HeapWord* result = young_gen()->allocate(size);

  // Second level allocation failure.
  //   Mark sweep and allocate in young generation.
  if (result == NULL && !invoked_full_gc) {
    do_full_collection(false);
    result = young_gen()->allocate(size);
  }

  death_march_check(result, size);

  // Third level allocation failure.
  //   After mark sweep and young generation allocation failure,
  //   allocate in old generation.
  if (result == NULL) {
    result = old_gen()->allocate(size);
  }

  // Fourth level allocation failure. We're running out of memory.
  //   More complete mark sweep and allocate in young generation.
  if (result == NULL) {
    do_full_collection(true);
    result = young_gen()->allocate(size);
  }

  // Fifth level allocation failure.
  //   After more complete mark sweep, allocate in old generation.
  if (result == NULL) {
    result = old_gen()->allocate(size);
  }

  return result;
}
```

 这里首先是通过`PSScavenge::invoke()`进行回收，然后再在`young_gen()`年轻代申请内存，如果成功就能return了，如果失败了，判断刚才是不是执行的`invoked_full_gc`，如果还没有执行`full_gc`，则通过`do_full_collection`再进行GC，然后再按年轻代-》老年代的顺序申请分配。

```
bool PSScavenge::invoke() {
  ParallelScavengeHeap* const heap = ParallelScavengeHeap::heap();
  PSAdaptiveSizePolicy* policy = heap->size_policy();
  IsGCActiveMark mark;
  const bool scavenge_done = PSScavenge::invoke_no_policy();
  const bool need_full_gc = !scavenge_done ||
    policy->should_full_GC(heap->old_gen()->free_in_bytes());
  bool full_gc_done = false;

  if (UsePerfData) {
    PSGCAdaptivePolicyCounters* const counters = heap->gc_policy_counters();
    const int ffs_val = need_full_gc ? full_follows_scavenge : not_skipped;
    counters->update_full_follows_scavenge(ffs_val);
  }
  if (need_full_gc) {
    GCCauseSetter gccs(heap, GCCause::_adaptive_size_policy);
    CollectorPolicy* cp = heap->collector_policy();
    const bool clear_all_softrefs = cp->should_clear_all_soft_refs();

    if (UseParallelOldGC) {
      full_gc_done = PSParallelCompact::invoke_no_policy(clear_all_softrefs);
    } else {
      full_gc_done = PSMarkSweep::invoke_no_policy(clear_all_softrefs);
    }
  }

  return full_gc_done;
}
```

可以看到这里进行GC的时候，会判断`UseParallelOldGC`，分别选择不同的类型进行GC，分别选择`PSParallelCompact`或`PSMarkSweep`。

 以上我们就分析了三种主要GC类型的初始化&垃圾收集的初始垃圾处理。然后`G1`垃圾收集器，大体也是这个过程，就不再具体展开了。

 再看还需不需要分析下具体收集过程，这个要明白以及这几种GC收集器为什么要怎样设计，以及内部的不同之处能带来性能上的差异，还是比较困难，这个以后再说吧。下一篇我们就来看下各种不同GC的具体使用，再结合源码来看下其为什么要这样传入参数。
