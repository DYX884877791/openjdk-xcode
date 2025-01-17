/*
 * Copyright (c) 2003, 2013, Oracle and/or its affiliates. All rights reserved.
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

#ifndef SHARE_VM_SERVICES_MEMORYPOOL_HPP
#define SHARE_VM_SERVICES_MEMORYPOOL_HPP

#include "gc_implementation/shared/mutableSpace.hpp"
#include "memory/defNewGeneration.hpp"
#include "memory/heap.hpp"
#include "memory/space.hpp"
#include "services/memoryUsage.hpp"
#include "utilities/macros.hpp"
#if INCLUDE_ALL_GCS
#include "gc_implementation/concurrentMarkSweep/compactibleFreeListSpace.hpp"
#endif // INCLUDE_ALL_GCS

// A memory pool represents the memory area that the VM manages.
// The Java virtual machine has at least one memory pool
// and it may create or remove memory pools during execution.
// A memory pool can belong to the heap or the non-heap memory.
// A Java virtual machine may also have memory pools belonging to
// both heap and non-heap memory.

// Forward declaration
class MemoryManager;
class SensorInfo;
class Generation;
class DefNewGeneration;
class ThresholdSupport;

// MemoryPool代表一块由JVM管理的内存区域，可以在运行时由JVM创建或者移除。MemoryPool只有两种类型，Heap或者NonHeap，
// 通过枚举PoolType表示，部分MemoryPool可能同时属于这两种类型。
// MemoryPool的定义位于hotspot/src/shared/vm/service/memoryPool.hpp中
class MemoryPool : public CHeapObj<mtInternal> {
  friend class MemoryManager;
 public:
  enum PoolType {
    Heap    = 1,
    NonHeap = 2
  };

 private:
  enum {
    max_num_managers = 5
  };

  // We could make some of the following as performance counters
  // for external monitoring.
    //名字
  const char*      _name;
    //类型
  PoolType         _type;
    //初始大小
  size_t           _initial_size;
    //最大大小
  size_t           _max_size;
    //能否分配内存，默认为true
  bool             _available_for_allocation; // Default is true
    //该MemoryPool所属的MemoryManager数组，max_num_managers是一个枚举值，为5
  MemoryManager*   _managers[max_num_managers];
    //该MemoryPool的MemoryManager的个数
  int              _num_managers;
    // 用来记录MemoryUsage各指标的在GC前的历史最高值
  MemoryUsage      _peak_usage;               // Peak memory usage
    // GC结束后的内存占用情况
  MemoryUsage      _after_gc_usage;           // After GC memory usage

  ThresholdSupport* _usage_threshold;
    //触发GC的阈值
  ThresholdSupport* _gc_usage_threshold;

    //达到阈值时触发的动作
  SensorInfo*      _usage_sensor;
  SensorInfo*      _gc_usage_sensor;

    //该MemoryPool对应的Java对象实例
  volatile instanceOop _memory_pool_obj;

  void add_manager(MemoryManager* mgr);

 public:
  MemoryPool(const char* name,
             PoolType type,
             size_t init_size,
             size_t max_size,
             bool support_usage_threshold,
             bool support_gc_threshold);

  const char* name()                       { return _name; }
  bool        is_heap()                    { return _type == Heap; }
  bool        is_non_heap()                { return _type == NonHeap; }
  size_t      initial_size()   const       { return _initial_size; }
  int         num_memory_managers() const  { return _num_managers; }
  // max size could be changed
  virtual size_t max_size()    const       { return _max_size; }

  bool is_pool(instanceHandle pool) { return (pool() == _memory_pool_obj); }

  bool available_for_allocation()   { return _available_for_allocation; }
  bool set_available_for_allocation(bool value) {
    bool prev = _available_for_allocation;
    _available_for_allocation = value;
    return prev;
  }

  MemoryManager* get_memory_manager(int index) {
    assert(index >= 0 && index < _num_managers, "Invalid index");
    return _managers[index];
  }

  // Records current memory usage if it's a peak usage
  void record_peak_memory_usage();

  MemoryUsage get_peak_memory_usage() {
    // check current memory usage first and then return peak usage
    record_peak_memory_usage();
    return _peak_usage;
  }
  void        reset_peak_memory_usage() {
    _peak_usage = get_memory_usage();
  }

  ThresholdSupport* usage_threshold()      { return _usage_threshold; }
  ThresholdSupport* gc_usage_threshold()   { return _gc_usage_threshold; }

  SensorInfo*       usage_sensor()         {  return _usage_sensor; }
  SensorInfo*       gc_usage_sensor()      { return _gc_usage_sensor; }

  void        set_usage_sensor_obj(instanceHandle s);
  void        set_gc_usage_sensor_obj(instanceHandle s);
  void        set_last_collection_usage(MemoryUsage u)  { _after_gc_usage = u; }

  virtual instanceOop get_memory_pool_instance(TRAPS);
  virtual MemoryUsage get_memory_usage() = 0;
  virtual size_t      used_in_bytes() = 0;
  virtual bool        is_collected_pool()         { return false; }
  virtual MemoryUsage get_last_collection_usage() { return _after_gc_usage; }

  // GC support
  void oops_do(OopClosure* f);
};

// CollectedMemoryPool及其子类对应分配Java对象的堆内存
class CollectedMemoryPool : public MemoryPool {
public:
  CollectedMemoryPool(const char* name, PoolType type, size_t init_size, size_t max_size, bool support_usage_threshold) :
    MemoryPool(name, type, init_size, max_size, support_usage_threshold, true) {};
  bool is_collected_pool()            { return true; }
};

class ContiguousSpacePool : public CollectedMemoryPool {
private:
  ContiguousSpace* _space;

public:
  ContiguousSpacePool(ContiguousSpace* space, const char* name, PoolType type, size_t max_size, bool support_usage_threshold);

  ContiguousSpace* space()              { return _space; }
  MemoryUsage get_memory_usage();
  size_t used_in_bytes()                { return space()->used(); }
};

class SurvivorContiguousSpacePool : public CollectedMemoryPool {
private:
  DefNewGeneration* _gen;

public:
  SurvivorContiguousSpacePool(DefNewGeneration* gen,
                              const char* name,
                              PoolType type,
                              size_t max_size,
                              bool support_usage_threshold);

  MemoryUsage get_memory_usage();

  size_t used_in_bytes() {
    return _gen->from()->used();
  }
  size_t committed_in_bytes() {
    return _gen->from()->capacity();
  }
};

#if INCLUDE_ALL_GCS
class CompactibleFreeListSpacePool : public CollectedMemoryPool {
private:
  CompactibleFreeListSpace* _space;
public:
  CompactibleFreeListSpacePool(CompactibleFreeListSpace* space,
                               const char* name,
                               PoolType type,
                               size_t max_size,
                               bool support_usage_threshold);

  MemoryUsage get_memory_usage();
  size_t used_in_bytes()            { return _space->used_stable(); }
};
#endif // INCLUDE_ALL_GCS


class GenerationPool : public CollectedMemoryPool {
private:
  Generation* _gen;
public:
  GenerationPool(Generation* gen, const char* name, PoolType type, bool support_usage_threshold);

  MemoryUsage get_memory_usage();
  size_t used_in_bytes()                { return _gen->used(); }
};

// CodeHeapPool对应于CodeCache管理的内存
class CodeHeapPool: public MemoryPool {
private:
  CodeHeap* _codeHeap;
public:
  CodeHeapPool(CodeHeap* codeHeap, const char* name, bool support_usage_threshold);
  MemoryUsage get_memory_usage();
  size_t used_in_bytes()            { return _codeHeap->allocated_capacity(); }
};

// MetaspacePool对应保存元数据的内存
// 在universe_post_init方法中会调用MemoryService::add_metaspace_memory_pools方法给Metaspace创建一个对应的MemeryPool，
// 但是该方法并没有像MemoryService::add_code_heap_memory_pool(CodeHeap* heap)一样把表示Metaspace的实例作为入参传给方法，
// 那MetaspacePool是如何获取Metaspace的内存使用情况了？
class MetaspacePool : public MemoryPool {
  size_t calculate_max_size() const;
 public:
  MetaspacePool();
  MemoryUsage get_memory_usage();
  size_t used_in_bytes();
};

// CompressedKlassSpacePool对应于保存压缩的Klass信息的内存
class CompressedKlassSpacePool : public MemoryPool {
 public:
  CompressedKlassSpacePool();
  MemoryUsage get_memory_usage();
  size_t used_in_bytes();
};

#endif // SHARE_VM_SERVICES_MEMORYPOOL_HPP
