/*
 * Copyright (c) 2003, 2019, Oracle and/or its affiliates. All rights reserved.
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

#ifndef SHARE_VM_SERVICES_MEMORYMANAGER_HPP
#define SHARE_VM_SERVICES_MEMORYMANAGER_HPP

#include "memory/allocation.hpp"
#include "runtime/timer.hpp"
#include "services/memoryUsage.hpp"

// A memory manager is responsible for managing one or more memory pools.
// The garbage collector is one type of memory managers responsible
// for reclaiming memory occupied by unreachable objects.  A Java virtual
// machine may have one or more memory managers.   It may
// add or remove memory managers during execution.
// A memory pool can be managed by more than one memory managers.

class MemoryPool;
class GCMemoryManager;
class OopClosure;

// MemoryManager的定义位于hotspot/src/shared/vm/service/memoryManager.hpp中，MemoryManager用来管理一个或者多个MemoryPool，
// 垃圾回收器就是典型的MemoryManager。JVM有多个MemoryManager，一个MemoryPool可以被一个或者多个MemoryManager管理。
class MemoryManager : public CHeapObj<mtInternal> {
protected:
  enum {
    max_num_pools = 10
  };

private:
    // max_num_pools是一个枚举值，值是10
  MemoryPool* _pools[max_num_pools];
  int         _num_pools;

protected:
    // _memory_mgr_obj表示一个与之对应的Java对象
  volatile instanceOop _memory_mgr_obj;

public:
  enum Name {
    Abstract,
    CodeCache,
    Metaspace,
    Copy,
    MarkSweepCompact,
    ParNew,
    ConcurrentMarkSweep,
    PSScavenge,
    PSMarkSweep,
    G1YoungGen,
    G1OldGen
  };

  MemoryManager();

  int num_memory_pools() const           { return _num_pools; }
  MemoryPool* get_memory_pool(int index) {
    assert(index >= 0 && index < _num_pools, "Invalid index");
    return _pools[index];
  }

  int add_pool(MemoryPool* pool);

  bool is_manager(instanceHandle mh)     { return mh() == _memory_mgr_obj; }

  virtual instanceOop get_memory_manager_instance(TRAPS);
  // 为了能够方便区分MemoryManager的类型，MemoryManager定义了一个kind方法，该方法返回一个枚举值MemoryManager::Name来表示MemoryManager的类型
  virtual MemoryManager::Name kind()     { return MemoryManager::Abstract; }
  virtual bool is_gc_memory_manager()    { return false; }
  virtual const char* name() = 0;

  // GC support
  void oops_do(OopClosure* f);

  // Static factory methods to get a memory manager of a specific type
  static MemoryManager*   get_code_cache_memory_manager();
  static MemoryManager*   get_metaspace_memory_manager();
  static GCMemoryManager* get_copy_memory_manager();
  static GCMemoryManager* get_msc_memory_manager();
  static GCMemoryManager* get_parnew_memory_manager();
  static GCMemoryManager* get_cms_memory_manager();
  static GCMemoryManager* get_psScavenge_memory_manager();
  static GCMemoryManager* get_psMarkSweep_memory_manager();
  static GCMemoryManager* get_g1YoungGen_memory_manager();
  static GCMemoryManager* get_g1OldGen_memory_manager();

};

class CodeCacheMemoryManager : public MemoryManager {
private:
public:
  CodeCacheMemoryManager() : MemoryManager() {}

  MemoryManager::Name kind() { return MemoryManager::CodeCache; }
  const char* name()         { return "CodeCacheManager"; }
};

class MetaspaceMemoryManager : public MemoryManager {
public:
  MetaspaceMemoryManager() : MemoryManager() {}

  MemoryManager::Name kind() { return MemoryManager::Metaspace; }
  const char *name()         { return "Metaspace Manager"; }
};

class GCStatInfo : public ResourceObj {
private:
    //当前第几次GC
  size_t _index;
    //gc开始时间
  jlong  _start_time;
    //gc结束时间
  jlong  _end_time;

  // We keep memory usage of all memory pools
    //是一个数组，通过index属性来读写数组的元素，每个元素对应一个MemoryPool，表示该Pool执行GC前的内存使用情况
  MemoryUsage* _before_gc_usage_array;
    //同上，表示index处的MemoryPool在GC完成后的内存使用情况
  MemoryUsage* _after_gc_usage_array;
    //表示MemoryPool数组的长度，等于GCMemoryManager管理的MemoryPool的个数
  int          _usage_array_size;

  void set_gc_usage(int pool_index, MemoryUsage, bool before_gc);

public:
  GCStatInfo(int num_pools);
  ~GCStatInfo();

  size_t gc_index()               { return _index; }
  jlong  start_time()             { return _start_time; }
  jlong  end_time()               { return _end_time; }
  int    usage_array_size()       { return _usage_array_size; }
  MemoryUsage before_gc_usage_for_pool(int pool_index) {
    assert(pool_index >= 0 && pool_index < _usage_array_size, "Range checking");
    return _before_gc_usage_array[pool_index];
  }
  MemoryUsage after_gc_usage_for_pool(int pool_index) {
    assert(pool_index >= 0 && pool_index < _usage_array_size, "Range checking");
    return _after_gc_usage_array[pool_index];
  }

  MemoryUsage* before_gc_usage_array() { return _before_gc_usage_array; }
  MemoryUsage* after_gc_usage_array()  { return _after_gc_usage_array; }

  void set_index(size_t index)    { _index = index; }
  void set_start_time(jlong time) { _start_time = time; }
  void set_end_time(jlong time)   { _end_time = time; }
  void set_before_gc_usage(int pool_index, MemoryUsage usage) {
    assert(pool_index >= 0 && pool_index < _usage_array_size, "Range checking");
    set_gc_usage(pool_index, usage, true /* before gc */);
  }
  void set_after_gc_usage(int pool_index, MemoryUsage usage) {
    assert(pool_index >= 0 && pool_index < _usage_array_size, "Range checking");
    set_gc_usage(pool_index, usage, false /* after gc */);
  }

  void clear();
};

// GCMemoryManager在 MemoryManager的基础上增加了部分跟GC相关的字段
class GCMemoryManager : public MemoryManager {
private:
  // TODO: We should unify the GCCounter and GCMemoryManager statistic
    //执行GC的总次数
  size_t       _num_collections;
    //累积时间
  elapsedTimer _accumulated_timer;
    // 当前GC的时间
  elapsedTimer _gc_timer;         // for measuring every GC duration
    //上一次GC的统计数据
  GCStatInfo*  _last_gc_stat;
    //上一次GC的锁
  Mutex*       _last_gc_lock;
    //当前GC的统计数据
  GCStatInfo*  _current_gc_stat;
    //GC的线程数
  int          _num_gc_threads;
    //是否允许通知GC的结果
  volatile bool _notification_enabled;
    //标识管理的MemoryPool是否在GC完成后更新MemoryUsage
  bool         _pool_always_affected_by_gc[MemoryManager::max_num_pools];

public:
  GCMemoryManager();
  ~GCMemoryManager();

  void add_pool(MemoryPool* pool);
  void add_pool(MemoryPool* pool, bool always_affected_by_gc);

  bool pool_always_affected_by_gc(int index) {
    assert(index >= 0 && index < num_memory_pools(), "Invalid index");
    return _pool_always_affected_by_gc[index];
  }

  void   initialize_gc_stat_info();

  bool   is_gc_memory_manager()         { return true; }
  jlong  gc_time_ms()                   { return _accumulated_timer.milliseconds(); }
  size_t gc_count()                     { return _num_collections; }
  int    num_gc_threads()               { return _num_gc_threads; }
  void   set_num_gc_threads(int count)  { _num_gc_threads = count; }

  void   gc_begin(bool recordGCBeginTime, bool recordPreGCUsage,
                  bool recordAccumulatedGCTime);
  void   gc_end(bool recordPostGCUsage, bool recordAccumulatedGCTime,
                bool recordGCEndTime, bool countCollection, GCCause::Cause cause,
                bool allMemoryPoolsAffected);

  void        reset_gc_stat()   { _num_collections = 0; _accumulated_timer.reset(); }

  // Copy out _last_gc_stat to the given destination, returning
  // the collection count. Zero signifies no gc has taken place.
  size_t get_last_gc_stat(GCStatInfo* dest);

  void set_notification_enabled(bool enabled) { _notification_enabled = enabled; }
  bool is_notification_enabled() { return _notification_enabled; }
  virtual MemoryManager::Name kind() = 0;
};

// These subclasses of GCMemoryManager are defined to include
// GC-specific information.
// TODO: Add GC-specific information
class CopyMemoryManager : public GCMemoryManager {
private:
public:
  CopyMemoryManager() : GCMemoryManager() {}

  MemoryManager::Name kind() { return MemoryManager::Copy; }
  const char* name()         { return "Copy"; }
};

class MSCMemoryManager : public GCMemoryManager {
private:
public:
  MSCMemoryManager() : GCMemoryManager() {}

  MemoryManager::Name kind() { return MemoryManager::MarkSweepCompact; }
  const char* name()         { return "MarkSweepCompact"; }

};

class ParNewMemoryManager : public GCMemoryManager {
private:
public:
  ParNewMemoryManager() : GCMemoryManager() {}

  MemoryManager::Name kind() { return MemoryManager::ParNew; }
  const char* name()         { return "ParNew"; }

};

class CMSMemoryManager : public GCMemoryManager {
private:
public:
  CMSMemoryManager() : GCMemoryManager() {}

  MemoryManager::Name kind() { return MemoryManager::ConcurrentMarkSweep; }
  const char* name()         { return "ConcurrentMarkSweep";}

};

class PSScavengeMemoryManager : public GCMemoryManager {
private:
public:
  PSScavengeMemoryManager() : GCMemoryManager() {}

  MemoryManager::Name kind() { return MemoryManager::PSScavenge; }
  const char* name()         { return "PS Scavenge"; }

};

class PSMarkSweepMemoryManager : public GCMemoryManager {
private:
public:
  PSMarkSweepMemoryManager() : GCMemoryManager() {}

  MemoryManager::Name kind() { return MemoryManager::PSMarkSweep; }
  const char* name()         { return "PS MarkSweep"; }
};

class G1YoungGenMemoryManager : public GCMemoryManager {
private:
public:
  G1YoungGenMemoryManager() : GCMemoryManager() {}

  MemoryManager::Name kind() { return MemoryManager::G1YoungGen; }
  const char* name()         { return "G1 Young Generation"; }
};

class G1OldGenMemoryManager : public GCMemoryManager {
private:
public:
  G1OldGenMemoryManager() : GCMemoryManager() {}

  MemoryManager::Name kind() { return MemoryManager::G1OldGen; }
  const char* name()         { return "G1 Old Generation"; }
};

#endif // SHARE_VM_SERVICES_MEMORYMANAGER_HPP
