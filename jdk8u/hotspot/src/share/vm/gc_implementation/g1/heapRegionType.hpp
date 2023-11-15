/*
 * Copyright (c) 2014, Oracle and/or its affiliates. All rights reserved.
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

#ifndef SHARE_VM_GC_IMPLEMENTATION_G1_HEAPREGIONTYPE_HPP
#define SHARE_VM_GC_IMPLEMENTATION_G1_HEAPREGIONTYPE_HPP

#include "memory/allocation.hpp"
#include "gc_implementation/g1/g1HeapRegionTraceType.hpp"

#define hrt_assert_is_valid(tag) \
  assert(is_valid((tag)), err_msg("invalid HR type: %u", (uint) (tag)))

// 分区（HR）是G1堆空间的最小管理单位
// G1的分区类型大概可以分为5类：
//
//  1. 自由分区 FHR
//  2. 新生代分区 YHR，细分为eden分区和survivor分区
//  3. 大对象分区 HHR，细分为大对象头分区和大对象连续分区，当对象size超过region_size一半时，即被认为是大对象
//  4. 老年代分区 OHR
//  5. 归档分区 AHR，细分为开放归档分区和关闭归档分区，区别在于是否允许引用堆外对象。
class HeapRegionType VALUE_OBJ_CLASS_SPEC {
private:
  // We encode the value of the heap region type so the generation can be
  // determined quickly. The tag is split into two parts:
  //
  //   major type (young, humongous)                         : top N-1 bits
  //   minor type (eden / survivor, starts / cont hum, etc.) : bottom 1 bit
  //
  // If there's need to increase the number of minor types in the
  // future, we'll have to increase the size of the latter and hence
  // decrease the size of the former.
  //
  // 0000 0 [ 0] Free
  //
  // 0001 0      Young Mask
  // 0001 0 [ 2] Eden
  // 0001 1 [ 3] Survivor
  //
  // 0010 0      Humongous Mask
  // 0010 0 [ 4] Humongous Starts
  // 0010 1 [ 5] Humongous Continues
  //
  // 01000 [ 8] Old
  typedef enum {
    FreeTag       = 0,

    YoungMask     = 2,
    EdenTag       = YoungMask,
    SurvTag       = YoungMask + 1,

    HumMask       = 4,
    HumStartsTag  = HumMask,
    HumContTag    = HumMask + 1,

    OldTag        = 8
  } Tag;

  volatile Tag _tag;

  static bool is_valid(Tag tag);

  Tag get() const {
    hrt_assert_is_valid(_tag);
    return _tag;
  }

  // Sets the type to 'tag'.
  void set(Tag tag) {
    hrt_assert_is_valid(tag);
    hrt_assert_is_valid(_tag);
    _tag = tag;
  }

  // Sets the type to 'tag', expecting the type to be 'before'. This
  // is available for when we want to add sanity checking to the type
  // transition.
  void set_from(Tag tag, Tag before) {
    hrt_assert_is_valid(tag);
    hrt_assert_is_valid(before);
    hrt_assert_is_valid(_tag);
    assert(_tag == before,
           err_msg("HR tag: %u, expected: %u new tag; %u", _tag, before, tag));
    _tag = tag;
  }

public:
  // Queries

  bool is_free() const { return get() == FreeTag; }

  bool is_young()    const { return (get() & YoungMask) != 0; }
  bool is_eden()     const { return get() == EdenTag;  }
  bool is_survivor() const { return get() == SurvTag;  }

  bool is_humongous()           const { return (get() & HumMask) != 0; }
  bool is_starts_humongous()    const { return get() == HumStartsTag;  }
  bool is_continues_humongous() const { return get() == HumContTag;    }

  bool is_old() const { return get() == OldTag; }

  // Setters

  void set_free() { set(FreeTag); }

  void set_eden()        { set_from(EdenTag, FreeTag); }
  void set_eden_pre_gc() { set_from(EdenTag, SurvTag); }
  void set_survivor()    { set_from(SurvTag, FreeTag); }

  void set_starts_humongous()    { set_from(HumStartsTag, FreeTag); }
  void set_continues_humongous() { set_from(HumContTag,   FreeTag); }

  void set_old() { set(OldTag); }

  // Misc

  const char* get_str() const;
  const char* get_short_str() const;
  G1HeapRegionTraceType::Type get_trace_type();

  HeapRegionType() : _tag(FreeTag) { hrt_assert_is_valid(_tag); }
};

#endif // SHARE_VM_GC_IMPLEMENTATION_G1_HEAPREGIONTYPE_HPP
