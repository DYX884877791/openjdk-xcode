/*
 * Copyright (c) 1997, 2013, Oracle and/or its affiliates. All rights reserved.
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

#ifndef SHARE_VM_MEMORY_HEAP_HPP
#define SHARE_VM_MEMORY_HEAP_HPP

#include "memory/allocation.hpp"
#include "runtime/virtualspace.hpp"

// Blocks

// HeapBlock表示一个内存块，HeapBlock只有一个union联合属性
class HeapBlock VALUE_OBJ_CLASS_SPEC {
  friend class VMStructs;

 public:
  struct Header {
      // _length属性表示这个内存块的大小
    size_t  _length;                             // the length in segments
      // _used属性表示这个内存块是否被占用了
    bool    _used;                               // Used bit
  };

 protected:
  union {
      // 通过_padding属性来保证_header属性对应的内存大小必须是sizeof(int64_t)的整数倍，即按照sizeof(int64_t)对齐，64位下sizeof(int64_t)等于8。Header是一个结构体
    Header _header;
    int64_t _padding[ (sizeof(Header) + sizeof(int64_t)-1) / sizeof(int64_t) ];
                        // pad to 0 mod 8
  };

 public:
    // HeapBlock定义的方法都是操作Header的两个属性的
  // Initialization
  void initialize(size_t length)                 { _header._length = length; set_used(); }

  // Accessors
  // allocated_space方法返回这个内存块可用于分配其他对象的内存地址，this+1即表示HeapBlock本身的内存区域的下一个字节的地址
  void* allocated_space() const                  { return (void*)(this + 1); }
  size_t length() const                          { return _header._length; }

  // Used/free
  void set_used()                                { _header._used = true; }
  void set_free()                                { _header._used = false; }
  bool free()                                    { return !_header._used; }
};

// FreeBlock继承自HeapBlock，添加了一个_link属性
class FreeBlock: public HeapBlock {
  friend class VMStructs;
 protected:
  FreeBlock* _link;

 public:
    // FreeBlock添加的方法都是读写_link属性的，可以通过此属性将所有空闲的HeapBlock组成一个链表，便于管理。
  // Initialization
  void initialize(size_t length)             { HeapBlock::initialize(length); _link= NULL; }

  // Merging
  void set_length(size_t l)                  { _header._length = l; }

  // Accessors
  FreeBlock* link() const                    { return _link; }
  void set_link(FreeBlock* link)             { _link = link; }
};

// CodeHeap就是实际管理汇编代码内存分配的实现，其定义在hotspot/src/share/vm/memory/heap.hpp中
class CodeHeap : public CHeapObj<mtCode> {
  friend class VMStructs;
 private:
    // 用于描述CodeHeap对应的一段连续的内存空间
  VirtualSpace _memory;                          // the memory holding the blocks
    // 用于保存所有的segment的起始地址，记录这些segment的使用情况，通过mark_segmap_as_free方法标记为未分配给Block，通过mark_segmap_as_used方法标记为已分配给Block
  VirtualSpace _segmap;                          // the memory holding the segment map

    // 已分配内存的segments的数量
  size_t       _number_of_committed_segments;
    // 剩余的未分配内存的保留的segments的数量
  size_t       _number_of_reserved_segments;
    // 一个segment的大小，一个segment可以理解为一个内存页，是操作系统分配内存的最小粒度，为了避免内存碎片，任意一个Block的大小都必须是segment的整数倍，即任意一个Block会对应N个segment。
  size_t       _segment_size;
    // segment的大小取log2，用于计算根据内存地址计算所属的segment的序号
  int          _log2_segment_size;

    // 下一待分配给Block的segment的序号
  size_t       _next_segment;

    // 可用的HeapBlock 链表，所有的Block按照地址依次增加的顺序排序，即_freelist是内存地址最小的一个Block
  FreeBlock*   _freelist;
    // 可用的segments的个数
  size_t       _freelist_segments;               // No. of segments in freelist

  // Helper functions
  size_t   size_to_segments(size_t size) const { return (size + _segment_size - 1) >> _log2_segment_size; }
  size_t   segments_to_size(size_t number_of_segments) const { return number_of_segments << _log2_segment_size; }

    //根据地址计算所属的segment的序号
  size_t   segment_for(void* p) const            { return ((char*)p - _memory.low()) >> _log2_segment_size; }
  HeapBlock* block_at(size_t i) const            { return (HeapBlock*)(_memory.low() + (i << _log2_segment_size)); }

  void  mark_segmap_as_free(size_t beg, size_t end);
  void  mark_segmap_as_used(size_t beg, size_t end);

  // Freelist management helpers
  FreeBlock* following_block(FreeBlock *b);
  void insert_after(FreeBlock* a, FreeBlock* b);
  void merge_right (FreeBlock* a);

  // Toplevel freelist management
  void add_to_freelist(HeapBlock *b);
  FreeBlock* search_freelist(size_t length, bool is_critical);

  // Iteration helpers
  void*      next_free(HeapBlock* b) const;
  HeapBlock* first_block() const;
  HeapBlock* next_block(HeapBlock* b) const;
  HeapBlock* block_start(void* p) const;

  // to perform additional actions on creation of executable code
  void on_code_mapping(char* base, size_t size);

 public:
  CodeHeap();

  /**
   * CodeHeap定义的public方法主要有以下几类：
`  *
   * 内存初始化，扩展，缩小和释放的，如reserve，release，expand_by，shrink_by等
   * CodeBlob内存分配和销毁的，如allocate，deallocate
   * 获取CodeHeap的内存使用情况的，如low_boundary，high，capacity，allocated_capacity等
   * CodeBlob遍历相关的，如first，next
   */
  // Heap extents
  // reserve方法用于CodeHeap的初始化，CodeCache初始化时调用此方法
  bool  reserve(size_t reserved_size, size_t committed_size, size_t segment_size);
  void  release();                               // releases all allocated memory
  // expand_by方法用于扩展CodeHeap的内存，CodeHeap的内存不足即分配CodeBlob失败时调用
  bool  expand_by(size_t size);                  // expands commited memory by size
  void  shrink_by(size_t size);                  // shrinks commited memory by size
  void  clear();                                 // clears all heap contents

  // Memory allocation
  void* allocate  (size_t size, bool is_critical);  // allocates a block of size or returns NULL
  void  deallocate(void* p);                     // deallocates a block

  // Attributes
  char* low_boundary() const                     { return _memory.low_boundary (); }
  char* high() const                             { return _memory.high(); }
  char* high_boundary() const                    { return _memory.high_boundary(); }

    //是否在地址范围内
  bool  contains(const void* p) const            { return low_boundary() <= p && p < high(); }
  void* find_start(void* p) const;              // returns the block containing p or NULL
  size_t alignment_unit() const;                // alignment of any block
  size_t alignment_offset() const;              // offset of first byte of any block, within the enclosing alignment unit
  static size_t header_size();                  // returns the header size for each heap block

  // Iteration

  // returns the first block or NULL
  void* first() const       { return next_free(first_block()); }
  // returns the next block given a block p or NULL
  void* next(void* p) const { return next_free(next_block(block_start(p))); }

  // Statistics
  size_t capacity() const;
  size_t max_capacity() const;
  size_t allocated_capacity() const;
  size_t unallocated_capacity() const            { return max_capacity() - allocated_capacity(); }

private:
  size_t heap_unallocated_capacity() const;

public:
  // Debugging
  void verify();
  void print()  PRODUCT_RETURN;
};

#endif // SHARE_VM_MEMORY_HEAP_HPP
