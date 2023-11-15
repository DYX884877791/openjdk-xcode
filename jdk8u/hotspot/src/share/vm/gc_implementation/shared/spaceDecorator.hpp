/*
 * Copyright (c) 2002, 2012, Oracle and/or its affiliates. All rights reserved.
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

#ifndef SHARE_VM_GC_IMPLEMENTATION_SHARED_SPACEDECORATOR_HPP
#define SHARE_VM_GC_IMPLEMENTATION_SHARED_SPACEDECORATOR_HPP

#include "gc_implementation/shared/mutableSpace.hpp"
#include "memory/space.hpp"
#include "utilities/globalDefinitions.hpp"

class SpaceDecorator: public AllStatic {
 public:
  // Initialization flags.
  static const bool Clear               = true;
  static const bool DontClear           = false;
  static const bool Mangle              = true;
  static const bool DontMangle          = false;
};

// Functionality for use with class Space and class MutableSpace.
//   The approach taken with the mangling is to mangle all
// the space initially and then to mangle areas that have
// been allocated since the last collection.  Mangling is
// done in the context of a generation and in the context
// of a space.
//   The space in a generation is mangled when it is first
// initialized and when the generation grows.  The spaces
// are not necessarily up-to-date when this mangling occurs
// and the method mangle_region() is used.
//   After allocations have been done in a space, the space generally
// need to be remangled.  Remangling is only done on the
// recently allocated regions in the space.  Typically, that is
// the region between the new top and the top just before a
// garbage collection.
//   An exception to the usual mangling in a space is done when the
// space is used for an extraordinary purpose.  Specifically, when
// to-space is used as scratch space for a mark-sweep-compact
// collection.
//   Spaces are mangled after a collection.  If the generation
// grows after a collection, the added space is mangled as part of
// the growth of the generation.  No additional mangling is needed when the
// spaces are resized after an expansion.
//   The class SpaceMangler keeps a pointer to the top of the allocated
// area and provides the methods for doing the piece meal mangling.
// Methods for doing sparces and full checking of the mangling are
// included.  The full checking is done if DEBUG_MANGLING is defined.
//   GenSpaceMangler is used with the GenCollectedHeap collectors and
// MutableSpaceMangler is used with the ParallelScavengeHeap collectors.
// These subclasses abstract the differences in the types of spaces used
// by each heap.

class SpaceMangler: public CHeapObj<mtGC> {
  friend class VMStructs;

  // High water mark for allocations.  Typically, the space above
  // this point have been mangle previously and don't need to be
  // touched again.  Space belows this point has been allocated
  // and remangling is needed between the current top and this
  // high water mark.
  // _top_for_allocations记录了Space中已分配出去的内存区域，在此之前的内存区域已经被分配了，之后的区域未被分配，因为之前Space初始化的时候已经执行过mangle了，所以不需要再次mangle了。
  HeapWord* _top_for_allocations;
  HeapWord* top_for_allocations() { return _top_for_allocations; }

 public:

  // Setting _top_for_allocations to NULL at initialization
  // makes it always below top so that mangling done as part
  // of the initialize() call of a space does nothing (as it
  // should since the mangling is done as part of the constructor
  // for the space.
  SpaceMangler() : _top_for_allocations(NULL) {}

  // Methods for top and end that delegate to the specific
  // space type.
  virtual HeapWord* top() const = 0;
  virtual HeapWord* end() const = 0;

  // Return true if q matches the mangled pattern.
  static bool is_mangled(HeapWord* q) PRODUCT_RETURN0;

  // Used to save the an address in a space for later use during mangling.
  void set_top_for_allocations(HeapWord* v);

  // Overwrites the unused portion of this space.
  // Mangle only the region not previously mangled [top, top_previously_mangled)
  void mangle_unused_area();
  // Mangle all the unused region [top, end)
  void mangle_unused_area_complete();
  // Do some sparse checking on the area that should have been mangled.
  void check_mangled_unused_area(HeapWord* limit) PRODUCT_RETURN;
  // Do a complete check of the area that should be mangled.
  void check_mangled_unused_area_complete() PRODUCT_RETURN;

  // Mangle the MemRegion.  This is a non-space specific mangler.  It
  // is used during the initial mangling of a space before the space
  // is fully constructed.  Also is used when a generation is expanded
  // and possibly before the spaces have been reshaped to to the new
  // size of the generation.
  static void mangle_region(MemRegion mr) PRODUCT_RETURN;
};

class ContiguousSpace;

// For use with GenCollectedHeap's
// GenSpaceMangler的定义在hotspot\src\share\vm\gc_implementation\shared\spaceDecorator.hpp中，
// 是GenCollectedHeap用来给Space执行mangle动作的，所谓mangle实际就是把未使用的内存区域填充为一个特定的值，
// 通过这种方式强制操作系统提前分配好对应内存区域的内存页，提升Space本身内存分配和对象初始化的效率，
// 避免Space实际分配内存时或者已分配给对象的内存在实际赋值时才由操作系统分配内存页。
//
// 该类继承自SpaceMangler，SpaceMangler的定义也在spaceDecorator.hpp中
// GenSpaceMangler在SpaceMangler的基础上只增加了一个属性，没有新增方法，只提供了父类的虚方法的实现。
class GenSpaceMangler: public SpaceMangler {
  ContiguousSpace* _sp;

  ContiguousSpace* sp() { return _sp; }

  HeapWord* top() const { return _sp->top(); }
  HeapWord* end() const { return _sp->end(); }

 public:
  GenSpaceMangler(ContiguousSpace* sp) : SpaceMangler(), _sp(sp) {}
};

// For use with ParallelScavengeHeap's.
// MutableSpaceMangler是适用于PS算法的ParallelScavengeHeap的Space
class MutableSpaceMangler: public SpaceMangler {
  MutableSpace* _sp;

  MutableSpace* sp() { return _sp; }

  HeapWord* top() const { return _sp->top(); }
  HeapWord* end() const { return _sp->end(); }

 public:
  MutableSpaceMangler(MutableSpace* sp) : SpaceMangler(), _sp(sp) {}
};

#endif // SHARE_VM_GC_IMPLEMENTATION_SHARED_SPACEDECORATOR_HPP
