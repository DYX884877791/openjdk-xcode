/*
 * Copyright (c) 1997, 2010, Oracle and/or its affiliates. All rights reserved.
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

#ifndef SHARE_VM_GC_IMPLEMENTATION_SHARED_AGETABLE_HPP
#define SHARE_VM_GC_IMPLEMENTATION_SHARED_AGETABLE_HPP

#include "oops/markOop.hpp"
#include "oops/oop.hpp"
#include "gc_implementation/shared/gcTrace.hpp"
#include "runtime/perfData.hpp"

/* Copyright (c) 1992-2009 Oracle and/or its affiliates, and Stanford University.
   See the LICENSE file for license information. */

// Age table for adaptive feedback-mediated tenuring (scavenging)
//
// Note: all sizes are in oops

// ageTable的定义在hotspot\src\share\vm\gc_implementation\shared\ageTable.hpp中，用来记录不同分代年龄的对象的大小，然后据此动态调整tenuring_threshold，重要属性只有一个
class ageTable VALUE_OBJ_CLASS_SPEC {
  friend class VMStructs;

 public:
  // constants
  enum { table_size = markOopDesc::max_age + 1 };

  // instance variables
  // sizes就是保存不同分代年龄的对象的大小的数组。重点关注以下方法的实现，clear方法用于将sizes数组中各元素的值置为0，add方法用于增加某个分代年龄下的对象大小，
  // compute_tenuring_threshold方法用于计算新的tenuring_threshold，保证to区中已使用空间占总空间的比例满足TargetSurvivorRatio的要求
  size_t sizes[table_size];

  // constructor.  "global" indicates that this is the global age table
  // (as opposed to gc-thread-local)
  ageTable(bool global = true);

  // clear table
  void clear();

  // add entry
  void add(oop p, size_t oop_size) {
    add(p->age(), oop_size);
  }

  void add(uint age, size_t oop_size) {
    assert(age > 0 && age < table_size, "invalid age of object");
    sizes[age] += oop_size;
  }

  // Merge another age table with the current one.  Used
  // for parallel young generation gc.
  void merge(ageTable* subTable);
  void merge_par(ageTable* subTable);

  // calculate new tenuring threshold based on age information
  uint compute_tenuring_threshold(size_t survivor_capacity, GCTracer &tracer);

 private:
  PerfVariable* _perf_sizes[table_size];
};

#endif // SHARE_VM_GC_IMPLEMENTATION_SHARED_AGETABLE_HPP
