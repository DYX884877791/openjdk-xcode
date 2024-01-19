/*
 * Copyright (c) 1999, 2020, Oracle and/or its affiliates. All rights reserved.
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

#ifndef SHARE_VM_MEMORY_THREADLOCALALLOCBUFFER_HPP
#define SHARE_VM_MEMORY_THREADLOCALALLOCBUFFER_HPP

#include "gc_implementation/shared/gcUtil.hpp"
#include "oops/typeArrayOop.hpp"
#include "runtime/perfData.hpp"

class GlobalTLABStats;

// ThreadLocalAllocBuffer: a descriptor for thread-local storage used by
// the threads for allocation.
//            It is thread-private at any time, but maybe multiplexed over
//            time across multiple threads. The park()/unpark() pair is
//            used to make it available for such multiplexing.
//
// 创建对象时，需要在堆上申请指定大小的内存，如果同时有大量线程申请内存的话，可以通过CAS乐观锁机制确保不会申请到同一块内存，在JVM运行中，内存分配时一个极其频繁的动作，这种方式势必会降低性能。
//  因此，在HotSpot1.6的实现中引入了TLAB技术。
//
//  什么是TLAB：
//  TLAB全称ThreadLocalAllocationBuffer，是线程的一块私有内存，如果设置了虚拟机参数 -XX：UseTLAB，在线程初始化时，同时也会申请一块指定大小的内存，
//  只给当前线程使用，这样每个线程都单独拥有一个Buffer，如果需要分配内存，就在自己的Buffer上分配，这样就不会存在竞争的情况，可以大大提升分配效率，
//  当Buffer容量不够的时候，再重新从Eden区域申请一个继续使用，这个申请动作还是需要原子操作的。
//
//  TLAB的目的是在为新对象分配内存空间时，让每个Java应用线程能在使用自己专属的分配指针来分配空间，均摊GC堆（eden区）里共享的分配指针做更新而带来的同步开销。
//
//  TLAB只是让每个线程私有的分配指针，但底下存对象的内存空间还是给所有线程访问的，只是其他线程无法在这个区域分配而已。当一个TLAB用满（分配指针top撞上分配极限end了），
//  就新申请一个TLAB，而在老TLAB里对象还留在原地什么都不用管——它们无法感知自己是否是曾经从TLAB分配出来的，而只关心自己是在eden里分配的。
//
// ThreadLocalAllocBuffer的定义位于hotspot/src/share/vm/memory/ThreadLocalAllocBuffer.hpp中，具体的实现在同目录的ThreadLocalAllocBuffer.inline.cpp
// 和ThreadLocalAllocBuffer.cpp中，该类表示由线程私有的内存区域，简称TLAB，主要是为了解决在Eden区分配内存时锁竞争导致性能下降的问题。
// 这块区域是线程私有的，这里的私有具体是指只能由该线程在这块内存区域中分配对象，但是已经分配的对象其他线程也可以正常访问。
// 继承自CHeapObj
class ThreadLocalAllocBuffer: public CHeapObj<mtThread> {
  friend class VMStructs;
private:
  HeapWord* _start;                              // address of TLAB TLAB 起始地址，表示堆内存地址都用 HeapWord*
  HeapWord* _top;                                // address after last allocation   上次分配的内存地址
  HeapWord* _pf_top;                             // allocation prefetch watermark   Allocation Prefetch CPU 缓存优化机制相关需要的参数，这里先不用考虑
  HeapWord* _end;                                // allocation end (excluding alignment_reserve)    TLAB 结束地址
  size_t    _desired_size;                       // desired size   (including alignment_reserve)    TLAB 大小 包括保留空间，表示内存大小都需要通过 size_t 类型，也就是实际字节数除以 HeapWordSize 的值
  size_t    _refill_waste_limit;                 // hold onto tlab if free() is larger than this    TLAB最大浪费空间，剩余空间不足分配浪费空间限制。在TLAB剩余空间不足的时候，根据这个值决定分配策略，如果浪费空间大于这个值则直接在 Eden 区分配，如果小于这个值则将当前 TLAB 放回 Eden 区管理并从 Eden 申请新的 TLAB 进行分配。
  size_t    _allocated_before_last_gc;           // total bytes allocated up until the last gc

  static size_t   _max_size;                     // maximum size of any TLAB
  static unsigned _target_refills;               // expected number of refills between GCs

  unsigned  _number_of_refills;
  unsigned  _fast_refill_waste;
  unsigned  _slow_refill_waste;
  unsigned  _gc_waste;
  unsigned  _slow_allocations;

  AdaptiveWeightedAverage _allocation_fraction;  // fraction of eden allocated in tlabs

  void accumulate_statistics();
  void initialize_statistics();

  void set_start(HeapWord* start)                { _start = start; }
  void set_end(HeapWord* end)                    { _end = end; }
  void set_top(HeapWord* top)                    { _top = top; }
  void set_pf_top(HeapWord* pf_top)              { _pf_top = pf_top; }
  void set_desired_size(size_t desired_size)     { _desired_size = desired_size; }
  void set_refill_waste_limit(size_t waste)      { _refill_waste_limit = waste;  }

  size_t initial_refill_waste_limit()            { return desired_size() / TLABRefillWasteFraction; }

  static int    target_refills()                 { return _target_refills; }
  size_t initial_desired_size();

  size_t remaining() const                       { return end() == NULL ? 0 : pointer_delta(hard_end(), top()); }

  // Make parsable and release it.
  void reset();

  // Resize based on amount of allocation, etc.
  void resize();

  void invariants() const { assert(top() >= start() && top() <= end(), "invalid tlab"); }

  void initialize(HeapWord* start, HeapWord* top, HeapWord* end);

  void print_stats(const char* tag);

  Thread* myThread();

  // statistics

  int number_of_refills() const { return _number_of_refills; }
  int fast_refill_waste() const { return _fast_refill_waste; }
  int slow_refill_waste() const { return _slow_refill_waste; }
  int gc_waste() const          { return _gc_waste; }
  int slow_allocations() const  { return _slow_allocations; }

  static GlobalTLABStats* _global_stats;
  static GlobalTLABStats* global_stats() { return _global_stats; }

public:
  ThreadLocalAllocBuffer() : _allocation_fraction(TLABAllocationWeight), _allocated_before_last_gc(0) {
    // do nothing.  tlabs must be inited by initialize() calls
  }

  static const size_t min_size()                 { return align_object_size(MinTLABSize / HeapWordSize) + alignment_reserve(); }
  static const size_t max_size()                 { assert(_max_size != 0, "max_size not set up"); return _max_size; }
  static void set_max_size(size_t max_size)      { _max_size = max_size; }

  HeapWord* start() const                        { return _start; }
  HeapWord* end() const                          { return _end; }
  HeapWord* hard_end() const                     { return _end + alignment_reserve(); }
  HeapWord* top() const                          { return _top; }
  HeapWord* pf_top() const                       { return _pf_top; }
  size_t desired_size() const                    { return _desired_size; }
  size_t used() const                            { return pointer_delta(top(), start()); }
  size_t used_bytes() const                      { return pointer_delta(top(), start(), 1); }
  size_t free() const                            { return pointer_delta(end(), top()); }
  // Don't discard tlab if remaining space is larger than this.
  size_t refill_waste_limit() const              { return _refill_waste_limit; }
  size_t hard_size_bytes() const                 { return pointer_delta(hard_end(), start(), 1); }
  bool in_used(HeapWord* addr) const             { return addr >= start() && addr < top(); }

  // Allocate size HeapWords. The memory is NOT initialized to zero.
  inline HeapWord* allocate(size_t size);

  // Reserve space at the end of TLAB
  static size_t end_reserve() {
    int reserve_size = typeArrayOopDesc::header_size(T_INT);
    return MAX2(reserve_size, VM_Version::reserve_for_allocation_prefetch());
  }
  static size_t alignment_reserve()              { return align_object_size(end_reserve()); }
  static size_t alignment_reserve_in_bytes()     { return alignment_reserve() * HeapWordSize; }

  // Return tlab size or remaining space in eden such that the
  // space is large enough to hold obj_size and necessary fill space.
  // Otherwise return 0;
  inline size_t compute_size(size_t obj_size);

  // Record slow allocation
  inline void record_slow_allocation(size_t obj_size);

  // Initialization at startup
  static void startup_initialization();

  // Make an in-use tlab parsable, optionally also retiring it.
  void make_parsable(bool retire);

  // Retire in-use tlab before allocation of a new tlab
  void clear_before_allocation();

  // Accumulate statistics across all tlabs before gc
  static void accumulate_statistics_before_gc();

  // Resize tlabs for all threads
  static void resize_all_tlabs();

  void fill(HeapWord* start, HeapWord* top, size_t new_size);
  void initialize();

  static size_t refill_waste_limit_increment()   { return TLABWasteIncrement; }

  // Code generation support
  static ByteSize start_offset()                 { return byte_offset_of(ThreadLocalAllocBuffer, _start); }
  static ByteSize end_offset()                   { return byte_offset_of(ThreadLocalAllocBuffer, _end  ); }
  static ByteSize top_offset()                   { return byte_offset_of(ThreadLocalAllocBuffer, _top  ); }
  static ByteSize pf_top_offset()                { return byte_offset_of(ThreadLocalAllocBuffer, _pf_top  ); }
  static ByteSize size_offset()                  { return byte_offset_of(ThreadLocalAllocBuffer, _desired_size ); }
  static ByteSize refill_waste_limit_offset()    { return byte_offset_of(ThreadLocalAllocBuffer, _refill_waste_limit ); }

  static ByteSize number_of_refills_offset()     { return byte_offset_of(ThreadLocalAllocBuffer, _number_of_refills ); }
  static ByteSize fast_refill_waste_offset()     { return byte_offset_of(ThreadLocalAllocBuffer, _fast_refill_waste ); }
  static ByteSize slow_allocations_offset()      { return byte_offset_of(ThreadLocalAllocBuffer, _slow_allocations ); }

  void verify();
};

class GlobalTLABStats: public CHeapObj<mtThread> {
private:

  // Accumulate perfdata in private variables because
  // PerfData should be write-only for security reasons
  // (see perfData.hpp)
  unsigned _allocating_threads;
  unsigned _total_refills;
  unsigned _max_refills;
  size_t   _total_allocation;
  size_t   _total_gc_waste;
  size_t   _max_gc_waste;
  size_t   _total_slow_refill_waste;
  size_t   _max_slow_refill_waste;
  size_t   _total_fast_refill_waste;
  size_t   _max_fast_refill_waste;
  unsigned _total_slow_allocations;
  unsigned _max_slow_allocations;

  PerfVariable* _perf_allocating_threads;
  PerfVariable* _perf_total_refills;
  PerfVariable* _perf_max_refills;
  PerfVariable* _perf_allocation;
  PerfVariable* _perf_gc_waste;
  PerfVariable* _perf_max_gc_waste;
  PerfVariable* _perf_slow_refill_waste;
  PerfVariable* _perf_max_slow_refill_waste;
  PerfVariable* _perf_fast_refill_waste;
  PerfVariable* _perf_max_fast_refill_waste;
  PerfVariable* _perf_slow_allocations;
  PerfVariable* _perf_max_slow_allocations;

  AdaptiveWeightedAverage _allocating_threads_avg;

public:
  GlobalTLABStats();

  // Initialize all counters
  void initialize();

  // Write all perf counters to the perf_counters
  void publish();

  void print();

  // Accessors
  unsigned allocating_threads_avg() {
    return MAX2((unsigned)(_allocating_threads_avg.average() + 0.5), 1U);
  }

  size_t allocation() {
    return _total_allocation;
  }

  // Update methods

  void update_allocating_threads() {
    _allocating_threads++;
  }
  void update_number_of_refills(unsigned value) {
    _total_refills += value;
    _max_refills    = MAX2(_max_refills, value);
  }
  void update_allocation(size_t value) {
    _total_allocation += value;
  }
  void update_gc_waste(size_t value) {
    _total_gc_waste += value;
    _max_gc_waste    = MAX2(_max_gc_waste, value);
  }
  void update_fast_refill_waste(size_t value) {
    _total_fast_refill_waste += value;
    _max_fast_refill_waste    = MAX2(_max_fast_refill_waste, value);
  }
  void update_slow_refill_waste(size_t value) {
    _total_slow_refill_waste += value;
    _max_slow_refill_waste    = MAX2(_max_slow_refill_waste, value);
  }
  void update_slow_allocations(unsigned value) {
    _total_slow_allocations += value;
    _max_slow_allocations    = MAX2(_max_slow_allocations, value);
  }
};

#endif // SHARE_VM_MEMORY_THREADLOCALALLOCBUFFER_HPP
