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

#include "precompiled.hpp"
#include "memory/heap.hpp"
#include "oops/oop.inline.hpp"
#include "runtime/os.hpp"
#include "services/memTracker.hpp"

size_t CodeHeap::header_size() {
  return sizeof(HeapBlock);
}


// Implementation of Heap

CodeHeap::CodeHeap() {
  _number_of_committed_segments = 0;
  _number_of_reserved_segments  = 0;
  _segment_size                 = 0;
  _log2_segment_size            = 0;
  _next_segment                 = 0;
  _freelist                     = NULL;
  _freelist_segments            = 0;
}


void CodeHeap::mark_segmap_as_free(size_t beg, size_t end) {
  assert(0   <= beg && beg <  _number_of_committed_segments, "interval begin out of bounds");
  assert(beg <  end && end <= _number_of_committed_segments, "interval end   out of bounds");
  // setup _segmap pointers for faster indexing
  address p = (address)_segmap.low() + beg;
  address q = (address)_segmap.low() + end;
  // initialize interval
  while (p < q) *p++ = 0xFF;
}


void CodeHeap::mark_segmap_as_used(size_t beg, size_t end) {
  assert(0   <= beg && beg <  _number_of_committed_segments, "interval begin out of bounds");
  assert(beg <  end && end <= _number_of_committed_segments, "interval end   out of bounds");
  // setup _segmap pointers for faster indexing
  address p = (address)_segmap.low() + beg;
  address q = (address)_segmap.low() + end;
  // initialize interval
  int i = 0;
  while (p < q) {
    *p++ = i++;
      //0XFF表示未分配给Block，1表示已分配
    if (i == 0xFF) i = 1;
  }
}


static size_t align_to_page_size(size_t size) {
  const size_t alignment = (size_t)os::vm_page_size();
  assert(is_power_of_2(alignment), "no kidding ???");
  return (size + alignment - 1) & ~(alignment - 1);
}


void CodeHeap::on_code_mapping(char* base, size_t size) {
#ifdef LINUX
  extern void linux_wrap_code(char* base, size_t size);
  linux_wrap_code(base, size);
#endif
}


bool CodeHeap::reserve(size_t reserved_size, size_t committed_size,
                       size_t segment_size) {
    //校验参数
  assert(reserved_size >= committed_size, "reserved < committed");
  assert(segment_size >= sizeof(FreeBlock), "segment size is too small");
  assert(is_power_of_2(segment_size), "segment_size must be a power of 2");

    //属性初始化
  _segment_size      = segment_size;
  _log2_segment_size = exact_log2(segment_size);

  // Reserve and initialize space for _memory.
    //获取内存页大小
  size_t page_size = os::vm_page_size();
  if (os::can_execute_large_page_memory()) {
    page_size = os::page_size_for_region_unaligned(reserved_size, 8);
  }

  const size_t granularity = os::vm_allocation_granularity();
  const size_t r_align = MAX2(page_size, granularity);
    //内存取整
  const size_t r_size = align_size_up(reserved_size, r_align);
  const size_t c_size = align_size_up(committed_size, page_size);

  const size_t rs_align = page_size == (size_t) os::vm_page_size() ? 0 :
    MAX2(page_size, granularity);
    //ReservedCodeSpace继承自ReservedSpace，用来描述并分配一段连续的内存空间
  ReservedCodeSpace rs(r_size, rs_align, rs_align > 0);
  os::trace_page_sizes("code heap", committed_size, reserved_size, page_size,
                       rs.base(), rs.size());
    //初始化_memory属性，如果rs分配内存失败，此处会返回false
  if (!_memory.initialize(rs, c_size)) {
    return false;
  }

    //Linux的内存映射相关操作
  on_code_mapping(_memory.low(), _memory.committed_size());
    //初始化属性，size_to_segments返回committed_size是_segment_size的多少倍，即有多少个commited的segment
  _number_of_committed_segments = size_to_segments(_memory.committed_size());
  _number_of_reserved_segments  = size_to_segments(_memory.reserved_size());
  assert(_number_of_reserved_segments >= _number_of_committed_segments, "just checking");
  const size_t reserved_segments_alignment = MAX2((size_t)os::vm_page_size(), granularity);
  const size_t reserved_segments_size = align_size_up(_number_of_reserved_segments, reserved_segments_alignment);
  const size_t committed_segments_size = align_to_page_size(_number_of_committed_segments);

  // reserve space for _segmap
    // 初始化_segmap
  if (!_segmap.initialize(reserved_segments_size, committed_segments_size)) {
    return false;
  }

  MemTracker::record_virtual_memory_type((address)_segmap.low_boundary(), mtCode);

    //校验初始化的结果
  assert(_segmap.committed_size() >= (size_t) _number_of_committed_segments, "could not commit  enough space for segment map");
  assert(_segmap.reserved_size()  >= (size_t) _number_of_reserved_segments , "could not reserve enough space for segment map");
  assert(_segmap.reserved_size()  >= _segmap.committed_size()     , "just checking");

  // initialize remaining instance variables
    //将所有的segment标记为待分配
  clear();
  return true;
}


void CodeHeap::release() {
  Unimplemented();
}


bool CodeHeap::expand_by(size_t size) {
  // expand _memory space
    //判断当前已分配内存是否足够
  size_t dm = align_to_page_size(_memory.committed_size() + size) - _memory.committed_size();
    //dm大于0表示需要扩展内存
  if (dm > 0) {
    char* base = _memory.low() + _memory.committed_size();
      //申请新的内存失败，返回false
    if (!_memory.expand_by(dm)) return false;
    on_code_mapping(base, dm);
    size_t i = _number_of_committed_segments;
      //重新计算_number_of_committed_segments
    _number_of_committed_segments = size_to_segments(_memory.committed_size());
    assert(_number_of_reserved_segments == size_to_segments(_memory.reserved_size()), "number of reserved segments should not change");
    assert(_number_of_reserved_segments >= _number_of_committed_segments, "just checking");
    // expand _segmap space
    size_t ds = align_to_page_size(_number_of_committed_segments) - _segmap.committed_size();
    if (ds > 0) {
        //扩展_segmap
      if (!_segmap.expand_by(ds)) return false;
    }
    assert(_segmap.committed_size() >= (size_t) _number_of_committed_segments, "just checking");
    // initialize additional segmap entries
      //i是原来分配的segment，将i以后的即新分配的segment初始化
    mark_segmap_as_free(i, _number_of_committed_segments);
  }
  return true;
}


void CodeHeap::shrink_by(size_t size) {
  Unimplemented();
}


void CodeHeap::clear() {
  _next_segment = 0;
    //将所有的segment标记为待分配，如果已经分配给某个Block则认为其已分配
  mark_segmap_as_free(0, _number_of_committed_segments);
}


// allocate方法用于从CodeHeap中分配指定内存大小的一块内存
void* CodeHeap::allocate(size_t instance_size, bool is_critical) {
    //计算需要占用多少个segment
  size_t number_of_segments = size_to_segments(instance_size + sizeof(HeapBlock));
    //判断number_of_segments对应的内存是否满足FreeBlock
  assert(segments_to_size(number_of_segments) >= sizeof(FreeBlock), "not enough room for FreeList");

  // First check if we can satify request from freelist
  debug_only(verify());
  HeapBlock* block = search_freelist(number_of_segments, is_critical);
  debug_only(if (VerifyCodeCacheOften) verify());
    //如果有可用的空闲Block
  if (block != NULL) {
      //CodeCacheMinBlockLength表示一个block最小的segment的个数，注意不是字节数，x86 C2下默认值是4
      //校验block的合法性，后面一个条件必须是true，是因为如果为false，block就会被截短成length个segment，从而保证这个条件成立
    assert(block->length() >= number_of_segments && block->length() < number_of_segments + CodeCacheMinBlockLength, "sanity check");
      //block必须被标记成已用
    assert(!block->free(), "must be marked free");
#ifdef ASSERT
    memset((void *)block->allocated_space(), badCodeHeapNewVal, instance_size);
#endif
      //返回该block的除去Header的可用空间的起始地址
    return block->allocated_space();
  }

  // Ensure minimum size for allocation to the heap.
    //如果没有可用的空闲Block
  if (number_of_segments < CodeCacheMinBlockLength) {
      //保证分配block的最低值
    number_of_segments = CodeCacheMinBlockLength;
  }

  if (!is_critical) {
    // Make sure the allocation fits in the unallocated heap without using
    // the CodeCacheMimimumFreeSpace that is reserved for critical allocations.
      //校验剩余可分配空间是否充足
    if (segments_to_size(number_of_segments) > (heap_unallocated_capacity() - CodeCacheMinimumFreeSpace)) {
      // Fail allocation
        //空间不足，返回NULL
      return NULL;
    }
  }

    //如果可用的segment充足，_next_segment表示下一个可用的segment
  if (_next_segment + number_of_segments <= _number_of_committed_segments) {
      //将_next_segment后面的多个segment标记为已分配
    mark_segmap_as_used(_next_segment, _next_segment + number_of_segments);
      //根据segment的序号计算对应的block的起始地址，并初始化HeapBlock
    HeapBlock* b =  block_at(_next_segment);
    b->initialize(number_of_segments);
      //重置_next_segment
    _next_segment += number_of_segments;
#ifdef ASSERT
    memset((void *)b->allocated_space(), badCodeHeapNewVal, instance_size);
#endif
      //返回该block的除去Header的可用空间的起始地址
    return b->allocated_space();
  } else {
      //可用的segment充足不足，返回NULL，这时调用方需要调用expand_by方法扩展CodeHeap的内存空间，增加_number_of_committed_segments
    return NULL;
  }
}


// deallocate用于将一个HeapBlock标记为可用，并将其添加到freelist中
void CodeHeap::deallocate(void* p) {
    //如果p不等于find_start方法的返回值，说明p是不合法的，不是allocate方法的返回值
  assert(p == find_start(p), "illegal deallocation");
  // Find start of HeapBlock
    //获取该p对应的HeapBlock的地址
  HeapBlock* b = (((HeapBlock *)p) - 1);
    //合法校验
  assert(b->allocated_space() == p, "sanity check");
#ifdef ASSERT
  memset((void *)b->allocated_space(),
         badCodeHeapFreeVal,
         segments_to_size(b->length()) - sizeof(HeapBlock));
#endif
    //将目标block添加到freelist中
  add_to_freelist(b);

  debug_only(if (VerifyCodeCacheOften) verify());
}


void* CodeHeap::find_start(void* p) const {
    //如果p对应的内存地址不再CodeHeap的内存地址范围内
  if (!contains(p)) {
    return NULL;
  }
    //获取p所属的segment的序号
  size_t i = segment_for(p);
  address b = (address)_segmap.low();
    //如果该segment被标记为未使用，如果是已使用的block则为1
  if (b[i] == 0xFF) {
    return NULL;
  }
    //往前遍历segment，直到该segment被标记为未使用，0xFF表示未使用，是一个负数
    //即找到这个segment所属的Block的起始segment
  while (b[i] > 0) i -= (int)b[i];
    //获取这个segment对应的Block的地址
  HeapBlock* h = block_at(i);
  if (h->free()) {
    return NULL;
  }
    //返回该Block可用于分配内存的起始地址
  return h->allocated_space();
}


size_t CodeHeap::alignment_unit() const {
  // this will be a power of two
  return _segment_size;
}


size_t CodeHeap::alignment_offset() const {
  // The lowest address in any allocated block will be
  // equal to alignment_offset (mod alignment_unit).
  return sizeof(HeapBlock) & (_segment_size - 1);
}

// Finds the next free heapblock. If the current one is free, that it returned
void* CodeHeap::next_free(HeapBlock *b) const {
  // Since free blocks are merged, there is max. on free block
  // between two used ones
  if (b != NULL && b->free()) b = next_block(b);
  assert(b == NULL || !b->free(), "must be in use or at end of heap");
  return (b == NULL) ? NULL : b->allocated_space();
}

// Returns the first used HeapBlock
HeapBlock* CodeHeap::first_block() const {
  if (_next_segment > 0)
    return block_at(0);
  return NULL;
}

HeapBlock *CodeHeap::block_start(void *q) const {
  HeapBlock* b = (HeapBlock*)find_start(q);
  if (b == NULL) return NULL;
  return b - 1;
}

// Returns the next Heap block an offset into one
HeapBlock* CodeHeap::next_block(HeapBlock *b) const {
  if (b == NULL) return NULL;
  size_t i = segment_for(b) + b->length();
  if (i < _next_segment)
    return block_at(i);
  return NULL;
}


// Returns current capacity
size_t CodeHeap::capacity() const {
  return _memory.committed_size();
}

size_t CodeHeap::max_capacity() const {
  return _memory.reserved_size();
}

size_t CodeHeap::allocated_capacity() const {
  // size of used heap - size on freelist
  return segments_to_size(_next_segment - _freelist_segments);
}

// Returns size of the unallocated heap block
size_t CodeHeap::heap_unallocated_capacity() const {
  // Total number of segments - number currently used
  return segments_to_size(_number_of_reserved_segments - _next_segment);
}

// Free list management

//根据b的起始地址和内存大小计算下一个block的起始地址
FreeBlock *CodeHeap::following_block(FreeBlock *b) {
  return (FreeBlock*)(((address)b) + _segment_size * b->length());
}

// Inserts block b after a
void CodeHeap::insert_after(FreeBlock* a, FreeBlock* b) {
  assert(a != NULL && b != NULL, "must be real pointers");

  // Link b into the list after a
    //将b插入到a的后面
  b->set_link(a->link());
  a->set_link(b);

  // See if we can merge blocks
    //尝试合并block
  merge_right(b); // Try to make b bigger
  merge_right(a); // Try to make a include b
}

// Try to merge this block with the following block
void CodeHeap::merge_right(FreeBlock *a) {
    //a必须是空闲的
  assert(a->free(), "must be a free block");
    //如果a的下一个block刚好也在freelist中，即存在两个连续的空闲block
  if (following_block(a) == a->link()) {
    assert(a->link() != NULL && a->link()->free(), "must be free too");
    // Update block a to include the following block
      //将这两个连续的空闲block合并成一个
    a->set_length(a->length() + a->link()->length());
    a->set_link(a->link()->link());
    // Update find_start map
    size_t beg = segment_for(a);
      //将两个block对应的segment标记为已分配
    mark_segmap_as_used(beg, beg + a->length());
  }
}

void CodeHeap::add_to_freelist(HeapBlock *a) {
    //将其强转成FreeBlock
  FreeBlock* b = (FreeBlock*)a;
  assert(b != _freelist, "cannot be removed twice");

  // Mark as free and update free space count
    //更新_freelist_segments
  _freelist_segments += b->length();
    //b标记为free
  b->set_free();

  // First element in list?
    //_freelist为空，即a是第一个空闲的Block
  if (_freelist == NULL) {
    _freelist = b;
    b->set_link(NULL);
    return;
  }

  // Scan for right place to put into list. List
  // is sorted by increasing addresseses
  FreeBlock* prev = NULL;
  FreeBlock* cur  = _freelist;
    //找到一个合适位置将a放入链表中，从_freelist开始遍历，内存地址是递增的，_freelist是内存地址最小的一个block
    //找到第一个地址小于b的block，pre是地址大于b的最后一个block
  while(cur != NULL && cur < b) {
    assert(prev == NULL || prev < cur, "must be ordered");
    prev = cur;
    cur  = cur->link();
  }

    //pre为null，即b是地址最小的，必须小于_freelist
  assert( (prev == NULL && b < _freelist) ||
          (prev < b && (cur == NULL || b < cur)), "list must be ordered");

  if (prev == NULL) {
    // Insert first in list
      //插入到链表头部
    b->set_link(_freelist);
    _freelist = b;
      //按需合并_freelist及其后面的一个block
    merge_right(_freelist);
  } else {
      //将b插入到a的后面
    insert_after(prev, b);
  }
}

// Search freelist for an entry on the list with the best fit
// Return NULL if no one was found
//注意这里的入参length并不是内存大小的字节数，而是转换后的满足指定内存大小的segment的个数
FreeBlock* CodeHeap::search_freelist(size_t length, bool is_critical) {
  FreeBlock *best_block = NULL;
  FreeBlock *best_prev  = NULL;
  size_t best_length = 0;

  // Search for smallest block which is bigger than length
    // 查找大于指定大小的最小 block
  FreeBlock *prev = NULL;
  FreeBlock *cur = _freelist;
  while(cur != NULL) {
    size_t l = cur->length();
    if (l >= length && (best_block == NULL || best_length > l)) {

      // Non critical allocations are not allowed to use the last part of the code heap.
      if (!is_critical) {
        // Make sure the end of the allocation doesn't cross into the last part of the code heap
          //确保可用空间充足，cur表示block的起始地址，将其强转成size_t，加上待分配空间的大小length，可计算待分配空间的终止地址
          //CodeCacheMinimumFreeSpace表示最低空闲空间，默认值是500k
        if (((size_t)cur + length) > ((size_t)high_boundary() - CodeCacheMinimumFreeSpace)) {
          // the freelist is sorted by address - if one fails, all consecutive will also fail.
            //因为freelist按照地址的顺序排序，所以有一个的剩余可用空间不足，则后面的block都空间不足
          break;
        }
      }

      // Remember best block, its previous element, and its length
        //记录符合要求的block，该block的前一个block，该block的长度
      best_block = cur;
      best_prev  = prev;
      best_length = best_block->length();
    }

    // Next element in list
      //遍历下一个block
    prev = cur;
    cur  = cur->link();
  }

    //best_block为空，即没有找到符合要求的block
  if (best_block == NULL) {
    // None found
    return NULL;
  }

    //校验查找的best_block是否合法
  assert((best_prev == NULL && _freelist == best_block ) ||
         (best_prev != NULL && best_prev->link() == best_block), "sanity check");

  // Exact (or at least good enough) fit. Remove from list.
  // Don't leave anything on the freelist smaller than CodeCacheMinBlockLength.
    //如果best_block在分配length的内存后的剩余可用空间不足CodeCacheMinBlockLength则将其从_freelist中移除
  if (best_length < length + CodeCacheMinBlockLength) {
    length = best_length;
      //如果没有前一个元素，说明best_block就是_freelist
    if (best_prev == NULL) {
      assert(_freelist == best_block, "sanity check");
        //将下一个元素置为链表头
      _freelist = _freelist->link();
    } else {
      // Unmap element
        //将best_block的前一个元素和后一个元素建立连接
      best_prev->set_link(best_block->link());
    }
  } else {
    // Truncate block and return a pointer to the following block
      //将best_block截短到除去length的剩余空间
    best_block->set_length(best_length - length);
      //获取当前block的下一个block，该block的长度就是length
    best_block = following_block(best_block);
    // Set used bit and length on new block
      //找到best_block对应的segment的序号
    size_t beg = segment_for(best_block);
      //将后面的segment都标记为已用
    mark_segmap_as_used(beg, beg + length);
      //设置长度
    best_block->set_length(length);
  }

    //标记为已用
  best_block->set_used();
  _freelist_segments -= length;
  return best_block;
}

//----------------------------------------------------------------------------
// Non-product code

#ifndef PRODUCT

void CodeHeap::print() {
  tty->print_cr("The Heap");
}

#endif

void CodeHeap::verify() {
  // Count the number of blocks on the freelist, and the amount of space
  // represented.
  int count = 0;
  size_t len = 0;
  for(FreeBlock* b = _freelist; b != NULL; b = b->link()) {
    len += b->length();
    count++;
  }

  // Verify that freelist contains the right amount of free space
  //  guarantee(len == _freelist_segments, "wrong freelist");

  // Verify that the number of free blocks is not out of hand.
  static int free_block_threshold = 10000;
  if (count > free_block_threshold) {
    warning("CodeHeap: # of free blocks > %d", free_block_threshold);
    // Double the warning limit
    free_block_threshold *= 2;
  }

  // Verify that the freelist contains the same number of free blocks that is
  // found on the full list.
  for(HeapBlock *h = first_block(); h != NULL; h = next_block(h)) {
    if (h->free()) count--;
  }
  //  guarantee(count == 0, "missing free blocks");
}
