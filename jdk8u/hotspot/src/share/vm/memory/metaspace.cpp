/*
 * Copyright (c) 2011, 2019, Oracle and/or its affiliates. All rights reserved.
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
#include "gc_interface/collectedHeap.hpp"
#include "memory/allocation.hpp"
#include "memory/binaryTreeDictionary.hpp"
#include "memory/freeList.hpp"
#include "memory/collectorPolicy.hpp"
#include "memory/filemap.hpp"
#include "memory/freeList.hpp"
#include "memory/gcLocker.hpp"
#include "memory/metachunk.hpp"
#include "memory/metaspace.hpp"
#include "memory/metaspaceGCThresholdUpdater.hpp"
#include "memory/metaspaceShared.hpp"
#include "memory/metaspaceTracer.hpp"
#include "memory/resourceArea.hpp"
#include "memory/universe.hpp"
#include "runtime/atomic.inline.hpp"
#include "runtime/globals.hpp"
#include "runtime/init.hpp"
#include "runtime/java.hpp"
#include "runtime/mutex.hpp"
#include "runtime/orderAccess.inline.hpp"
#include "services/memTracker.hpp"
#include "services/memoryService.hpp"
#include "utilities/copy.hpp"
#include "utilities/debug.hpp"

PRAGMA_FORMAT_MUTE_WARNINGS_FOR_GCC

typedef BinaryTreeDictionary<Metablock, FreeList<Metablock> > BlockTreeDictionary;
// ChunkTreeDictionary也是一个别名，一个支持查找和排序的二叉树模板类的别名。
typedef BinaryTreeDictionary<Metachunk, FreeList<Metachunk> > ChunkTreeDictionary;

// Set this constant to enable slow integrity checking of the free chunk lists
const bool metaspace_slow_verify = false;

size_t const allocation_from_dictionary_limit = 4 * K;

MetaWord* last_allocated = 0;

size_t Metaspace::_compressed_class_space_size;
const MetaspaceTracer* Metaspace::_tracer = NULL;

// Used in declarations in SpaceManager and ChunkManager
enum ChunkIndex {
  ZeroIndex = 0,
  SpecializedIndex = ZeroIndex,
  SmallIndex = SpecializedIndex + 1,
  MediumIndex = SmallIndex + 1,
  HumongousIndex = MediumIndex + 1,
  NumberOfFreeLists = 3,
  NumberOfInUseLists = 4
};

enum ChunkSizes {    // in words.
  ClassSpecializedChunk = 128,
  SpecializedChunk = 128,
  ClassSmallChunk = 256,
  SmallChunk = 512,
  ClassMediumChunk = 4 * K,
  MediumChunk = 8 * K
};

static ChunkIndex next_chunk_index(ChunkIndex i) {
  assert(i < NumberOfInUseLists, "Out of bound");
  return (ChunkIndex) (i+1);
}

volatile intptr_t MetaspaceGC::_capacity_until_GC = 0;
uint MetaspaceGC::_shrink_factor = 0;
bool MetaspaceGC::_should_concurrent_collect = false;

// ChunkList是表示一个链表的模板类FreeList的别名，每个ChunkList对应一个固定大小的Metachunk列表
typedef class FreeList<Metachunk> ChunkList;

// Manages the global free lists of chunks.
// ChunkManager用来管理全局的所有空闲Metachunk，与之类似的BlockFreelist用来管理所有的空闲Metablock。ChunkManager的定义位于metaspace.cpp中
class ChunkManager : public CHeapObj<mtInternal> {
  friend class TestVirtualSpaceNodeTest;

  // Free list of chunks of different sizes.
  //   SpecializedChunk
  //   SmallChunk
  //   MediumChunk
  //   HumongousChunk
    // _free_chunks是一个长度为3的ChunkList数组
  ChunkList _free_chunks[NumberOfFreeLists];

  //   HumongousChunk
    // _humongous_dictionary表示大小跟上面这三种不一致的特殊的Metachunk链表，参考ChunkManager::list_index方法的实现
  ChunkTreeDictionary _humongous_dictionary;

  // ChunkManager in all lists of this type
  size_t _free_chunks_total;
  size_t _free_chunks_count;

  void dec_free_chunks_total(size_t v) {
    assert(_free_chunks_count > 0 &&
             _free_chunks_total > 0,
             "About to go negative");
      //总个数减1
    Atomic::add_ptr(-1, &_free_chunks_count);
    jlong minus_v = (jlong) - (jlong) v;
      //总内存空间减少v
    Atomic::add_ptr(minus_v, &_free_chunks_total);
  }

  // Debug support

  size_t sum_free_chunks();
  size_t sum_free_chunks_count();

  void locked_verify_free_chunks_total();
  void slow_locked_verify_free_chunks_total() {
    if (metaspace_slow_verify) {
      locked_verify_free_chunks_total();
    }
  }
  void locked_verify_free_chunks_count();
  void slow_locked_verify_free_chunks_count() {
    if (metaspace_slow_verify) {
      locked_verify_free_chunks_count();
    }
  }
  void verify_free_chunks_count();

 public:

    // 构造函数中，传入的specialized_size，small_size，medium_size实际是一个枚举值，参考Metaspace::global_initialize()中ChunkManager的初始化
  ChunkManager(size_t specialized_size, size_t small_size, size_t medium_size)
      : _free_chunks_total(0), _free_chunks_count(0) {
        // 上面提到过chunk块大小有多种，各不一样，这里分别对元空间涉及的3种chunk块设置成_free_chunks数组中的元素，_free_chunks数组指向的是FreeList类型的链表，用于链接所包含的所有chunk块，这里的size是指FreeList链表中每个chunk的大小，千万不要误认为是FreeList链表的总大小
    _free_chunks[SpecializedIndex].set_size(specialized_size);
    _free_chunks[SmallIndex].set_size(small_size);
    _free_chunks[MediumIndex].set_size(medium_size);
  }

  // add or delete (return) a chunk to the global freelist.
  Metachunk* chunk_freelist_allocate(size_t word_size);

  // Map a size to a list index assuming that there are lists
  // for special, small, medium, and humongous chunks.
  ChunkIndex list_index(size_t size);

  // Remove the chunk from its freelist.  It is
  // expected to be on one of the _free_chunks[] lists.
  void remove_chunk(Metachunk* chunk);

  // Add the simple linked list of chunks to the freelist of chunks
  // of type index.
  void return_chunks(ChunkIndex index, Metachunk* chunks);

  // Total of the space in the free chunks list
  size_t free_chunks_total_words();
  size_t free_chunks_total_bytes();

  // Number of chunks in the free chunks list
  size_t free_chunks_count();

  void inc_free_chunks_total(size_t v, size_t count = 1) {
    Atomic::add_ptr(count, &_free_chunks_count);
    Atomic::add_ptr(v, &_free_chunks_total);
  }
  ChunkTreeDictionary* humongous_dictionary() {
    return &_humongous_dictionary;
  }

  ChunkList* free_chunks(ChunkIndex index);

  // Returns the list for the given chunk word size.
  ChunkList* find_free_chunks_list(size_t word_size);

  // Remove from a list by size.  Selects list based on size of chunk.
  Metachunk* free_chunks_get(size_t chunk_word_size);

#define index_bounds_check(index)                                         \
  assert(index == SpecializedIndex ||                                     \
         index == SmallIndex ||                                           \
         index == MediumIndex ||                                          \
         index == HumongousIndex, err_msg("Bad index: %d", (int) index))

  size_t num_free_chunks(ChunkIndex index) const {
    index_bounds_check(index);

    if (index == HumongousIndex) {
      return _humongous_dictionary.total_free_blocks();
    }

    ssize_t count = _free_chunks[index].count();
    return count == -1 ? 0 : (size_t) count;
  }

  size_t size_free_chunks_in_bytes(ChunkIndex index) const {
    index_bounds_check(index);

    size_t word_size = 0;
    if (index == HumongousIndex) {
      word_size = _humongous_dictionary.total_size();
    } else {
      const size_t size_per_chunk_in_words = _free_chunks[index].size();
      word_size = size_per_chunk_in_words * num_free_chunks(index);
    }

    return word_size * BytesPerWord;
  }

  MetaspaceChunkFreeListSummary chunk_free_list_summary() const {
    return MetaspaceChunkFreeListSummary(num_free_chunks(SpecializedIndex),
                                         num_free_chunks(SmallIndex),
                                         num_free_chunks(MediumIndex),
                                         num_free_chunks(HumongousIndex),
                                         size_free_chunks_in_bytes(SpecializedIndex),
                                         size_free_chunks_in_bytes(SmallIndex),
                                         size_free_chunks_in_bytes(MediumIndex),
                                         size_free_chunks_in_bytes(HumongousIndex));
  }

  // Debug support
  void verify();
  void slow_verify() {
    if (metaspace_slow_verify) {
      verify();
    }
  }
  void locked_verify();
  void slow_locked_verify() {
      //metaspace_slow_verify默认为false，dubug模式下为true
    if (metaspace_slow_verify) {
      locked_verify();
    }
  }
  void verify_free_chunks_total();

  void locked_print_free_chunks(outputStream* st);
  void locked_print_sum_free_chunks(outputStream* st);

  void print_on(outputStream* st) const;
};

// Used to manage the free list of Metablocks (a block corresponds
// to the allocation of a quantum of metadata).
// BlockFreelist用来管理空闲的Metablock，其定义在metaspace.cpp中。
// 所有空闲的Metablock都被添加到支持按照空闲空间大小排序和查找的二叉树BlockTreeDictionary中，BlockTreeDictionary实际是模板类BinaryTreeDictionary的别名
class BlockFreelist VALUE_OBJ_CLASS_SPEC {
    // _dictionary会在第一次调用return_block归还空闲Metablock时初始化
  BlockTreeDictionary* _dictionary;

  // Only allocate and split from freelist if the size of the allocation
  // is at least 1/4th the size of the available block.
    // WasteMultiplier是调用get_block获取满足指定大小的空闲Metablock时使用，要求查找到的空闲Metablock的大小不能超过目标大小的WasteMultiplier倍。
  const static int WasteMultiplier = 4;

  // Accessors
  BlockTreeDictionary* dictionary() const { return _dictionary; }

 public:
  BlockFreelist();
  ~BlockFreelist();

  // Get and return a block to the free list
  MetaWord* get_block(size_t word_size);
  void return_block(MetaWord* p, size_t word_size);

  size_t total_size() {
  if (dictionary() == NULL) {
    return 0;
  } else {
    return dictionary()->total_size();
  }
}

  void print_on(outputStream* st) const;
};

// A VirtualSpaceList node.
// VirtualSpaceNode是VirtualSpaceList的一个节点，用来表示一大段连续的内存空间，一个VirtualSpaceNode对应一个单独的ReservedSpace和VirtualSpace。
class VirtualSpaceNode : public CHeapObj<mtClass> {
  friend class VirtualSpaceList;

  // Link to next VirtualSpaceNode
    // _next属性表示链表上下一个VirtualSpaceNode
  VirtualSpaceNode* _next;

  // total in the VirtualSpace
    // _reserved表示保留的未向操作系统申请内存的一块区域
  MemRegion _reserved;
    // rs和virtual_space负责维护这段连续的内存空间
  ReservedSpace _rs;
  VirtualSpace _virtual_space;
    // _top表示未被分配的内存起始地址
  MetaWord* _top;
  // count of chunks contained in this VirtualSpace
    // _container_count表示这个VirtualSpaceNode包含的非空闲Metachunk的个数，获取Metachunk时_container_count增加，归还Metachunk到ChunkManager时减少
  uintx _container_count;

  // VirtualSpaceNode定义的方法大部分是获取这段连续内存空闲的属性的相关方法，如bottom，end，reserved_words等，实际是对VirtualSpace方法的包装:

  // Convenience functions to access the _virtual_space
  char* low()  const { return virtual_space()->low(); }
  char* high() const { return virtual_space()->high(); }

  // The first Metachunk will be allocated at the bottom of the
  // VirtualSpace
  Metachunk* first_chunk() { return (Metachunk*) bottom(); }

  // Committed but unused space in the virtual space
  size_t free_words_in_vs() const;
 public:

  VirtualSpaceNode(size_t byte_size);
  VirtualSpaceNode(ReservedSpace rs) : _top(NULL), _next(NULL), _rs(rs), _container_count(0) {}
  ~VirtualSpaceNode();

  // Convenience functions for logical bottom and end
  MetaWord* bottom() const { return (MetaWord*) _virtual_space.low(); }
  MetaWord* end() const { return (MetaWord*) _virtual_space.high(); }

  bool contains(const void* ptr) { return ptr >= low() && ptr < high(); }

  size_t reserved_words() const  { return _virtual_space.reserved_size() / BytesPerWord; }
  size_t committed_words() const { return _virtual_space.actual_committed_size() / BytesPerWord; }

  bool is_pre_committed() const { return _virtual_space.special(); }

  // address of next available space in _virtual_space;
  // Accessors
  VirtualSpaceNode* next() { return _next; }
  void set_next(VirtualSpaceNode* v) { _next = v; }

  void set_reserved(MemRegion const v) { _reserved = v; }
  void set_top(MetaWord* v) { _top = v; }

  // Accessors
  MemRegion* reserved() { return &_reserved; }
  VirtualSpace* virtual_space() const { return (VirtualSpace*) &_virtual_space; }

  // Returns true if "word_size" is available in the VirtualSpace
  bool is_available(size_t word_size) { return word_size <= pointer_delta(end(), _top, sizeof(MetaWord)); }

  MetaWord* top() const { return _top; }
  void inc_top(size_t word_size) { _top += word_size; }

  uintx container_count() { return _container_count; }
  void inc_container_count();
  void dec_container_count();
#ifdef ASSERT
  uint container_count_slow();
  void verify_container_count();
#endif

  // used and capacity in this single entry in the list
  size_t used_words_in_vs() const;
  size_t capacity_words_in_vs() const;

  bool initialize();

  // get space from the virtual space
  Metachunk* take_from_committed(size_t chunk_word_size);

  // Allocate a chunk from the virtual space and return it.
  Metachunk* get_chunk_vs(size_t chunk_word_size);

  // Expands/shrinks the committed space in a virtual space.  Delegates
  // to Virtualspace
  bool expand_by(size_t min_words, size_t preferred_words);

  // In preparation for deleting this node, remove all the chunks
  // in the node from any freelist.
  void purge(ChunkManager* chunk_manager);

  // If an allocation doesn't fit in the current node a new node is created.
  // Allocate chunks out of the remaining committed space in this node
  // to avoid wasting that memory.
  // This always adds up because all the chunk sizes are multiples of
  // the smallest chunk size.
  void retire(ChunkManager* chunk_manager);

#ifdef ASSERT
  // Debug support
  void mangle();
#endif

  void print_on(outputStream* st) const;
};

#define assert_is_ptr_aligned(ptr, alignment) \
  assert(is_ptr_aligned(ptr, alignment),      \
    err_msg(PTR_FORMAT " is not aligned to "  \
      SIZE_FORMAT, ptr, alignment))

#define assert_is_size_aligned(size, alignment) \
  assert(is_size_aligned(size, alignment),      \
    err_msg(SIZE_FORMAT " is not aligned to "   \
       SIZE_FORMAT, size, alignment))


// Decide if large pages should be committed when the memory is reserved.
static bool should_commit_large_pages_when_reserving(size_t bytes) {
  if (UseLargePages && UseLargePagesInMetaspace && !os::can_commit_large_page_memory()) {
    size_t words = bytes / BytesPerWord;
    bool is_class = false; // We never reserve large pages for the class space.
      //如果当前Metaspace的剩余容量允许扩展指定大小
    if (MetaspaceGC::can_expand(words, is_class) &&
        MetaspaceGC::allowed_expansion() >= words) {
      return true;
    }
  }

  return false;
}

  // byte_size is the size of the associated virtualspace.
  // 构造方法有两个版本，负责初始化属性rs，与之对应的析构函数负责释放rs对应的内存地址空间。
VirtualSpaceNode::VirtualSpaceNode(size_t bytes) : _top(NULL), _next(NULL), _rs(), _container_count(0) {
  assert_is_size_aligned(bytes, Metaspace::reserve_alignment());

#if INCLUDE_CDS
  // This allocates memory with mmap.  For DumpSharedspaces, try to reserve
  // configurable address, generally at the top of the Java heap so other
  // memory addresses don't conflict.
      //DumpSharedSpaces表示将加载的类Dump到一个文件中给其他的JVM使用，默认为false，如果为true则申请一段连续的内存时需要
      //从Java堆空间的顶部申请，避免地址冲突
  if (DumpSharedSpaces) {
    bool large_pages = false; // No large pages when dumping the CDS archive.
      //SharedBaseAddress表示共享内存区域的基地址，64位下是32G，即从32G往后尝试申请一段连续的内存空间
    char* shared_base = (char*)align_ptr_up((char*)SharedBaseAddress, Metaspace::reserve_alignment());

    _rs = ReservedSpace(bytes, Metaspace::reserve_alignment(), large_pages, shared_base, 0);
    if (_rs.is_reserved()) {
        //分配成功
      assert(shared_base == 0 || _rs.base() == shared_base, "should match");
    } else {
      // Get a mmap region anywhere if the SharedBaseAddress fails.
        //在SharedBaseAddress上分配失败，则重试，不指定起始分配地址
      _rs = ReservedSpace(bytes, Metaspace::reserve_alignment(), large_pages);
    }
    MetaspaceShared::set_shared_rs(&_rs);
  } else
#endif
  {
      //判断是否使用内存页
    bool large_pages = should_commit_large_pages_when_reserving(bytes);

      // 直接看这一行，这里创建一个ReservedSpace对象句柄，同时创建指定大小的内存区域，里面用的是mmap映射方式，总之，这里才是真正给元空间mmap映射了一块内存区域
    _rs = ReservedSpace(bytes, Metaspace::reserve_alignment(), large_pages);
  }

      //如果申请成功
  if (_rs.is_reserved()) {
      //校验分配的地址空间是否符合要求
    assert(_rs.base() != NULL, "Catch if we get a NULL address");
    assert(_rs.size() != 0, "Catch if we get a 0 size");
    assert_is_ptr_aligned(_rs.base(), Metaspace::reserve_alignment());
    assert_is_size_aligned(_rs.size(), Metaspace::reserve_alignment());

      //记录日志
    MemTracker::record_virtual_memory_type((address)_rs.base(), mtClass);
  }
}

//  purge方法用于将此VirtualSpaceNode保存的所有的Metachunk从ChunkManager管理的Metachunk freeList中移除，删除此VirtualSpaceNode时调用。
void VirtualSpaceNode::purge(ChunkManager* chunk_manager) {
  Metachunk* chunk = first_chunk();
  Metachunk* invalid_chunk = (Metachunk*) top();
    //按照起始内存地址遍历所有的Chunk，因为他们是地址连续的
  while (chunk < invalid_chunk ) {
    assert(chunk->is_tagged_free(), "Should be tagged free");
    MetaWord* next = ((MetaWord*)chunk) + chunk->word_size();
      //移除目标Metachunk
    chunk_manager->remove_chunk(chunk);
      //校验移除是否正常完成
    assert(chunk->next() == NULL &&
           chunk->prev() == NULL,
           "Was not removed from its list");
    chunk = (Metachunk*) next;
  }
}

#ifdef ASSERT
uint VirtualSpaceNode::container_count_slow() {
  uint count = 0;
  Metachunk* chunk = first_chunk();
  Metachunk* invalid_chunk = (Metachunk*) top();
    //根据内存地址遍历所有的Metachunk，统计Metachunk的个数
  while (chunk < invalid_chunk ) {
    MetaWord* next = ((MetaWord*)chunk) + chunk->word_size();
    // Don't count the chunks on the free lists.  Those are
    // still part of the VirtualSpaceNode but not currently
    // counted.
    if (!chunk->is_tagged_free()) {
      count++;
    }
    chunk = (Metachunk*) next;
  }
  return count;
}
#endif

// List of VirtualSpaces for metadata allocation.
//   VirtualSpaceList在hotspot/src/share/vm/memory/metaspace.cpp中，表示一个VirtualSpaceNode链表，负责创建和维护所有的VirtualSpaceNode。
class VirtualSpaceList : public CHeapObj<mtClass> {
  friend class VirtualSpaceNode;

  enum VirtualSpaceSizes {
    VirtualSpaceSize = 256 * K
  };

  // Head of the list
  VirtualSpaceNode* _virtual_space_list;
  // virtual space currently being used for allocations
  VirtualSpaceNode* _current_virtual_space;

  // Is this VirtualSpaceList used for the compressed class space
  bool _is_class;

  // Sum of reserved and committed memory in the virtual spaces
  size_t _reserved_words;
  size_t _committed_words;

  // Number of virtual spaces
  size_t _virtual_space_count;

  ~VirtualSpaceList();

  VirtualSpaceNode* virtual_space_list() const { return _virtual_space_list; }

  void set_virtual_space_list(VirtualSpaceNode* v) {
    _virtual_space_list = v;
  }
  void set_current_virtual_space(VirtualSpaceNode* v) {
    _current_virtual_space = v;
  }

  void link_vs(VirtualSpaceNode* new_entry);

  // Get another virtual space and add it to the list.  This
  // is typically prompted by a failed attempt to allocate a chunk
  // and is typically followed by the allocation of a chunk.
  bool create_new_virtual_space(size_t vs_word_size);

  // Chunk up the unused committed space in the current
  // virtual space and add the chunks to the free list.
  void retire_current_virtual_space();

 public:
  VirtualSpaceList(size_t word_size);
  VirtualSpaceList(ReservedSpace rs);

  size_t free_bytes();

  Metachunk* get_new_chunk(size_t chunk_word_size,
                           size_t suggested_commit_granularity);

  bool expand_node_by(VirtualSpaceNode* node,
                      size_t min_words,
                      size_t preferred_words);

  bool expand_by(size_t min_words,
                 size_t preferred_words);

  VirtualSpaceNode* current_virtual_space() {
    return _current_virtual_space;
  }

  bool is_class() const { return _is_class; }

  bool initialization_succeeded() { return _virtual_space_list != NULL; }

  size_t reserved_words()  { return _reserved_words; }
  size_t reserved_bytes()  { return reserved_words() * BytesPerWord; }
  size_t committed_words() { return _committed_words; }
  size_t committed_bytes() { return committed_words() * BytesPerWord; }

  void inc_reserved_words(size_t v);
  void dec_reserved_words(size_t v);
  void inc_committed_words(size_t v);
  void dec_committed_words(size_t v);
  void inc_virtual_space_count();
  void dec_virtual_space_count();

  bool contains(const void* ptr);

  // Unlink empty VirtualSpaceNodes and free it.
  void purge(ChunkManager* chunk_manager);

  void print_on(outputStream* st) const;

  class VirtualSpaceListIterator : public StackObj {
    VirtualSpaceNode* _virtual_spaces;
   public:
    VirtualSpaceListIterator(VirtualSpaceNode* virtual_spaces) :
      _virtual_spaces(virtual_spaces) {}

    bool repeat() {
      return _virtual_spaces != NULL;
    }

    VirtualSpaceNode* get_next() {
      VirtualSpaceNode* result = _virtual_spaces;
      if (_virtual_spaces != NULL) {
        _virtual_spaces = _virtual_spaces->next();
      }
      return result;
    }
  };
};

class Metadebug : AllStatic {
  // Debugging support for Metaspaces
  static int _allocation_fail_alot_count;

 public:

  static void init_allocation_fail_alot_count();
#ifdef ASSERT
  static bool test_metadata_failure();
#endif
};

int Metadebug::_allocation_fail_alot_count = 0;

//  SpaceManager - used by Metaspace to handle allocations
// SpaceManager的定义位于metaspace.cpp中，用于给Metaspace提供内存管理接口
class SpaceManager : public CHeapObj<mtClass> {
  friend class Metaspace;
  friend class Metadebug;

 private:

  // protects allocations
    //内存分配的锁
  Mutex* const _lock;

  // Type of metadata allocated.
    //Metaspace的类型
  Metaspace::MetadataType _mdtype;

  // List of chunks in use by this SpaceManager.  Allocations
  // are done from the current chunk.  The list is used for deallocating
  // chunks when the SpaceManager is freed.
    // SpaceManager使用的Metachunk的数组，总共有4个元素，即3种标准规格，加上一个特殊规格的chunk，每个chunk通过next属性构成一个链表，
    // 链表中的chunk如果不是当前chunk都会被retire，即把剩余空间分配成MetaBlock放入BlockFreelist中。
  Metachunk* _chunks_in_use[NumberOfInUseLists];
    //SpaceManager当前使用的Metachunk
  Metachunk* _current_chunk;

  // Number of small chunks to allocate to a manager
  // If class space manager, small chunks are unlimited
    //SpaceManager所能分配的small chunks的数量上限，ClassType类型的Metaspace没有此限制
  static uint const _small_chunk_limit;

  // Sum of all space in allocated chunks
    //已分配的所有block的内存大小
  size_t _allocated_blocks_words;

  // Sum of all allocated chunks
    //已分配的chunk的内存大小
  size_t _allocated_chunks_words;
    //已分配的chunk的个数
  size_t _allocated_chunks_count;

  // Free lists of blocks are per SpaceManager since they
  // are assumed to be in chunks in use by the SpaceManager
  // and all chunks in use by a SpaceManager are freed when
  // the class loader using the SpaceManager is collected.
    //负责管理空闲block的BlockFreelist
  BlockFreelist _block_freelists;

  // protects virtualspace and chunk expansions
    //_expand_lock的name属性
  static const char*  _expand_lock_name;
    //_expand_lock的rank属性
  static const int    _expand_lock_rank;
  static Mutex* const _expand_lock;

 private:
  // Accessors
  Metachunk* chunks_in_use(ChunkIndex index) const { return _chunks_in_use[index]; }
  void set_chunks_in_use(ChunkIndex index, Metachunk* v) {
    _chunks_in_use[index] = v;
  }

  BlockFreelist* block_freelists() const {
    return (BlockFreelist*) &_block_freelists;
  }

  Metaspace::MetadataType mdtype() { return _mdtype; }

  VirtualSpaceList* vs_list()   const { return Metaspace::get_space_list(_mdtype); }
  ChunkManager* chunk_manager() const { return Metaspace::get_chunk_manager(_mdtype); }

  Metachunk* current_chunk() const { return _current_chunk; }
  void set_current_chunk(Metachunk* v) {
    _current_chunk = v;
  }

  Metachunk* find_current_chunk(size_t word_size);

  // Add chunk to the list of chunks in use
  void add_chunk(Metachunk* v, bool make_current);
  void retire_current_chunk();

  Mutex* lock() const { return _lock; }

  const char* chunk_size_name(ChunkIndex index) const;

 protected:
  void initialize();

 public:
  SpaceManager(Metaspace::MetadataType mdtype,
               Mutex* lock);
  ~SpaceManager();

  enum ChunkMultiples {
    MediumChunkMultiple = 4
  };

  static size_t specialized_chunk_size(bool is_class) { return is_class ? ClassSpecializedChunk : SpecializedChunk; }
  static size_t small_chunk_size(bool is_class)       { return is_class ? ClassSmallChunk : SmallChunk; }
  static size_t medium_chunk_size(bool is_class)      { return is_class ? ClassMediumChunk : MediumChunk; }

  static size_t smallest_chunk_size(bool is_class)    { return specialized_chunk_size(is_class); }

  // Accessors
  bool is_class() const { return _mdtype == Metaspace::ClassType; }

  size_t specialized_chunk_size() const { return specialized_chunk_size(is_class()); }
  size_t small_chunk_size()       const { return small_chunk_size(is_class()); }
  size_t medium_chunk_size()      const { return medium_chunk_size(is_class()); }

  size_t smallest_chunk_size()    const { return smallest_chunk_size(is_class()); }

  size_t medium_chunk_bunch()     const { return medium_chunk_size() * MediumChunkMultiple; }

  size_t allocated_blocks_words() const { return _allocated_blocks_words; }
  size_t allocated_blocks_bytes() const { return _allocated_blocks_words * BytesPerWord; }
  size_t allocated_chunks_words() const { return _allocated_chunks_words; }
  size_t allocated_chunks_bytes() const { return _allocated_chunks_words * BytesPerWord; }
  size_t allocated_chunks_count() const { return _allocated_chunks_count; }

  bool is_humongous(size_t word_size) { return word_size > medium_chunk_size(); }

  static Mutex* expand_lock() { return _expand_lock; }

  // Increment the per Metaspace and global running sums for Metachunks
  // by the given size.  This is used when a Metachunk to added to
  // the in-use list.
  void inc_size_metrics(size_t words);
  // Increment the per Metaspace and global running sums Metablocks by the given
  // size.  This is used when a Metablock is allocated.
  void inc_used_metrics(size_t words);
  // Delete the portion of the running sums for this SpaceManager. That is,
  // the globals running sums for the Metachunks and Metablocks are
  // decremented for all the Metachunks in-use by this SpaceManager.
  void dec_total_from_size_metrics();

  // Adjust the initial chunk size to match one of the fixed chunk list sizes,
  // or return the unadjusted size if the requested size is humongous.
  static size_t adjust_initial_chunk_size(size_t requested, bool is_class_space);
  size_t adjust_initial_chunk_size(size_t requested) const;

  // Get the initial chunks size for this metaspace type.
  size_t get_initial_chunk_size(Metaspace::MetaspaceType type) const;

  size_t sum_capacity_in_chunks_in_use() const;
  size_t sum_used_in_chunks_in_use() const;
  size_t sum_free_in_chunks_in_use() const;
  size_t sum_waste_in_chunks_in_use() const;
  size_t sum_waste_in_chunks_in_use(ChunkIndex index ) const;

  size_t sum_count_in_chunks_in_use();
  size_t sum_count_in_chunks_in_use(ChunkIndex i);

  Metachunk* get_new_chunk(size_t chunk_word_size);

  // Block allocation and deallocation.
  // Allocates a block from the current chunk
  MetaWord* allocate(size_t word_size);

  // Helper for allocations
  MetaWord* allocate_work(size_t word_size);

  // Returns a block to the per manager freelist
  void deallocate(MetaWord* p, size_t word_size);

  // Based on the allocation size and a minimum chunk size,
  // returned chunk size (for expanding space for chunk allocation).
  size_t calc_chunk_size(size_t allocation_word_size);

  // Called when an allocation from the current chunk fails.
  // Gets a new chunk (may require getting a new virtual space),
  // and allocates from that chunk.
  MetaWord* grow_and_allocate(size_t word_size);

  // Notify memory usage to MemoryService.
  void track_metaspace_memory_usage();

  // debugging support.

  void dump(outputStream* const out) const;
  void print_on(outputStream* st) const;
  void locked_print_chunks_in_use_on(outputStream* st) const;

  void verify();
  void verify_chunk_size(Metachunk* chunk);
  NOT_PRODUCT(void mangle_freed_chunks();)
#ifdef ASSERT
  void verify_allocated_blocks_words();
#endif

  size_t get_raw_word_size(size_t word_size) {
    size_t byte_size = word_size * BytesPerWord;

      //byte_size最低大于Metablock的大小
    size_t raw_bytes_size = MAX2(byte_size, sizeof(Metablock));
      //向上内存取整
    raw_bytes_size = align_size_up(raw_bytes_size, Metachunk::object_alignment());

    size_t raw_word_size = raw_bytes_size / BytesPerWord;
    assert(raw_word_size * BytesPerWord == raw_bytes_size, "Size problem");

    return raw_word_size;
  }
};

// _expand_lock和_small_chunk_limit是静态初始化的
uint const SpaceManager::_small_chunk_limit = 4;

const char* SpaceManager::_expand_lock_name =
  "SpaceManager chunk allocation lock";
const int SpaceManager::_expand_lock_rank = Monitor::leaf - 1;
Mutex* const SpaceManager::_expand_lock =
  new Mutex(SpaceManager::_expand_lock_rank,
            SpaceManager::_expand_lock_name,
            Mutex::_allow_vm_block_flag);

void VirtualSpaceNode::inc_container_count() {
  assert_lock_strong(SpaceManager::expand_lock());
    //增加计数器
  _container_count++;
  assert(_container_count == container_count_slow(),
         err_msg("Inconsistency in countainer_count _container_count " SIZE_FORMAT
                 " container_count_slow() " SIZE_FORMAT,
                 _container_count, container_count_slow()));
}

void VirtualSpaceNode::dec_container_count() {
  assert_lock_strong(SpaceManager::expand_lock());
  _container_count--;
}

#ifdef ASSERT
void VirtualSpaceNode::verify_container_count() {
  assert(_container_count == container_count_slow(),
    err_msg("Inconsistency in countainer_count _container_count " SIZE_FORMAT
            " container_count_slow() " SIZE_FORMAT, _container_count, container_count_slow()));
}
#endif

// BlockFreelist methods

BlockFreelist::BlockFreelist() : _dictionary(NULL) {}

BlockFreelist::~BlockFreelist() {
  if (_dictionary != NULL) {
    if (Verbose && TraceMetadataChunkAllocation) {
      _dictionary->print_free_lists(gclog_or_tty);
    }
    delete _dictionary;
  }
}

void BlockFreelist::return_block(MetaWord* p, size_t word_size) {
    //根据内存块的起始地址和大小构造一个新的Metablock
  Metablock* free_chunk = ::new (p) Metablock(word_size);
  if (dictionary() == NULL) {
      //初始化_dictionary
   _dictionary = new BlockTreeDictionary();
  }
    //添加到二叉树中保存
  dictionary()->return_chunk(free_chunk);
}

MetaWord* BlockFreelist::get_block(size_t word_size) {
    //_dictionary未初始化，肯定没有空闲的
  if (dictionary() == NULL) {
    return NULL;
  }

    //TreeChunk是一个模板类，BlockTreeDictionary的实现会用到
    //如果word_size太小则返回NULL
  if (word_size < TreeChunk<Metablock, FreeList<Metablock> >::min_size()) {
    // Dark matter.  Too small for dictionary.
    return NULL;
  }

    //查找大于等于目标大小的空闲Metablock
  Metablock* free_block =
    dictionary()->get_chunk(word_size, FreeBlockDictionary<Metablock>::atLeast);
  if (free_block == NULL) {
    return NULL;
  }

  const size_t block_size = free_block->size();
    //如果找到的空闲Metablock的大小大于目标大小的4倍，则将其归还，返回NULL，避免浪费
  if (block_size > WasteMultiplier * word_size) {
    return_block((MetaWord*)free_block, block_size);
    return NULL;
  }

    //如果小于目标大小的4倍
  MetaWord* new_block = (MetaWord*)free_block;
  assert(block_size >= word_size, "Incorrect size of block from freelist");
    //计算多余的空间
  const size_t unused = block_size - word_size;
  if (unused >= TreeChunk<Metablock, FreeList<Metablock> >::min_size()) {
      //将多余的空间归还
    return_block(new_block + word_size, unused);
  }

  return new_block;
}

void BlockFreelist::print_on(outputStream* st) const {
  if (dictionary() == NULL) {
    return;
  }
  dictionary()->print_free_lists(st);
}

// VirtualSpaceNode methods

VirtualSpaceNode::~VirtualSpaceNode() {
  _rs.release();
#ifdef ASSERT
  size_t word_size = sizeof(*this) / BytesPerWord;
  Copy::fill_to_words((HeapWord*) this, word_size, 0xf1f1f1f1);
#endif
}

size_t VirtualSpaceNode::used_words_in_vs() const {
  return pointer_delta(top(), bottom(), sizeof(MetaWord));
}

// Space committed in the VirtualSpace
size_t VirtualSpaceNode::capacity_words_in_vs() const {
  return pointer_delta(end(), bottom(), sizeof(MetaWord));
}

size_t VirtualSpaceNode::free_words_in_vs() const {
  return pointer_delta(end(), top(), sizeof(MetaWord));
}

// Allocates the chunk from the virtual space only.
// This interface is also used internally for debugging.  Not all
// chunks removed here are necessarily used for allocation.
Metachunk* VirtualSpaceNode::take_from_committed(size_t chunk_word_size) {
  // Bottom of the new chunk
    //获取未分配内存的起始地址
  MetaWord* chunk_limit = top();
  assert(chunk_limit != NULL, "Not safe to call this method");

  // The virtual spaces are always expanded by the
  // commit granularity to enforce the following condition.
  // Without this the is_available check will not work correctly.
    //校验_virtual_space是否按照期望的方式expand，如果为false，则下面的is_available可能返回错误的结果
  assert(_virtual_space.committed_size() == _virtual_space.actual_committed_size(),
      "The committed memory doesn't match the expanded memory.");

    //如果剩余空间不足，返回NULL
  if (!is_available(chunk_word_size)) {
      //打印日志
    if (TraceMetadataChunkAllocation) {
      gclog_or_tty->print("VirtualSpaceNode::take_from_committed() not available %d words ", chunk_word_size);
      // Dump some information about the virtual space that is nearly full
      print_on(gclog_or_tty);
    }
    return NULL;
  }

  // Take the space  (bump top on the current virtual space).
    //将top指针往高地址移动
  inc_top(chunk_word_size);

  // Initialize the chunk
    //初始化Metachunk
  Metachunk* result = ::new (chunk_limit) Metachunk(chunk_word_size, this);
  return result;
}


// Expand the virtual space (commit more of the reserved space)
bool VirtualSpaceNode::expand_by(size_t min_words, size_t preferred_words) {
  size_t min_bytes = min_words * BytesPerWord;
  size_t preferred_bytes = preferred_words * BytesPerWord;

  size_t uncommitted = virtual_space()->reserved_size() - virtual_space()->actual_committed_size();

  if (uncommitted < min_bytes) {
    return false;
  }

  size_t commit = MIN2(preferred_bytes, uncommitted);
  bool result = virtual_space()->expand_by(commit, false);

  assert(result, "Failed to commit memory");

  return result;
}

// get_chunk_vs负责从virtual_space中分配一段指定大小的内存空间
Metachunk* VirtualSpaceNode::get_chunk_vs(size_t chunk_word_size) {
    //校验已获取锁
  assert_lock_strong(SpaceManager::expand_lock());
    //从commited区域的内存分配一个Metachunk
  Metachunk* result = take_from_committed(chunk_word_size);
  if (result != NULL) {
      //分配成功，增加计数器
    inc_container_count();
  }
  return result;
}

// initialize是在构造方法执行完后根据已经初始化好的rs来初始化后virtual_space，初始化VirtualSpace的相关属性的
bool VirtualSpaceNode::initialize() {

    //_rs申请内存地址空间失败，返回false
    // 由上一步VirtualSpaceNode::VirtualSpaceNode函数映射的内存区域用ReservedSpace对象来持有，命名为_rs，该对象创建的内存is_reserved()一定是返回true，否则return false退出函数
  if (!_rs.is_reserved()) {
    return false;
  }

  // These are necessary restriction to make sure that the virtual space always
  // grows in steps of Metaspace::commit_alignment(). If both base and size are
  // aligned only the middle alignment of the VirtualSpace is used.
    //校验申请的地址空间是否合法
    // 断言验证_rs的base和size值都是对齐后的值
  assert_is_ptr_aligned(_rs.base(), Metaspace::commit_alignment());
  assert_is_size_aligned(_rs.size(), Metaspace::commit_alignment());

  // ReservedSpaces marked as special will have the entire memory
  // pre-committed. Setting a committed size will make sure that
  // committed_size and actual_committed_size agrees.
    // 由前面创建_rs得知_rs.special()返回的是false,所以这里pre_committed_size就是0
    //如果rs支持pre-committed，则设置pre_committed_size为rs的大小
  size_t pre_committed_size = _rs.special() ? _rs.size() : 0;

    //初始化virtual_space
    // 这一步也是做一初始值设置工作或赋值操作
  bool result = virtual_space()->initialize_with_granularity(_rs, pre_committed_size,
                                            Metaspace::commit_alignment());
    //申请内存成功
  if (result) {
    assert(virtual_space()->committed_size() == virtual_space()->actual_committed_size(),
        "Checking that the pre-committed memory was registered by the VirtualSpace");

      //设置其他属性
      // 设置top值，也就是内存空间的限制界线值
    set_top((MetaWord*)virtual_space()->low());
      // 设置reserved的值，其实就是上面_rs的值再通过MemRegion来包装
    set_reserved(MemRegion((HeapWord*)_rs.base(),
                 (HeapWord*)(_rs.base() + _rs.size())));

    assert(reserved()->start() == (HeapWord*) _rs.base(),
      err_msg("Reserved start was not set properly " PTR_FORMAT
        " != " PTR_FORMAT, reserved()->start(), _rs.base()));
    assert(reserved()->word_size() == _rs.size() / BytesPerWord,
      err_msg("Reserved size was not set properly " SIZE_FORMAT
        " != " SIZE_FORMAT, reserved()->word_size(),
        _rs.size() / BytesPerWord));
  }

    // 返回结果
  return result;
}

void VirtualSpaceNode::print_on(outputStream* st) const {
  size_t used = used_words_in_vs();
  size_t capacity = capacity_words_in_vs();
  VirtualSpace* vs = virtual_space();
  st->print_cr("   space @ " PTR_FORMAT " " SIZE_FORMAT "K, %3d%% used "
           "[" PTR_FORMAT ", " PTR_FORMAT ", "
           PTR_FORMAT ", " PTR_FORMAT ")",
           vs, capacity / K,
           capacity == 0 ? 0 : used * 100 / capacity,
           bottom(), top(), end(),
           vs->high_boundary());
}

#ifdef ASSERT
void VirtualSpaceNode::mangle() {
  size_t word_size = capacity_words_in_vs();
  Copy::fill_to_words((HeapWord*) low(), word_size, 0xf1f1f1f1);
}
#endif // ASSERT

// VirtualSpaceList methods
// Space allocated from the VirtualSpace

VirtualSpaceList::~VirtualSpaceList() {
    //从链表头元素开始遍历，释放所有的VirtualSpaceNode
  VirtualSpaceListIterator iter(virtual_space_list());
  while (iter.repeat()) {
    VirtualSpaceNode* vsl = iter.get_next();
    delete vsl;
  }
}

void VirtualSpaceList::inc_reserved_words(size_t v) {
  assert_lock_strong(SpaceManager::expand_lock());
  _reserved_words = _reserved_words + v;
}
void VirtualSpaceList::dec_reserved_words(size_t v) {
  assert_lock_strong(SpaceManager::expand_lock());
  _reserved_words = _reserved_words - v;
}

#define assert_committed_below_limit()                             \
  assert(MetaspaceAux::committed_bytes() <= MaxMetaspaceSize,      \
      err_msg("Too much committed memory. Committed: " SIZE_FORMAT \
              " limit (MaxMetaspaceSize): " SIZE_FORMAT,           \
          MetaspaceAux::committed_bytes(), MaxMetaspaceSize));

void VirtualSpaceList::inc_committed_words(size_t v) {
  assert_lock_strong(SpaceManager::expand_lock());
  _committed_words = _committed_words + v;

  assert_committed_below_limit();
}
void VirtualSpaceList::dec_committed_words(size_t v) {
  assert_lock_strong(SpaceManager::expand_lock());
  _committed_words = _committed_words - v;

  assert_committed_below_limit();
}

void VirtualSpaceList::inc_virtual_space_count() {
  assert_lock_strong(SpaceManager::expand_lock());
  _virtual_space_count++;
}
void VirtualSpaceList::dec_virtual_space_count() {
  assert_lock_strong(SpaceManager::expand_lock());
  _virtual_space_count--;
}

// remove_chunk是将某个Metachunk从ChunkManager中移除
void ChunkManager::remove_chunk(Metachunk* chunk) {
  size_t word_size = chunk->word_size();
    //根据大小匹配对应的空闲chunk链表，然后从链表中移除
  ChunkIndex index = list_index(word_size);
  if (index != HumongousIndex) {
    free_chunks(index)->remove_chunk(chunk);
  } else {
    humongous_dictionary()->remove_chunk(chunk);
  }

  // Chunk is being removed from the chunks free list.
    //减少计数器
  dec_free_chunks_total(chunk->word_size());
}

// Walk the list of VirtualSpaceNodes and delete
// nodes with a 0 container_count.  Remove Metachunks in
// the node from their respective freelists.
//  purge方法用于清理掉VirtualSpaceList中空闲的即没有任何Metachunk的VirtualSpaceNode节点，当VirtualSpaceList关联的ClassLoaderData被垃圾回收器清理掉了就会触发此方法
void VirtualSpaceList::purge(ChunkManager* chunk_manager) {
    //校验是否在安全点
  assert(SafepointSynchronize::is_at_safepoint(), "must be called at safepoint for contains to work");
    //校验获取锁
  assert_lock_strong(SpaceManager::expand_lock());
  // Don't use a VirtualSpaceListIterator because this
  // list is being changed and a straightforward use of an iterator is not safe.
  VirtualSpaceNode* purged_vsl = NULL;
    //链表头
  VirtualSpaceNode* prev_vsl = virtual_space_list();
  VirtualSpaceNode* next_vsl = prev_vsl;
  while (next_vsl != NULL) {
    VirtualSpaceNode* vsl = next_vsl;
    next_vsl = vsl->next();
    // Don't free the current virtual space since it will likely
    // be needed soon.
      //如果不包含任何Metachunk且不是当前节点，因为当前节点可能会被使用
    if (vsl->container_count() == 0 && vsl != current_virtual_space()) {
      // Unlink it from the list
        //从链表中移除
      if (prev_vsl == vsl) {
        // This is the case of the current node being the first node.
          //如果是头节点，将头结点的下一个节点作为头结点
        assert(vsl == virtual_space_list(), "Expected to be the first node");
        set_virtual_space_list(vsl->next());
      } else {
        prev_vsl->set_next(vsl->next());
      }

        //回收该节点
      vsl->purge(chunk_manager);
        //减少计数器
      dec_reserved_words(vsl->reserved_words());
      dec_committed_words(vsl->committed_words());
      dec_virtual_space_count();
      purged_vsl = vsl;
        //释放节点
      delete vsl;
    } else {
      prev_vsl = vsl;
    }
  }
#ifdef ASSERT
  if (purged_vsl != NULL) {
    // List should be stable enough to use an iterator here.
    VirtualSpaceListIterator iter(virtual_space_list());
    while (iter.repeat()) {
      VirtualSpaceNode* vsl = iter.get_next();
      assert(vsl != purged_vsl, "Purge of vsl failed");
    }
  }
#endif
}


// This function looks at the mmap regions in the metaspace without locking.
// The chunks are added with store ordering and not deleted except for at
// unloading time during a safepoint.
bool VirtualSpaceList::contains(const void* ptr) {
  // List should be stable enough to use an iterator here because removing virtual
  // space nodes is only allowed at a safepoint.
  VirtualSpaceListIterator iter(virtual_space_list());
  while (iter.repeat()) {
    VirtualSpaceNode* vsn = iter.get_next();
    if (vsn->contains(ptr)) {
      return true;
    }
  }
  return false;
}

void VirtualSpaceList::retire_current_virtual_space() {
  assert_lock_strong(SpaceManager::expand_lock());

  VirtualSpaceNode* vsn = current_virtual_space();

  ChunkManager* cm = is_class() ? Metaspace::chunk_manager_class() :
                                  Metaspace::chunk_manager_metadata();

    //回收当前节点
  vsn->retire(cm);
}

// retire方法是当前VirtualSpaceNode的剩余空间不足需要申请一个新的VirtualSpaceNode是调用的，retire方法会在当前VirtualSpaceNode的剩余空间内申请新的Metachunk，并将其添加到ChunkManager中，避免空间浪费。
void VirtualSpaceNode::retire(ChunkManager* chunk_manager) {
  for (int i = (int)MediumIndex; i >= (int)ZeroIndex; --i) {
    ChunkIndex index = (ChunkIndex)i;
      //获取不同规格的Metachunk的大小
    size_t chunk_size = chunk_manager->free_chunks(index)->size();

      //如果剩余空间充足
    while (free_words_in_vs() >= chunk_size) {
      DEBUG_ONLY(verify_container_count();)
        //申请一个新的Metachunk
      Metachunk* chunk = get_chunk_vs(chunk_size);
      assert(chunk != NULL, "allocation should have been successful");

        //申请成功将其交给ChunkManager
      chunk_manager->return_chunks(index, chunk);
      chunk_manager->inc_free_chunks_total(chunk_size);
      DEBUG_ONLY(verify_container_count();)
    }
  }
  assert(free_words_in_vs() == 0, "should be empty now");
}

VirtualSpaceList::VirtualSpaceList(size_t word_size) :
                                   _is_class(false),
                                   _virtual_space_list(NULL),
                                   _current_virtual_space(NULL),
                                   _reserved_words(0),
                                   _committed_words(0),
                                   _virtual_space_count(0) {
    //获取锁expand_lock
  MutexLockerEx cl(SpaceManager::expand_lock(),
                   Mutex::_no_safepoint_check_flag);
    //创建一个新的virtual_space
    // 实际做事的函数
  create_new_virtual_space(word_size);
}

VirtualSpaceList::VirtualSpaceList(ReservedSpace rs) :
                                   _is_class(true),
                                   _virtual_space_list(NULL),
                                   _current_virtual_space(NULL),
                                   _reserved_words(0),
                                   _committed_words(0),
                                   _virtual_space_count(0) {
  MutexLockerEx cl(SpaceManager::expand_lock(),
                   Mutex::_no_safepoint_check_flag);
  VirtualSpaceNode* class_entry = new VirtualSpaceNode(rs);
  bool succeeded = class_entry->initialize();
  if (succeeded) {
    link_vs(class_entry);
  }
}

size_t VirtualSpaceList::free_bytes() {
  return current_virtual_space()->free_words_in_vs() * BytesPerWord;
}

// Allocate another meta virtual space and add it to the list.
bool VirtualSpaceList::create_new_virtual_space(size_t vs_word_size) {
  assert_lock_strong(SpaceManager::expand_lock());

    //创建compressed class的VirtualSpace不会走到此分支
    // 上面初始化时，已经设置_is_class = false，所以这条逻辑不会走
  if (is_class()) {
    assert(false, "We currently don't support more than one VirtualSpace for"
                  " the compressed class space. The initialization of the"
                  " CCS uses another code path and should not hit this path.");
    return false;
  }

    // 大小不能为0
  if (vs_word_size == 0) {
    assert(false, "vs_word_size should always be at least _reserve_alignment large.");
    return false;
  }

  // Reserve the space
    // 求得对齐后，要分配的字节大小
  size_t vs_byte_size = vs_word_size * BytesPerWord;
    //内存取整
  assert_is_size_aligned(vs_byte_size, Metaspace::reserve_alignment());

  // Allocate the meta virtual space and initialize it.
    //创建一个新的节点
    // 创建并分配VirtualSpaceNode对象，看下面VirtualSpaceNode::VirtualSpaceNode函数
  VirtualSpaceNode* new_entry = new VirtualSpaceNode(vs_byte_size);
    //   创建完后，要对字段进行初始化，看下面VirtualSpaceNode::initialize函数
  if (!new_entry->initialize()) {
      //初始化失败，返回false
    delete new_entry;
    return false;
  } else {
      //初始化成功，校验结果
    assert(new_entry->reserved_words() == vs_word_size,
        "Reserved memory size differs from requested memory size");
    // ensure lock-free iteration sees fully initialized node
      //同步结果
    OrderAccess::storestore();
    link_vs(new_entry);
    return true;
  }
}

void VirtualSpaceList::link_vs(VirtualSpaceNode* new_entry) {
    //插入到链表中
  if (virtual_space_list() == NULL) {
      set_virtual_space_list(new_entry);
  } else {
    current_virtual_space()->set_next(new_entry);
  }
  set_current_virtual_space(new_entry);
    //增加计数
  inc_reserved_words(new_entry->reserved_words());
  inc_committed_words(new_entry->committed_words());
  inc_virtual_space_count();
#ifdef ASSERT
  new_entry->mangle();
#endif
  if (TraceMetavirtualspaceAllocation && Verbose) {
      //打印日志
    VirtualSpaceNode* vsl = current_virtual_space();
    vsl->print_on(gclog_or_tty);
  }
}

bool VirtualSpaceList::expand_node_by(VirtualSpaceNode* node,
                                      size_t min_words,
                                      size_t preferred_words) {
  size_t before = node->committed_words();

    //节点expand事假调用VirtualSpace::expand_by方法扩展，如果成功返回true
  bool result = node->expand_by(min_words, preferred_words);

  size_t after = node->committed_words();

  // after and before can be the same if the memory was pre-committed.
  assert(after >= before, "Inconsistency");
    //增加已提交的内存量
  inc_committed_words(after - before);

  return result;
}

bool VirtualSpaceList::expand_by(size_t min_words, size_t preferred_words) {
    //校验参数
  assert_is_size_aligned(min_words,       Metaspace::commit_alignment_words());
  assert_is_size_aligned(preferred_words, Metaspace::commit_alignment_words());
  assert(min_words <= preferred_words, "Invalid arguments");

    //MetaspaceGC根据当前已经提交的总内存量和Metaspace最大内存量判断能否扩展
  if (!MetaspaceGC::can_expand(min_words, this->is_class())) {
    return  false;
  }

  size_t allowed_expansion_words = MetaspaceGC::allowed_expansion();
  if (allowed_expansion_words < min_words) {
    return false;
  }

    //因为preferred_words和allowed_expansion_words都是大于或者等于min_words，所以取两者的最小值也能满足要求
  size_t max_expansion_words = MIN2(preferred_words, allowed_expansion_words);

  // Commit more memory from the the current virtual space.
    //尝试当前节点扩展
  bool vs_expanded = expand_node_by(current_virtual_space(),
                                    min_words,
                                    max_expansion_words);
    //扩展成功
  if (vs_expanded) {
    return true;
  }
    //节点创建时申请的reserved_size的剩余空间不足导致扩展失败，回收当前节点
  retire_current_virtual_space();

  // Get another virtual space.
    //取两者间的最大值，并做内存取整
  size_t grow_vs_words = MAX2((size_t)VirtualSpaceSize, preferred_words);
  grow_vs_words = align_size_up(grow_vs_words, Metaspace::reserve_alignment_words());

    //创建一个新的节点
  if (create_new_virtual_space(grow_vs_words)) {
      //pre_committed即创建的时候已经完成commited
    if (current_virtual_space()->is_pre_committed()) {
      // The memory was pre-committed, so we are done here.
      assert(min_words <= current_virtual_space()->committed_words(),
          "The new VirtualSpace was pre-committed, so it"
          "should be large enough to fit the alloc request.");
      return true;
    }

      //非pre_committed，需要手动commited
    return expand_node_by(current_virtual_space(),
                          min_words,
                          max_expansion_words);
  }

  return false;
}

// get_new_chunk用于获取一个新的满足大小要求的Metachunk，是VirtualSpaceList的核心方法
Metachunk* VirtualSpaceList::get_new_chunk(size_t chunk_word_size, size_t suggested_commit_granularity) {

  // Allocate a chunk out of the current virtual space.
    //从当前的VirtualSpaceNode节点分配一个Metachunk
  Metachunk* next = current_virtual_space()->get_chunk_vs(chunk_word_size);

  if (next != NULL) {
      //分配成功则返回
    return next;
  }

  // The expand amount is currently only determined by the requested sizes
  // and not how much committed memory is left in the current virtual space.

    //当前节点内存不足，需要扩展创建一个新的节点，扩展的量是根据要求分配的chunk_word_size内存大小计算的，而不是当前节点剩余的已提交内存
    //对chunk_word_size做内存取整
  size_t min_word_size       = align_size_up(chunk_word_size,              Metaspace::commit_alignment_words());
  size_t preferred_word_size = align_size_up(suggested_commit_granularity, Metaspace::commit_alignment_words());
  if (min_word_size >= preferred_word_size) {
    // Can happen when humongous chunks are allocated.
    preferred_word_size = min_word_size;
  }

    //按照min_word_size重新创建一个新的VirtualSpaceNode
  bool expanded = expand_by(min_word_size, preferred_word_size);
  if (expanded) {
      //如果创建成功，则使用新的节点创建一个Metachunk
    next = current_virtual_space()->get_chunk_vs(chunk_word_size);
    assert(next != NULL, "The allocation was expected to succeed after the expansion");
  }

   return next;
}

void VirtualSpaceList::print_on(outputStream* st) const {
  if (TraceMetadataChunkAllocation && Verbose) {
    VirtualSpaceListIterator iter(virtual_space_list());
    while (iter.repeat()) {
      VirtualSpaceNode* node = iter.get_next();
      node->print_on(st);
    }
  }
}

// MetaspaceGC methods

// VM_CollectForMetadataAllocation is the vm operation used to GC.
// Within the VM operation after the GC the attempt to allocate the metadata
// should succeed.  If the GC did not free enough space for the metaspace
// allocation, the HWM is increased so that another virtualspace will be
// allocated for the metadata.  With perm gen the increase in the perm
// gen had bounds, MinMetaspaceExpansion and MaxMetaspaceExpansion.  The
// metaspace policy uses those as the small and large steps for the HWM.
//
// After the GC the compute_new_size() for MetaspaceGC is called to
// resize the capacity of the metaspaces.  The current implementation
// is based on the flags MinMetaspaceFreeRatio and MaxMetaspaceFreeRatio used
// to resize the Java heap by some GC's.  New flags can be implemented
// if really needed.  MinMetaspaceFreeRatio is used to calculate how much
// free space is desirable in the metaspace capacity to decide how much
// to increase the HWM.  MaxMetaspaceFreeRatio is used to decide how much
// free space is desirable in the metaspace capacity before decreasing
// the HWM.

// Calculate the amount to increase the high water mark (HWM).
// Increase by a minimum amount (MinMetaspaceExpansion) so that
// another expansion is not requested too soon.  If that is not
// enough to satisfy the allocation, increase by MaxMetaspaceExpansion.
// If that is still not enough, expand by the size of the allocation
// plus some.
size_t MetaspaceGC::delta_capacity_until_GC(size_t bytes) {
  size_t min_delta = MinMetaspaceExpansion;
  size_t max_delta = MaxMetaspaceExpansion;
  size_t delta = align_size_up(bytes, Metaspace::commit_alignment());

  if (delta <= min_delta) {
    delta = min_delta;
  } else if (delta <= max_delta) {
    // Don't want to hit the high water mark on the next
    // allocation so make the delta greater than just enough
    // for this allocation.
    delta = max_delta;
  } else {
    // This allocation is large but the next ones are probably not
    // so increase by the minimum.
    delta = delta + min_delta;
  }

  assert_is_size_aligned(delta, Metaspace::commit_alignment());

  return delta;
}

size_t MetaspaceGC::capacity_until_GC() {
  size_t value = (size_t)OrderAccess::load_ptr_acquire(&_capacity_until_GC);
  assert(value >= MetaspaceSize, "Not initialied properly?");
  return value;
}

// Try to increase the _capacity_until_GC limit counter by v bytes.
// Returns true if it succeeded. It may fail if either another thread
// concurrently increased the limit or the new limit would be larger
// than MaxMetaspaceSize.
// On success, optionally returns new and old metaspace capacity in
// new_cap_until_GC and old_cap_until_GC respectively.
// On error, optionally sets can_retry to indicate whether if there is
// actually enough space remaining to satisfy the request.
// 增加capacity_until_GC属性的值
//
// 尝试把_capacity_until_GC的值增加v，如果成功则返回true，如果其他线程并发的增加这个属性或者这个增加后的值超过了MaxMetaspaceSize都会
// 失败返回false。如果成功，会可选的返回一个新的cap_until_GC的值和原来的旧的cap_until_GC的值，即设置到指针变量new_cap_until_GC和
// old_cap_until_GC中。如果失败，则会可选的设置can_retry指针变量，来表明是否存在足够的空间满足要求，如果有则调用方可以重新调用此方法
bool MetaspaceGC::inc_capacity_until_GC(size_t v, size_t* new_cap_until_GC, size_t* old_cap_until_GC, bool* can_retry) {
    //确认已经按照Metaspace内存分配粒度取整
  assert_is_size_aligned(v, Metaspace::commit_alignment());

    //原来的值
  size_t capacity_until_GC = (size_t) _capacity_until_GC;
    //新值
  size_t new_value = capacity_until_GC + v;

  if (new_value < capacity_until_GC) {
    // The addition wrapped around, set new_value to aligned max value.
      //不会走到此分支
    new_value = align_size_down(max_uintx, Metaspace::reserve_alignment());
  }

  if (new_value > MaxMetaspaceSize) {
      //大于最大值了，返回false
    if (can_retry != NULL) {
      *can_retry = false;
    }
    return false;
  }

  if (can_retry != NULL) {
    *can_retry = true;
  }

  intptr_t expected = (intptr_t) capacity_until_GC;
    //原子的修改属性
  intptr_t actual = Atomic::cmpxchg_ptr((intptr_t) new_value, &_capacity_until_GC, expected);

    //如果修改失败
  if (expected != actual) {
    return false;
  }

    //如果修改成功
  if (new_cap_until_GC != NULL) {
    *new_cap_until_GC = new_value;
  }
  if (old_cap_until_GC != NULL) {
    *old_cap_until_GC = capacity_until_GC;
  }
  return true;
}

size_t MetaspaceGC::dec_capacity_until_GC(size_t v) {
  assert_is_size_aligned(v, Metaspace::commit_alignment());

    //原子的减少属性_capacity_until_GC
  return (size_t)Atomic::add_ptr(-(intptr_t)v, &_capacity_until_GC);
}

// initialize是在universe_init方法中触发的，与post_initialize两方法都是设置_capacity_until_GC属性的值
void MetaspaceGC::initialize() {
  // Set the high-water mark to MaxMetapaceSize during VM initializaton since
  // we can't do a GC during initialization.
    // MaxMetaspaceSize表示元空间的最大值，默认是int类型的最大值
  _capacity_until_GC = MaxMetaspaceSize;
}

//  post_initialize是在初始化完成后通过Metaspace::post_initialize方法触发的
void MetaspaceGC::post_initialize() {
  // Reset the high-water mark once the VM initialization is done.
    // MetaspaceSize表示元空间的初始值，启用C2编译下默认是16M
  _capacity_until_GC = MAX2(MetaspaceAux::committed_bytes(), MetaspaceSize);
}

bool MetaspaceGC::can_expand(size_t word_size, bool is_class) {
  // Check if the compressed class space is full.
  if (is_class && Metaspace::using_class_space()) {
    size_t class_committed = MetaspaceAux::committed_bytes(Metaspace::ClassType);
    if (class_committed + word_size * BytesPerWord > CompressedClassSpaceSize) {
      return false;
    }
  }

  // Check if the user has imposed a limit on the metaspace memory.
  size_t committed_bytes = MetaspaceAux::committed_bytes();
  if (committed_bytes + word_size * BytesPerWord > MaxMetaspaceSize) {
    return false;
  }

  return true;
}

size_t MetaspaceGC::allowed_expansion() {
  size_t committed_bytes = MetaspaceAux::committed_bytes();
  size_t capacity_until_gc = capacity_until_GC();

  assert(capacity_until_gc >= committed_bytes,
        err_msg("capacity_until_gc: " SIZE_FORMAT " < committed_bytes: " SIZE_FORMAT,
                capacity_until_gc, committed_bytes));

  size_t left_until_max  = MaxMetaspaceSize - committed_bytes;
  size_t left_until_GC = capacity_until_gc - committed_bytes;
  size_t left_to_commit = MIN2(left_until_GC, left_until_max);

  return left_to_commit / BytesPerWord;
}

// compute_new_size用于在GC完成后根据设置的最低空闲比例MinMetaspaceFreeRatio和最大的空闲比例MaxMetaspaceFreeRatio计算一个新的_capacity_until_GC属性值，实现动态调整Metaspace大小的功能
void MetaspaceGC::compute_new_size() {
  assert(_shrink_factor <= 100, "invalid shrink factor");
  uint current_shrink_factor = _shrink_factor;
  _shrink_factor = 0;

  // Using committed_bytes() for used_after_gc is an overestimation, since the
  // chunk free lists are included in committed_bytes() and the memory in an
  // un-fragmented chunk free list is available for future allocations.
  // However, if the chunk free lists becomes fragmented, then the memory may
  // not be available for future allocations and the memory is therefore "in use".
  // Including the chunk free lists in the definition of "in use" is therefore
  // necessary. Not including the chunk free lists can cause capacity_until_GC to
  // shrink below committed_bytes() and this has caused serious bugs in the past.
    //committed_bytes()实际包含部分空闲的chunk块，即实际是未使用的，如果不包含他们会导致capacity_until_GC
    //缩减，过去曾因此导致了几个严重bug，所以这里依然把这些空闲的chunk块当做是使用中的内存
  const size_t used_after_gc = MetaspaceAux::committed_bytes();
  const size_t capacity_until_GC = MetaspaceGC::capacity_until_GC();

    //MinMetaspaceFreeRatio是GC完成后需要保证的Metaspace最低的空闲空间比例，默认是40，为了避免Metaspace因为内存不足再次触发GC
  const double minimum_free_percentage = MinMetaspaceFreeRatio / 100.0;
  const double maximum_used_percentage = 1.0 - minimum_free_percentage;

    //计算需要的最低内存值
  const double min_tmp = used_after_gc / maximum_used_percentage;
    //如果min_tmp大于MaxMetaspaceSize则取MaxMetaspaceSize，保证扩容后不超过最大值
  size_t minimum_desired_capacity =
    (size_t)MIN2(min_tmp, double(MaxMetaspaceSize));
  // Don't shrink less than the initial generation size
    //如果MetaspaceSize大于minimum_desired_capacity则取MetaspaceSize，保证缩容后不低于初始值
  minimum_desired_capacity = MAX2(minimum_desired_capacity,
                                  MetaspaceSize);

    //打印GC日志
  if (PrintGCDetails && Verbose) {
    gclog_or_tty->print_cr("\nMetaspaceGC::compute_new_size: ");
    gclog_or_tty->print_cr("  "
                  "  minimum_free_percentage: %6.2f"
                  "  maximum_used_percentage: %6.2f",
                  minimum_free_percentage,
                  maximum_used_percentage);
    gclog_or_tty->print_cr("  "
                  "   used_after_gc       : %6.1fKB",
                  used_after_gc / (double) K);
  }


  size_t shrink_bytes = 0;
    //如果当前的capacity_until_GC小于期望值，则扩容，增加capacity_until_GC
  if (capacity_until_GC < minimum_desired_capacity) {
    // If we have less capacity below the metaspace HWM, then
    // increment the HWM.
      //计算需要增加的值
    size_t expand_bytes = minimum_desired_capacity - capacity_until_GC;
      //取整
    expand_bytes = align_size_up(expand_bytes, Metaspace::commit_alignment());
    // Don't expand unless it's significant
      //MinMetaspaceExpansion表示扩容时最低的扩展值，默认是256k，低于此值不扩容
    if (expand_bytes >= MinMetaspaceExpansion) {
      size_t new_capacity_until_GC = 0;
        //增加capacity_until_GC
      bool succeeded = MetaspaceGC::inc_capacity_until_GC(expand_bytes, &new_capacity_until_GC);
        //在GC结束后的安全点调用此方法总是成功的
      assert(succeeded, "Should always succesfully increment HWM when at safepoint");

        //打印日志
      Metaspace::tracer()->report_gc_threshold(capacity_until_GC,
                                               new_capacity_until_GC,
                                               MetaspaceGCThresholdUpdater::ComputeNewSize);
      if (PrintGCDetails && Verbose) {
        gclog_or_tty->print_cr("    expanding:"
                      "  minimum_desired_capacity: %6.1fKB"
                      "  expand_bytes: %6.1fKB"
                      "  MinMetaspaceExpansion: %6.1fKB"
                      "  new metaspace HWM:  %6.1fKB",
                      minimum_desired_capacity / (double) K,
                      expand_bytes / (double) K,
                      MinMetaspaceExpansion / (double) K,
                      new_capacity_until_GC / (double) K);
      }
    }
    return;
  }

  // No expansion, now see if we want to shrink
  // We would never want to shrink more than this
    //如果当前的capacity_until_GC大于最低期望值，需要判断capacity_until_GC是否大于最高期望值，如果是则缩容，减少capacity_until_GC
    //计算需要缩容的空间
  size_t max_shrink_bytes = capacity_until_GC - minimum_desired_capacity;
  assert(max_shrink_bytes >= 0, err_msg("max_shrink_bytes " SIZE_FORMAT,
    max_shrink_bytes));

  // Should shrinking be considered?
    //MaxMetaspaceFreeRatio表示GC结束后Metaspace的最大空闲比例，默认是70
  if (MaxMetaspaceFreeRatio < 100) {
      //根据MaxMetaspaceFreeRatio计算允许的最大的空间值，不能低于MetaspaceSize初始值，不能大于最大值MaxMetaspaceSize
    const double maximum_free_percentage = MaxMetaspaceFreeRatio / 100.0;
    const double minimum_used_percentage = 1.0 - maximum_free_percentage;
    const double max_tmp = used_after_gc / minimum_used_percentage;
    size_t maximum_desired_capacity = (size_t)MIN2(max_tmp, double(MaxMetaspaceSize));
    maximum_desired_capacity = MAX2(maximum_desired_capacity,
                                    MetaspaceSize);
      //打印日志
    if (PrintGCDetails && Verbose) {
      gclog_or_tty->print_cr("  "
                             "  maximum_free_percentage: %6.2f"
                             "  minimum_used_percentage: %6.2f",
                             maximum_free_percentage,
                             minimum_used_percentage);
      gclog_or_tty->print_cr("  "
                             "  minimum_desired_capacity: %6.1fKB"
                             "  maximum_desired_capacity: %6.1fKB",
                             minimum_desired_capacity / (double) K,
                             maximum_desired_capacity / (double) K);
    }

      //合理校验，要求的最小内存值必须小于或者等于最大内存值
    assert(minimum_desired_capacity <= maximum_desired_capacity,
           "sanity check");

      //如果capacity_until_GC大于根据最大空闲比例计算出的允许的最大内存值，即当前的空间比例大于设置的最大比例，需要缩容
    if (capacity_until_GC > maximum_desired_capacity) {
      // Capacity too large, compute shrinking size
        //计算需要缩容的大小
      shrink_bytes = capacity_until_GC - maximum_desired_capacity;
      // We don't want shrink all the way back to initSize if people call
      // System.gc(), because some programs do that between "phases" and then
      // we'd just have to grow the heap up again for the next phase.  So we
      // damp the shrinking: 0% on the first call, 10% on the second call, 40%
      // on the third call, and 100% by the fourth call.  But if we recompute
      // size without shrinking, it goes back to 0%.
        //为了避免程序调用System.gc()触发的GC导致堆空间的再次分配，增加参数_shrink_factor，第一次GC时是0，即第一次GC时不会触发缩容
        //第二次是10，即最多只缩容10%,第三次是40%,第四次是100%
      shrink_bytes = shrink_bytes / 100 * current_shrink_factor;

        //内存取整
      shrink_bytes = align_size_down(shrink_bytes, Metaspace::commit_alignment());

      assert(shrink_bytes <= max_shrink_bytes,
        err_msg("invalid shrink size " SIZE_FORMAT " not <= " SIZE_FORMAT,
          shrink_bytes, max_shrink_bytes));
        //更新_shrink_factor
      if (current_shrink_factor == 0) {
        _shrink_factor = 10;
      } else {
        _shrink_factor = MIN2(current_shrink_factor * 4, (uint) 100);
      }
        //打印日志
      if (PrintGCDetails && Verbose) {
        gclog_or_tty->print_cr("  "
                      "  shrinking:"
                      "  initSize: %.1fK"
                      "  maximum_desired_capacity: %.1fK",
                      MetaspaceSize / (double) K,
                      maximum_desired_capacity / (double) K);
        gclog_or_tty->print_cr("  "
                      "  shrink_bytes: %.1fK"
                      "  current_shrink_factor: %d"
                      "  new shrink factor: %d"
                      "  MinMetaspaceExpansion: %.1fK",
                      shrink_bytes / (double) K,
                      current_shrink_factor,
                      _shrink_factor,
                      MinMetaspaceExpansion / (double) K);
      }
    }
  }

  // Don't shrink unless it's significant
    //如果大于最低扩容空间，且缩容后大于初始值MetaspaceSize，则缩容
  if (shrink_bytes >= MinMetaspaceExpansion &&
      ((capacity_until_GC - shrink_bytes) >= MetaspaceSize)) {
    size_t new_capacity_until_GC = MetaspaceGC::dec_capacity_until_GC(shrink_bytes);
    Metaspace::tracer()->report_gc_threshold(capacity_until_GC,
                                             new_capacity_until_GC,
                                             MetaspaceGCThresholdUpdater::ComputeNewSize);
  }
}

// Metadebug methods

void Metadebug::init_allocation_fail_alot_count() {
  if (MetadataAllocationFailALot) {
    _allocation_fail_alot_count =
      1+(long)((double)MetadataAllocationFailALotInterval*os::random()/(max_jint+1.0));
  }
}

#ifdef ASSERT
bool Metadebug::test_metadata_failure() {
  if (MetadataAllocationFailALot &&
      Threads::is_vm_complete()) {
    if (_allocation_fail_alot_count > 0) {
      _allocation_fail_alot_count--;
    } else {
      if (TraceMetadataChunkAllocation && Verbose) {
        gclog_or_tty->print_cr("Metadata allocation failing for "
                               "MetadataAllocationFailALot");
      }
      init_allocation_fail_alot_count();
      return true;
    }
  }
  return false;
}
#endif

// ChunkManager methods

size_t ChunkManager::free_chunks_total_words() {
  return _free_chunks_total;
}

size_t ChunkManager::free_chunks_total_bytes() {
  return free_chunks_total_words() * BytesPerWord;
}

size_t ChunkManager::free_chunks_count() {
#ifdef ASSERT
  if (!UseConcMarkSweepGC && !SpaceManager::expand_lock()->is_locked()) {
    MutexLockerEx cl(SpaceManager::expand_lock(),
                     Mutex::_no_safepoint_check_flag);
    // This lock is only needed in debug because the verification
    // of the _free_chunks_totals walks the list of free chunks
    slow_locked_verify_free_chunks_count();
  }
#endif
  return _free_chunks_count;
}

void ChunkManager::locked_verify_free_chunks_total() {
  assert_lock_strong(SpaceManager::expand_lock());
    //校验_free_chunks_total正确
  assert(sum_free_chunks() == _free_chunks_total,
    err_msg("_free_chunks_total " SIZE_FORMAT " is not the"
           " same as sum " SIZE_FORMAT, _free_chunks_total,
           sum_free_chunks()));
}

void ChunkManager::verify_free_chunks_total() {
  MutexLockerEx cl(SpaceManager::expand_lock(),
                     Mutex::_no_safepoint_check_flag);
  locked_verify_free_chunks_total();
}

void ChunkManager::locked_verify_free_chunks_count() {
  assert_lock_strong(SpaceManager::expand_lock());
    //校验_free_chunks_count正确
  assert(sum_free_chunks_count() == _free_chunks_count,
    err_msg("_free_chunks_count " SIZE_FORMAT " is not the"
           " same as sum " SIZE_FORMAT, _free_chunks_count,
           sum_free_chunks_count()));
}

void ChunkManager::verify_free_chunks_count() {
#ifdef ASSERT
  MutexLockerEx cl(SpaceManager::expand_lock(),
                     Mutex::_no_safepoint_check_flag);
  locked_verify_free_chunks_count();
#endif
}

void ChunkManager::verify() {
  MutexLockerEx cl(SpaceManager::expand_lock(),
                     Mutex::_no_safepoint_check_flag);
  locked_verify();
}

void ChunkManager::locked_verify() {
  locked_verify_free_chunks_count();
  locked_verify_free_chunks_total();
}

void ChunkManager::locked_print_free_chunks(outputStream* st) {
  assert_lock_strong(SpaceManager::expand_lock());
  st->print_cr("Free chunk total " SIZE_FORMAT "  count " SIZE_FORMAT,
                _free_chunks_total, _free_chunks_count);
}

void ChunkManager::locked_print_sum_free_chunks(outputStream* st) {
  assert_lock_strong(SpaceManager::expand_lock());
  st->print_cr("Sum free chunk total " SIZE_FORMAT "  count " SIZE_FORMAT,
                sum_free_chunks(), sum_free_chunks_count());
}

ChunkList* ChunkManager::free_chunks(ChunkIndex index) {
  assert(index == SpecializedIndex || index == SmallIndex || index == MediumIndex,
         err_msg("Bad index: %d", (int)index));

  return &_free_chunks[index];
}

// These methods that sum the free chunk lists are used in printing
// methods that are used in product builds.
//返回所有空闲chunk的总的内存大小
size_t ChunkManager::sum_free_chunks() {
  assert_lock_strong(SpaceManager::expand_lock());
  size_t result = 0;
    //遍历数组_free_chunks
  for (ChunkIndex i = ZeroIndex; i < NumberOfFreeLists; i = next_chunk_index(i)) {
    ChunkList* list = free_chunks(i);

    if (list == NULL) {
      continue;
    }

      //链表中每个chunk的大小都是list->size()
    result = result + list->count() * list->size();
  }
  result = result + humongous_dictionary()->total_size();
  return result;
}

// //统计总的空闲chunk数量
size_t ChunkManager::sum_free_chunks_count() {
  assert_lock_strong(SpaceManager::expand_lock());
  size_t count = 0;
    //遍历数组_free_chunks
  for (ChunkIndex i = ZeroIndex; i < NumberOfFreeLists; i = next_chunk_index(i)) {
    ChunkList* list = free_chunks(i);
    if (list == NULL) {
      continue;
    }
    count = count + list->count();
  }
  count = count + humongous_dictionary()->total_free_blocks();
  return count;
}

ChunkList* ChunkManager::find_free_chunks_list(size_t word_size) {
  ChunkIndex index = list_index(word_size);
  assert(index < HumongousIndex, "No humongous list");
  return free_chunks(index);
}

Metachunk* ChunkManager::free_chunks_get(size_t word_size) {
    //校验获取锁
  assert_lock_strong(SpaceManager::expand_lock());

    //校验计数器
  slow_locked_verify();

  Metachunk* chunk = NULL;
    //如果是通用规格的Metachunk
  if (list_index(word_size) != HumongousIndex) {
      //获取对应的链表
    ChunkList* free_list = find_free_chunks_list(word_size);
    assert(free_list != NULL, "Sanity check");

      //获取链表头元素
    chunk = free_list->head();

    if (chunk == NULL) {
      return NULL;
    }

    // Remove the chunk as the head of the list.
      //不为空，则将头元素从链表中移除
    free_list->remove_chunk(chunk);

    if (TraceMetadataChunkAllocation && Verbose) {
        //打印日志
      gclog_or_tty->print_cr("ChunkManager::free_chunks_get: free_list "
                             PTR_FORMAT " head " PTR_FORMAT " size " SIZE_FORMAT,
                             free_list, chunk, chunk->word_size());
    }
  } else {
      //如果是特殊规格，则在humongous链表中查找大于等于word_size的Metachunk
    chunk = humongous_dictionary()->get_chunk(
      word_size,
      FreeBlockDictionary<Metachunk>::atLeast);

    if (chunk == NULL) {
      return NULL;
    }

    if (TraceMetadataHumongousAllocation) {
        //查找成功，打印日志
      size_t waste = chunk->word_size() - word_size;
      gclog_or_tty->print_cr("Free list allocate humongous chunk size "
                             SIZE_FORMAT " for requested size " SIZE_FORMAT
                             " waste " SIZE_FORMAT,
                             chunk->word_size(), word_size, waste);
    }
  }

  // Chunk is being removed from the chunks free list.
    //修改计数器
  dec_free_chunks_total(chunk->word_size());

  // Remove it from the links to this freelist
    //删除对前后节点的引用
  chunk->set_next(NULL);
  chunk->set_prev(NULL);
#ifdef ASSERT
  // Chunk is no longer on any freelist. Setting to false make container_count_slow()
  // work.
  chunk->set_is_tagged_free(false);
#endif
    //增加Metachunk所属的VirtualSpaceNode的计数器
  chunk->container()->inc_container_count();

  slow_locked_verify();
  return chunk;
}

// chunk_freelist_allocate用于从空闲Metachunk链表中查找一个满足指定大小要求的Metachunk
Metachunk* ChunkManager::chunk_freelist_allocate(size_t word_size) {
    //获取锁
  assert_lock_strong(SpaceManager::expand_lock());
    //校验两个计数器的正确
  slow_locked_verify();

  // Take from the beginning of the list
    //查找满足要求的Metachunk
  Metachunk* chunk = free_chunks_get(word_size);
  if (chunk == NULL) {
    return NULL;
  }

    //查找成功，校验结果
  assert((word_size <= chunk->word_size()) ||
         (list_index(chunk->word_size()) == HumongousIndex),
         "Non-humongous variable sized chunk");
  if (TraceMetadataChunkAllocation) {
    size_t list_count;
      //判断word_size对应哪种类型的Metachunk，获取对应类型的Metachunk链表的长度
    if (list_index(word_size) < HumongousIndex) {
      ChunkList* list = find_free_chunks_list(word_size);
      list_count = list->count();
    } else {
      list_count = humongous_dictionary()->total_count();
    }
      //打印日志
    gclog_or_tty->print("ChunkManager::chunk_freelist_allocate: " PTR_FORMAT " chunk "
                        PTR_FORMAT "  size " SIZE_FORMAT " count " SIZE_FORMAT " ",
                        this, chunk, chunk->word_size(), list_count);
    locked_print_free_chunks(gclog_or_tty);
  }

  return chunk;
}

void ChunkManager::print_on(outputStream* out) const {
  if (PrintFLSStatistics != 0) {
    const_cast<ChunkManager *>(this)->humongous_dictionary()->report_statistics();
  }
}

// SpaceManager methods

size_t SpaceManager::adjust_initial_chunk_size(size_t requested, bool is_class_space) {
  size_t chunk_sizes[] = {
      specialized_chunk_size(is_class_space),
      small_chunk_size(is_class_space),
      medium_chunk_size(is_class_space)
  };

  // Adjust up to one of the fixed chunk sizes ...
    //从小到大依次遍历
  for (size_t i = 0; i < ARRAY_SIZE(chunk_sizes); i++) {
    if (requested <= chunk_sizes[i]) {
      return chunk_sizes[i];
    }
  }

  // ... or return the size as a humongous chunk.
  return requested;
}

size_t SpaceManager::adjust_initial_chunk_size(size_t requested) const {
  return adjust_initial_chunk_size(requested, is_class());
}

// get_initial_chunk_size用于获取初始chunk的大小
size_t SpaceManager::get_initial_chunk_size(Metaspace::MetaspaceType type) const {
  size_t requested;

  if (is_class()) {
    switch (type) {
    case Metaspace::BootMetaspaceType:       requested = Metaspace::first_class_chunk_word_size(); break;
    case Metaspace::ROMetaspaceType:         requested = ClassSpecializedChunk; break;
    case Metaspace::ReadWriteMetaspaceType:  requested = ClassSpecializedChunk; break;
    case Metaspace::AnonymousMetaspaceType:  requested = ClassSpecializedChunk; break;
    case Metaspace::ReflectionMetaspaceType: requested = ClassSpecializedChunk; break;
    default:                                 requested = ClassSmallChunk; break;
    }
  } else {
    switch (type) {
    case Metaspace::BootMetaspaceType:       requested = Metaspace::first_chunk_word_size(); break;
    case Metaspace::ROMetaspaceType:         requested = SharedReadOnlySize / wordSize; break;
    case Metaspace::ReadWriteMetaspaceType:  requested = SharedReadWriteSize / wordSize; break;
    case Metaspace::AnonymousMetaspaceType:  requested = SpecializedChunk; break;
    case Metaspace::ReflectionMetaspaceType: requested = SpecializedChunk; break;
    default:                                 requested = SmallChunk; break;
    }
  }

  // Adjust to one of the fixed chunk sizes (unless humongous)
    //将requested 适配到标准的chunk size
  const size_t adjusted = adjust_initial_chunk_size(requested);

  assert(adjusted != 0, err_msg("Incorrect initial chunk size. Requested: "
         SIZE_FORMAT " adjusted: " SIZE_FORMAT, requested, adjusted));

  return adjusted;
}

size_t SpaceManager::sum_free_in_chunks_in_use() const {
  MutexLockerEx cl(lock(), Mutex::_no_safepoint_check_flag);
  size_t free = 0;
  for (ChunkIndex i = ZeroIndex; i < NumberOfInUseLists; i = next_chunk_index(i)) {
    Metachunk* chunk = chunks_in_use(i);
    while (chunk != NULL) {
      free += chunk->free_word_size();
      chunk = chunk->next();
    }
  }
  return free;
}

size_t SpaceManager::sum_waste_in_chunks_in_use() const {
  MutexLockerEx cl(lock(), Mutex::_no_safepoint_check_flag);
  size_t result = 0;
  for (ChunkIndex i = ZeroIndex; i < NumberOfInUseLists; i = next_chunk_index(i)) {
   result += sum_waste_in_chunks_in_use(i);
  }

  return result;
}

size_t SpaceManager::sum_waste_in_chunks_in_use(ChunkIndex index) const {
  size_t result = 0;
  Metachunk* chunk = chunks_in_use(index);
  // Count the free space in all the chunk but not the
  // current chunk from which allocations are still being done.
  while (chunk != NULL) {
    if (chunk != current_chunk()) {
      result += chunk->free_word_size();
    }
    chunk = chunk->next();
  }
  return result;
}

size_t SpaceManager::sum_capacity_in_chunks_in_use() const {
  // For CMS use "allocated_chunks_words()" which does not need the
  // Metaspace lock.  For the other collectors sum over the
  // lists.  Use both methods as a check that "allocated_chunks_words()"
  // is correct.  That is, sum_capacity_in_chunks() is too expensive
  // to use in the product and allocated_chunks_words() should be used
  // but allow for  checking that allocated_chunks_words() returns the same
  // value as sum_capacity_in_chunks_in_use() which is the definitive
  // answer.
  if (UseConcMarkSweepGC) {
      //CMS不需要使用Metaspace lock，所以不用遍历校验
    return allocated_chunks_words();
  } else {
    MutexLockerEx cl(lock(), Mutex::_no_safepoint_check_flag);
    size_t sum = 0;
    for (ChunkIndex i = ZeroIndex; i < NumberOfInUseLists; i = next_chunk_index(i)) {
      Metachunk* chunk = chunks_in_use(i);
        //遍历所有的Metachunk
      while (chunk != NULL) {
        sum += chunk->word_size();
        chunk = chunk->next();
      }
    }
  return sum;
  }
}

size_t SpaceManager::sum_count_in_chunks_in_use() {
  size_t count = 0;
  for (ChunkIndex i = ZeroIndex; i < NumberOfInUseLists; i = next_chunk_index(i)) {
    count = count + sum_count_in_chunks_in_use(i);
  }

  return count;
}

size_t SpaceManager::sum_count_in_chunks_in_use(ChunkIndex i) {
  size_t count = 0;
  Metachunk* chunk = chunks_in_use(i);
  while (chunk != NULL) {
    count++;
    chunk = chunk->next();
  }
  return count;
}


size_t SpaceManager::sum_used_in_chunks_in_use() const {
  MutexLockerEx cl(lock(), Mutex::_no_safepoint_check_flag);
  size_t used = 0;
  for (ChunkIndex i = ZeroIndex; i < NumberOfInUseLists; i = next_chunk_index(i)) {
    Metachunk* chunk = chunks_in_use(i);
    while (chunk != NULL) {
      used += chunk->used_word_size();
      chunk = chunk->next();
    }
  }
  return used;
}

void SpaceManager::locked_print_chunks_in_use_on(outputStream* st) const {

  for (ChunkIndex i = ZeroIndex; i < NumberOfInUseLists; i = next_chunk_index(i)) {
    Metachunk* chunk = chunks_in_use(i);
    st->print("SpaceManager: %s " PTR_FORMAT,
                 chunk_size_name(i), chunk);
    if (chunk != NULL) {
      st->print_cr(" free " SIZE_FORMAT,
                   chunk->free_word_size());
    } else {
      st->cr();
    }
  }

  chunk_manager()->locked_print_free_chunks(st);
  chunk_manager()->locked_print_sum_free_chunks(st);
}

size_t SpaceManager::calc_chunk_size(size_t word_size) {

  // Decide between a small chunk and a medium chunk.  Up to
  // _small_chunk_limit small chunks can be allocated but
  // once a medium chunk has been allocated, no more small
  // chunks will be allocated.
    //判断采用MediumChunk还是SmallChunk
  size_t chunk_word_size;
  if (chunks_in_use(MediumIndex) == NULL &&
      sum_count_in_chunks_in_use(SmallIndex) < _small_chunk_limit) {
    chunk_word_size = (size_t) small_chunk_size();
    if (word_size + Metachunk::overhead() > small_chunk_size()) {
      chunk_word_size = medium_chunk_size();
    }
  } else {
    chunk_word_size = medium_chunk_size();
  }

  // Might still need a humongous chunk.  Enforce
  // humongous allocations sizes to be aligned up to
  // the smallest chunk size.
    //按照SpecializedChunk的内存取整
  size_t if_humongous_sized_chunk =
    align_size_up(word_size + Metachunk::overhead(),
                  smallest_chunk_size());
  chunk_word_size =
    MAX2((size_t) chunk_word_size, if_humongous_sized_chunk);

    //is_humongous为false时，chunk_word_size就是SmallChunk
    //is_humongous为true时，chunk_word_size就是MediumChunk,word_size大于MediumChunk，算出来的if_humongous_sized_chunk肯定大于chunk_word_size
    //取最大值时，chunk_word_size变成if_humongous_sized_chunk
  assert(!SpaceManager::is_humongous(word_size) ||
         chunk_word_size == if_humongous_sized_chunk,
         err_msg("Size calculation is wrong, word_size " SIZE_FORMAT
                 " chunk_word_size " SIZE_FORMAT,
                 word_size, chunk_word_size));
  if (TraceMetadataHumongousAllocation &&
      SpaceManager::is_humongous(word_size)) {
    gclog_or_tty->print_cr("Metadata humongous allocation:");
    gclog_or_tty->print_cr("  word_size " PTR_FORMAT, word_size);
    gclog_or_tty->print_cr("  chunk_word_size " PTR_FORMAT,
                           chunk_word_size);
    gclog_or_tty->print_cr("    chunk overhead " PTR_FORMAT,
                           Metachunk::overhead());
  }
  return chunk_word_size;
}

void SpaceManager::track_metaspace_memory_usage() {
  if (is_init_completed()) {
    if (is_class()) {
      MemoryService::track_compressed_class_memory_usage();
    }
    MemoryService::track_metaspace_memory_usage();
  }
}

MetaWord* SpaceManager::grow_and_allocate(size_t word_size) {
    //校验参数
  assert(vs_list()->current_virtual_space() != NULL,
         "Should have been set");
  assert(current_chunk() == NULL ||
         current_chunk()->allocate(word_size) == NULL,
         "Don't need to expand");
    //获取锁expand_lock
  MutexLockerEx cl(SpaceManager::expand_lock(), Mutex::_no_safepoint_check_flag);

  if (TraceMetadataChunkAllocation && Verbose) {
    size_t words_left = 0;
    size_t words_used = 0;
    if (current_chunk() != NULL) {
      words_left = current_chunk()->free_word_size();
      words_used = current_chunk()->used_word_size();
    }
      //打印日志
    gclog_or_tty->print_cr("SpaceManager::grow_and_allocate for " SIZE_FORMAT
                           " words " SIZE_FORMAT " words used " SIZE_FORMAT
                           " words left",
                            word_size, words_used, words_left);
  }

  // Get another chunk out of the virtual space
    //计算新chunk的大小
  size_t chunk_word_size = calc_chunk_size(word_size);
    //首先从ChunkManager的空闲列表中查找，查找失败则VirtualSpaceList创建一个新的chunk
  Metachunk* next = get_new_chunk(chunk_word_size);

  MetaWord* mem = NULL;

  // If a chunk was available, add it to the in-use chunk list
  // and do an allocation from it.
  if (next != NULL) {
    // Add to this manager's list of chunks in use.
      //将新chunk添加到_chunks_in_use数组中
    add_chunk(next, false);
      //使用新chunk分配内存
    mem = next->allocate(word_size);
  }

  // Track metaspace memory usage statistic.
  track_metaspace_memory_usage();

  return mem;
}

void SpaceManager::print_on(outputStream* st) const {

  for (ChunkIndex i = ZeroIndex;
       i < NumberOfInUseLists ;
       i = next_chunk_index(i) ) {
    st->print_cr("  chunks_in_use " PTR_FORMAT " chunk size " PTR_FORMAT,
                 chunks_in_use(i),
                 chunks_in_use(i) == NULL ? 0 : chunks_in_use(i)->word_size());
  }
  st->print_cr("    waste:  Small " SIZE_FORMAT " Medium " SIZE_FORMAT
               " Humongous " SIZE_FORMAT,
               sum_waste_in_chunks_in_use(SmallIndex),
               sum_waste_in_chunks_in_use(MediumIndex),
               sum_waste_in_chunks_in_use(HumongousIndex));
  // block free lists
  if (block_freelists() != NULL) {
    st->print_cr("total in block free lists " SIZE_FORMAT,
      block_freelists()->total_size());
  }
}

SpaceManager::SpaceManager(Metaspace::MetadataType mdtype,
                           Mutex* lock) :
  _mdtype(mdtype),
  _allocated_blocks_words(0),
  _allocated_chunks_words(0),
  _allocated_chunks_count(0),
  _lock(lock)
{
  initialize();
}

void SpaceManager::inc_size_metrics(size_t words) {
  assert_lock_strong(SpaceManager::expand_lock());
  // Total of allocated Metachunks and allocated Metachunks count
  // for each SpaceManager
  _allocated_chunks_words = _allocated_chunks_words + words;
  _allocated_chunks_count++;
  // Global total of capacity in allocated Metachunks
  MetaspaceAux::inc_capacity(mdtype(), words);
  // Global total of allocated Metablocks.
  // used_words_slow() includes the overhead in each
  // Metachunk so include it in the used when the
  // Metachunk is first added (so only added once per
  // Metachunk).
  MetaspaceAux::inc_used(mdtype(), Metachunk::overhead());
}

void SpaceManager::inc_used_metrics(size_t words) {
  // Add to the per SpaceManager total
  Atomic::add_ptr(words, &_allocated_blocks_words);
  // Add to the global total
  MetaspaceAux::inc_used(mdtype(), words);
}

void SpaceManager::dec_total_from_size_metrics() {
    //将MetaspaceAux的计数减少
  MetaspaceAux::dec_capacity(mdtype(), allocated_chunks_words());
    //扣减所有block的内存空间
  MetaspaceAux::dec_used(mdtype(), allocated_blocks_words());
  // Also deduct the overhead per Metachunk
    //扣减Metachunk自身占用的内存空间
  MetaspaceAux::dec_used(mdtype(), allocated_chunks_count() * Metachunk::overhead());
}

void SpaceManager::initialize() {
  Metadebug::init_allocation_fail_alot_count();
    //初始化数组元素
  for (ChunkIndex i = ZeroIndex; i < NumberOfInUseLists; i = next_chunk_index(i)) {
      //  把所有chunk已用状态设置为null
    _chunks_in_use[i] = NULL;
  }
  _current_chunk = NULL;
  if (TraceMetadataChunkAllocation && Verbose) {
      //打印日志
    gclog_or_tty->print_cr("SpaceManager(): " PTR_FORMAT, this);
  }
}

// return_chunks是将某个Metachunk添加到ChunkManager中
// return_chunks方法添加的Metachunk都是通用规格的
void ChunkManager::return_chunks(ChunkIndex index, Metachunk* chunks) {
  if (chunks == NULL) {
    return;
  }
    //找到匹配的链表
  ChunkList* list = free_chunks(index);
  assert(list->size() == chunks->word_size(), "Mismatch in chunk sizes");
  assert_lock_strong(SpaceManager::expand_lock());
  Metachunk* cur = chunks;

  // This returns chunks one at a time.  If a new
  // class List can be created that is a base class
  // of FreeList then something like FreeList::prepend()
  // can be used in place of this loop
  while (cur != NULL) {
    assert(cur->container() != NULL, "Container should have been set");
      //增加Metachunk所属的VirtualSpaceNode的计数器
    cur->container()->dec_container_count();
    // Capture the next link before it is changed
    // by the call to return_chunk_at_head();
      //获取下一个节点
    Metachunk* next = cur->next();
    DEBUG_ONLY(cur->set_is_tagged_free(true);)
      //将cur设置为链表头
    list->return_chunk_at_head(cur);
    cur = next;
  }
}

SpaceManager::~SpaceManager() {
  // This call this->_lock which can't be done while holding expand_lock()
    //校验计数器_allocated_chunks_words的正确
  assert(sum_capacity_in_chunks_in_use() == allocated_chunks_words(),
    err_msg("sum_capacity_in_chunks_in_use() " SIZE_FORMAT
            " allocated_chunks_words() " SIZE_FORMAT,
            sum_capacity_in_chunks_in_use(), allocated_chunks_words()));

    //获取锁
  MutexLockerEx fcl(SpaceManager::expand_lock(),
                    Mutex::_no_safepoint_check_flag);

    //校验ChunkManager的计数器的正确
  chunk_manager()->slow_locked_verify();

    //将MetaspaceAux中的计数器减少
  dec_total_from_size_metrics();

  if (TraceMetadataChunkAllocation && Verbose) {
      //打印日志
    gclog_or_tty->print_cr("~SpaceManager(): " PTR_FORMAT, this);
    locked_print_chunks_in_use_on(gclog_or_tty);
  }

  // Do not mangle freed Metachunks.  The chunk size inside Metachunks
  // is during the freeing of a VirtualSpaceNodes.

  // Have to update before the chunks_in_use lists are emptied
  // below.
    // 增加空闲的chunk的数量和空闲chunk的内存大小
  chunk_manager()->inc_free_chunks_total(allocated_chunks_words(),
                                         sum_count_in_chunks_in_use());

  // Add all the chunks in use by this space manager
  // to the global list of free chunks.

  // Follow each list of chunks-in-use and add them to the
  // free lists.  Each list is NULL terminated.

  for (ChunkIndex i = ZeroIndex; i < HumongousIndex; i = next_chunk_index(i)) {
    if (TraceMetadataChunkAllocation && Verbose) {
      gclog_or_tty->print_cr("returned %d %s chunks to freelist",
                             sum_count_in_chunks_in_use(i),
                             chunk_size_name(i));
    }
      //遍历_chunks_in_use中的Chunk，将其归还到ChunkManager
    Metachunk* chunks = chunks_in_use(i);
    chunk_manager()->return_chunks(i, chunks);
    set_chunks_in_use(i, NULL);
    if (TraceMetadataChunkAllocation && Verbose) {
      gclog_or_tty->print_cr("updated freelist count %d %s",
                             chunk_manager()->free_chunks(i)->count(),
                             chunk_size_name(i));
    }
    assert(i != HumongousIndex, "Humongous chunks are handled explicitly later");
  }

  // The medium chunk case may be optimized by passing the head and
  // tail of the medium chunk list to add_at_head().  The tail is often
  // the current chunk but there are probably exceptions.

  // Humongous chunks
  if (TraceMetadataChunkAllocation && Verbose) {
    gclog_or_tty->print_cr("returned %d %s humongous chunks to dictionary",
                            sum_count_in_chunks_in_use(HumongousIndex),
                            chunk_size_name(HumongousIndex));
    gclog_or_tty->print("Humongous chunk dictionary: ");
  }
  // Humongous chunks are never the current chunk.
    //获取Humongous chunk，因为大小不规整，不能通过return_chunks方法添加到ChunkManager中
  Metachunk* humongous_chunks = chunks_in_use(HumongousIndex);

  while (humongous_chunks != NULL) {
#ifdef ASSERT
    humongous_chunks->set_is_tagged_free(true);
#endif
    if (TraceMetadataChunkAllocation && Verbose) {
      gclog_or_tty->print(PTR_FORMAT " (" SIZE_FORMAT ") ",
                          humongous_chunks,
                          humongous_chunks->word_size());
    }
      //校验Humongous chunk的内存大小是经过内存取整的
    assert(humongous_chunks->word_size() == (size_t)
           align_size_up(humongous_chunks->word_size(),
                             smallest_chunk_size()),
           err_msg("Humongous chunk size is wrong: word size " SIZE_FORMAT
                   " granularity %d",
                   humongous_chunks->word_size(), smallest_chunk_size()));
      //获取下一个chunk
    Metachunk* next_humongous_chunks = humongous_chunks->next();
      //减少VirtualSpaceNode中的计数器
    humongous_chunks->container()->dec_container_count();
      //归还到_humongous_dictionary中
    chunk_manager()->humongous_dictionary()->return_chunk(humongous_chunks);
    humongous_chunks = next_humongous_chunks;
  }
  if (TraceMetadataChunkAllocation && Verbose) {
    gclog_or_tty->cr();
    gclog_or_tty->print_cr("updated dictionary count %d %s",
                     chunk_manager()->humongous_dictionary()->total_count(),
                     chunk_size_name(HumongousIndex));
  }
  chunk_manager()->slow_locked_verify();
}

const char* SpaceManager::chunk_size_name(ChunkIndex index) const {
  switch (index) {
    case SpecializedIndex:
      return "Specialized";
    case SmallIndex:
      return "Small";
    case MediumIndex:
      return "Medium";
    case HumongousIndex:
      return "Humongous";
    default:
      return NULL;
  }
}

ChunkIndex ChunkManager::list_index(size_t size) {
  if (free_chunks(SpecializedIndex)->size() == size) {
    return SpecializedIndex;
  }
  if (free_chunks(SmallIndex)->size() == size) {
    return SmallIndex;
  }
  if (free_chunks(MediumIndex)->size() == size) {
    return MediumIndex;
  }

    // free_chunks(MediumIndex)->size()并非返回这个Metachunk链表元素的个数，而是包含的Metachunk的大小
  assert(size > free_chunks(MediumIndex)->size(), "Not a humongous chunk");
  return HumongousIndex;
}

// deallocate用于释放这个内存块，将内存块作为MetaBlock归还到BlockFreelist中
void SpaceManager::deallocate(MetaWord* p, size_t word_size) {
  assert_lock_strong(_lock);
    //获取取整后的字宽数
  size_t raw_word_size = get_raw_word_size(word_size);
  size_t min_size = TreeChunk<Metablock, FreeList<Metablock> >::min_size();
  assert(raw_word_size >= min_size,
         err_msg("Should not deallocate dark matter " SIZE_FORMAT "<" SIZE_FORMAT, word_size, min_size));
    //将其作为MetaBlock放到BlockFreelist中
  block_freelists()->return_block(p, raw_word_size);
}

// Adds a chunk to the list of chunks in use.
void SpaceManager::add_chunk(Metachunk* new_chunk, bool make_current) {

  assert(new_chunk != NULL, "Should not be NULL");
  assert(new_chunk->next() == NULL, "Should not be on a list");

    //将其重置成初始状态
  new_chunk->reset_empty();

  // Find the correct list and and set the current
  // chunk for that list.
    //根据chunk的大小找到匹配的链表
  ChunkIndex index = chunk_manager()->list_index(new_chunk->word_size());

    //如果是规整的chunk
  if (index != HumongousIndex) {
      //将当前chunk的剩余空间分配成block，放入BlockFreelist中，避免空间浪费
    retire_current_chunk();
      //将新的chunk设置为当前chunk，原来的chunk作为当前chunk的next节点
    set_current_chunk(new_chunk);
    new_chunk->set_next(chunks_in_use(index));
    set_chunks_in_use(index, new_chunk);
  } else {
    // For null class loader data and DumpSharedSpaces, the first chunk isn't
    // small, so small will be null.  Link this first chunk as the current
    // chunk.
    if (make_current) {
      // Set as the current chunk but otherwise treat as a humongous chunk.
        //将新chunk设置为当前chunk，SpaceManager::grow_and_allocate中调用时永远传false，Metaspace::initialize_first_chunk中调用时传true
        //只有启动类加载器对应的metaspace才会将_current_chunk设置为一个humongous chunk
      set_current_chunk(new_chunk);
    }
    // Link at head.  The _current_chunk only points to a humongous chunk for
    // the null class loader metaspace (class and data virtual space managers)
    // any humongous chunks so will not point to the tail
    // of the humongous chunks list.
    new_chunk->set_next(chunks_in_use(HumongousIndex));
    set_chunks_in_use(HumongousIndex, new_chunk);

    assert(new_chunk->word_size() > medium_chunk_size(), "List inconsistency");
  }

  // Add to the running sum of capacity
    //增加计数
  inc_size_metrics(new_chunk->word_size());

  assert(new_chunk->is_empty(), "Not ready for reuse");
  if (TraceMetadataChunkAllocation && Verbose) {
    gclog_or_tty->print("SpaceManager::add_chunk: %d) ",
                        sum_count_in_chunks_in_use());
    new_chunk->print_on(gclog_or_tty);
      //打印日志
    chunk_manager()->locked_print_free_chunks(gclog_or_tty);
  }
}

void SpaceManager::retire_current_chunk() {
  if (current_chunk() != NULL) {
      //获取剩余的内存
    size_t remaining_words = current_chunk()->free_word_size();
    if (remaining_words >= TreeChunk<Metablock, FreeList<Metablock> >::min_size()) {
        //如果大于最小值，则将其变成block放到BlockFreelist中，避免空间浪费
      block_freelists()->return_block(current_chunk()->allocate(remaining_words), remaining_words);
      inc_used_metrics(remaining_words);
    }
  }
}

Metachunk* SpaceManager::get_new_chunk(size_t chunk_word_size) {
  // Get a chunk from the chunk freelist
    //首先从空闲的chunk列表中查找
  Metachunk* next = chunk_manager()->chunk_freelist_allocate(chunk_word_size);

  if (next == NULL) {
      //查找失败，从VirtualSpaceList中分配一个新的chunk
    next = vs_list()->get_new_chunk(chunk_word_size,
                                    medium_chunk_bunch());
  }

  if (TraceMetadataHumongousAllocation && next != NULL &&
      SpaceManager::is_humongous(next->word_size())) {
      //打印日志
    gclog_or_tty->print_cr("  new humongous chunk word size "
                           PTR_FORMAT, next->word_size());
  }

  return next;
}

// allocate方法用来分配一个指定大小的内存块，优先从BlockFreelist中分配，分配失败从当前chunk中分配，还是失败则创建一个新的chunk，从新chunk中分配
MetaWord* SpaceManager::allocate(size_t word_size) {
  MutexLockerEx cl(lock(), Mutex::_no_safepoint_check_flag);

    //获取取整后的字宽数
  size_t raw_word_size = get_raw_word_size(word_size);
  BlockFreelist* fl =  block_freelists();
  MetaWord* p = NULL;
  // Allocation from the dictionary is expensive in the sense that
  // the dictionary has to be searched for a size.  Don't allocate
  // from the dictionary until it starts to get fat.  Is this
  // a reasonable policy?  Maybe an skinny dictionary is fast enough
  // for allocations.  Do some profiling.  JJJ
    //从BlockFreelist中分配需要遍历，成本比较贵，所以只有当BlockFreelist对应的内存比较大的时候才尝试从BlockFreelist中分配
    //allocation_from_dictionary_limit是一个固定值，固定值4K
  if (fl->total_size() > allocation_from_dictionary_limit) {
    p = fl->get_block(raw_word_size);
  }
  if (p == NULL) {
      // 直接看这一行
      //从当前chunk中分配，如果内存不足则创建一个新的chunk，从新chunk中分配
    p = allocate_work(raw_word_size);
  }

  return p;
}

// Returns the address of spaced allocated for "word_size".
// This methods does not know about blocks (Metablocks)
MetaWord* SpaceManager::allocate_work(size_t word_size) {
  assert_lock_strong(_lock);
#ifdef ASSERT
  if (Metadebug::test_metadata_failure()) {
    return NULL;
  }
#endif
  // Is there space in the current chunk?
  MetaWord* result = NULL;

  // For DumpSharedSpaces, only allocate out of the current chunk which is
  // never null because we gave it the size we wanted.   Caller reports out
  // of memory if this returns null.
    //DumpSharedSpaces表示Dump出共享的space到一个文件中，默认为false
    //如果DumpSharedSpaces为true则只尝试从当前的chunk中分配，如果分配失败，返回NULL由调用方负责处理
  if (DumpSharedSpaces) {
    assert(current_chunk() != NULL, "should never happen");
    inc_used_metrics(word_size);
    return current_chunk()->allocate(word_size); // caller handles null result
  }

    // current_chunk() 函数拿到当前在用的chunk，这个chunk就是initialize_first_chunk()函数实现里面设置的，然后再通过allocate分配，怎么分配的呢，继续往下看
  if (current_chunk() != NULL) {
      //从当前的chunk中分配
    result = current_chunk()->allocate(word_size);
  }

    // 分配不成功，肯定是容量不够了，这一步就是扩容
  if (result == NULL) {
      //当前chunk可用空间不足导致分配失败，则扩展一个新的chunk，从新的chunk中分配
    result = grow_and_allocate(word_size);
  }

  if (result != NULL) {
      //如果分配成功，增加计数
    inc_used_metrics(word_size);
    assert(result != (MetaWord*) chunks_in_use(MediumIndex),
           "Head of the list is being allocated");
  }

  return result;
}

void SpaceManager::verify() {
  // If there are blocks in the dictionary, then
  // verfication of chunks does not work since
  // being in the dictionary alters a chunk.
  if (block_freelists()->total_size() == 0) {
    for (ChunkIndex i = ZeroIndex; i < NumberOfInUseLists; i = next_chunk_index(i)) {
      Metachunk* curr = chunks_in_use(i);
      while (curr != NULL) {
        curr->verify();
        verify_chunk_size(curr);
        curr = curr->next();
      }
    }
  }
}

void SpaceManager::verify_chunk_size(Metachunk* chunk) {
  assert(is_humongous(chunk->word_size()) ||
         chunk->word_size() == medium_chunk_size() ||
         chunk->word_size() == small_chunk_size() ||
         chunk->word_size() == specialized_chunk_size(),
         "Chunk size is wrong");
  return;
}

#ifdef ASSERT
void SpaceManager::verify_allocated_blocks_words() {
  // Verification is only guaranteed at a safepoint.
  assert(SafepointSynchronize::is_at_safepoint() || !Universe::is_fully_initialized(),
    "Verification can fail if the applications is running");
  assert(allocated_blocks_words() == sum_used_in_chunks_in_use(),
    err_msg("allocation total is not consistent " SIZE_FORMAT
            " vs " SIZE_FORMAT,
            allocated_blocks_words(), sum_used_in_chunks_in_use()));
}

#endif

void SpaceManager::dump(outputStream* const out) const {
  size_t curr_total = 0;
  size_t waste = 0;
  uint i = 0;
  size_t used = 0;
  size_t capacity = 0;

  // Add up statistics for all chunks in this SpaceManager.
  for (ChunkIndex index = ZeroIndex;
       index < NumberOfInUseLists;
       index = next_chunk_index(index)) {
    for (Metachunk* curr = chunks_in_use(index);
         curr != NULL;
         curr = curr->next()) {
      out->print("%d) ", i++);
      curr->print_on(out);
      curr_total += curr->word_size();
      used += curr->used_word_size();
      capacity += curr->word_size();
      waste += curr->free_word_size() + curr->overhead();;
    }
  }

  if (TraceMetadataChunkAllocation && Verbose) {
    block_freelists()->print_on(out);
  }

  size_t free = current_chunk() == NULL ? 0 : current_chunk()->free_word_size();
  // Free space isn't wasted.
  waste -= free;

  out->print_cr("total of all chunks "  SIZE_FORMAT " used " SIZE_FORMAT
                " free " SIZE_FORMAT " capacity " SIZE_FORMAT
                " waste " SIZE_FORMAT, curr_total, used, free, capacity, waste);
}

#ifndef PRODUCT
void SpaceManager::mangle_freed_chunks() {
  for (ChunkIndex index = ZeroIndex;
       index < NumberOfInUseLists;
       index = next_chunk_index(index)) {
    for (Metachunk* curr = chunks_in_use(index);
         curr != NULL;
         curr = curr->next()) {
      curr->mangle();
    }
  }
}
#endif // PRODUCT

// MetaspaceAux


size_t MetaspaceAux::_capacity_words[] = {0, 0};
size_t MetaspaceAux::_used_words[] = {0, 0};

size_t MetaspaceAux::free_bytes(Metaspace::MetadataType mdtype) {
  VirtualSpaceList* list = Metaspace::get_space_list(mdtype);
  return list == NULL ? 0 : list->free_bytes();
}

size_t MetaspaceAux::free_bytes() {
  return free_bytes(Metaspace::ClassType) + free_bytes(Metaspace::NonClassType);
}

void MetaspaceAux::dec_capacity(Metaspace::MetadataType mdtype, size_t words) {
  assert_lock_strong(SpaceManager::expand_lock());
  assert(words <= capacity_words(mdtype),
    err_msg("About to decrement below 0: words " SIZE_FORMAT
            " is greater than _capacity_words[%u] " SIZE_FORMAT,
            words, mdtype, capacity_words(mdtype)));
  _capacity_words[mdtype] -= words;
}

void MetaspaceAux::inc_capacity(Metaspace::MetadataType mdtype, size_t words) {
  assert_lock_strong(SpaceManager::expand_lock());
  // Needs to be atomic
  _capacity_words[mdtype] += words;
}

void MetaspaceAux::dec_used(Metaspace::MetadataType mdtype, size_t words) {
  assert(words <= used_words(mdtype),
    err_msg("About to decrement below 0: words " SIZE_FORMAT
            " is greater than _used_words[%u] " SIZE_FORMAT,
            words, mdtype, used_words(mdtype)));
  // For CMS deallocation of the Metaspaces occurs during the
  // sweep which is a concurrent phase.  Protection by the expand_lock()
  // is not enough since allocation is on a per Metaspace basis
  // and protected by the Metaspace lock.
  jlong minus_words = (jlong) - (jlong) words;
  Atomic::add_ptr(minus_words, &_used_words[mdtype]);
}

// inc_used / dec_used
//     这两方法就是用来增加和减少静态属性_used_words的，这两方法的调用方都是SpaceManager
void MetaspaceAux::inc_used(Metaspace::MetadataType mdtype, size_t words) {
  // _used_words tracks allocations for
  // each piece of metadata.  Those allocations are
  // generally done concurrently by different application
  // threads so must be done atomically.
  Atomic::add_ptr(words, &_used_words[mdtype]);
}

size_t MetaspaceAux::used_bytes_slow(Metaspace::MetadataType mdtype) {
  size_t used = 0;
    //ClassLoaderDataGraphMetaspaceIterator的构造方法会获取ClassLoaderData链表的头元素，从头元素开始依次遍历
  ClassLoaderDataGraphMetaspaceIterator iter;
    //遍历所有的ClassLoader
  while (iter.repeat()) {
    Metaspace* msp = iter.get_next();
    // Sum allocated_blocks_words for each metaspace
    if (msp != NULL) {
      used += msp->used_words_slow(mdtype);
    }
  }
  return used * BytesPerWord;
}

size_t MetaspaceAux::free_bytes_slow(Metaspace::MetadataType mdtype) {
  size_t free = 0;
  ClassLoaderDataGraphMetaspaceIterator iter;
  while (iter.repeat()) {
    Metaspace* msp = iter.get_next();
    if (msp != NULL) {
      free += msp->free_words_slow(mdtype);
    }
  }
  return free * BytesPerWord;
}

size_t MetaspaceAux::capacity_bytes_slow(Metaspace::MetadataType mdtype) {
  if ((mdtype == Metaspace::ClassType) && !Metaspace::using_class_space()) {
    return 0;
  }
  // Don't count the space in the freelists.  That space will be
  // added to the capacity calculation as needed.
  size_t capacity = 0;
  ClassLoaderDataGraphMetaspaceIterator iter;
  while (iter.repeat()) {
    Metaspace* msp = iter.get_next();
    if (msp != NULL) {
      capacity += msp->capacity_words_slow(mdtype);
    }
  }
  return capacity * BytesPerWord;
}

size_t MetaspaceAux::capacity_bytes_slow() {
#ifdef PRODUCT
  // Use capacity_bytes() in PRODUCT instead of this function.
  guarantee(false, "Should not call capacity_bytes_slow() in the PRODUCT");
#endif
  size_t class_capacity = capacity_bytes_slow(Metaspace::ClassType);
  size_t non_class_capacity = capacity_bytes_slow(Metaspace::NonClassType);
  assert(capacity_bytes() == class_capacity + non_class_capacity,
      err_msg("bad accounting: capacity_bytes() " SIZE_FORMAT
        " class_capacity + non_class_capacity " SIZE_FORMAT
        " class_capacity " SIZE_FORMAT " non_class_capacity " SIZE_FORMAT,
        capacity_bytes(), class_capacity + non_class_capacity,
        class_capacity, non_class_capacity));

  return class_capacity + non_class_capacity;
}

size_t MetaspaceAux::reserved_bytes(Metaspace::MetadataType mdtype) {
  VirtualSpaceList* list = Metaspace::get_space_list(mdtype);
  return list == NULL ? 0 : list->reserved_bytes();
}

size_t MetaspaceAux::committed_bytes(Metaspace::MetadataType mdtype) {
  VirtualSpaceList* list = Metaspace::get_space_list(mdtype);
  return list == NULL ? 0 : list->committed_bytes();
}

size_t MetaspaceAux::min_chunk_size_words() { return Metaspace::first_chunk_word_size(); }

size_t MetaspaceAux::free_chunks_total_words(Metaspace::MetadataType mdtype) {
  ChunkManager* chunk_manager = Metaspace::get_chunk_manager(mdtype);
  if (chunk_manager == NULL) {
    return 0;
  }
  chunk_manager->slow_verify();
  return chunk_manager->free_chunks_total_words();
}

size_t MetaspaceAux::free_chunks_total_bytes(Metaspace::MetadataType mdtype) {
  return free_chunks_total_words(mdtype) * BytesPerWord;
}

size_t MetaspaceAux::free_chunks_total_words() {
  return free_chunks_total_words(Metaspace::ClassType) +
         free_chunks_total_words(Metaspace::NonClassType);
}

size_t MetaspaceAux::free_chunks_total_bytes() {
  return free_chunks_total_words() * BytesPerWord;
}

bool MetaspaceAux::has_chunk_free_list(Metaspace::MetadataType mdtype) {
  return Metaspace::get_chunk_manager(mdtype) != NULL;
}

MetaspaceChunkFreeListSummary MetaspaceAux::chunk_free_list_summary(Metaspace::MetadataType mdtype) {
  if (!has_chunk_free_list(mdtype)) {
    return MetaspaceChunkFreeListSummary();
  }

  const ChunkManager* cm = Metaspace::get_chunk_manager(mdtype);
  return cm->chunk_free_list_summary();
}

void MetaspaceAux::print_metaspace_change(size_t prev_metadata_used) {
  gclog_or_tty->print(", [Metaspace:");
  if (PrintGCDetails && Verbose) {
    gclog_or_tty->print(" "  SIZE_FORMAT
                        "->" SIZE_FORMAT
                        "("  SIZE_FORMAT ")",
                        prev_metadata_used,
                        used_bytes(),
                        reserved_bytes());
  } else {
    gclog_or_tty->print(" "  SIZE_FORMAT "K"
                        "->" SIZE_FORMAT "K"
                        "("  SIZE_FORMAT "K)",
                        prev_metadata_used/K,
                        used_bytes()/K,
                        reserved_bytes()/K);
  }

  gclog_or_tty->print("]");
}

// This is printed when PrintGCDetails
void MetaspaceAux::print_on(outputStream* out) {
  Metaspace::MetadataType nct = Metaspace::NonClassType;

  out->print_cr(" Metaspace       "
                "used "      SIZE_FORMAT "K, "
                "capacity "  SIZE_FORMAT "K, "
                "committed " SIZE_FORMAT "K, "
                "reserved "  SIZE_FORMAT "K",
                used_bytes()/K,
                capacity_bytes()/K,
                committed_bytes()/K,
                reserved_bytes()/K);

  if (Metaspace::using_class_space()) {
    Metaspace::MetadataType ct = Metaspace::ClassType;
    out->print_cr("  class space    "
                  "used "      SIZE_FORMAT "K, "
                  "capacity "  SIZE_FORMAT "K, "
                  "committed " SIZE_FORMAT "K, "
                  "reserved "  SIZE_FORMAT "K",
                  used_bytes(ct)/K,
                  capacity_bytes(ct)/K,
                  committed_bytes(ct)/K,
                  reserved_bytes(ct)/K);
  }
}

// Print information for class space and data space separately.
// This is almost the same as above.
void MetaspaceAux::print_on(outputStream* out, Metaspace::MetadataType mdtype) {
  size_t free_chunks_capacity_bytes = free_chunks_total_bytes(mdtype);
  size_t capacity_bytes = capacity_bytes_slow(mdtype);
  size_t used_bytes = used_bytes_slow(mdtype);
  size_t free_bytes = free_bytes_slow(mdtype);
  size_t used_and_free = used_bytes + free_bytes +
                           free_chunks_capacity_bytes;
  out->print_cr("  Chunk accounting: used in chunks " SIZE_FORMAT
             "K + unused in chunks " SIZE_FORMAT "K  + "
             " capacity in free chunks " SIZE_FORMAT "K = " SIZE_FORMAT
             "K  capacity in allocated chunks " SIZE_FORMAT "K",
             used_bytes / K,
             free_bytes / K,
             free_chunks_capacity_bytes / K,
             used_and_free / K,
             capacity_bytes / K);
  // Accounting can only be correct if we got the values during a safepoint
  assert(!SafepointSynchronize::is_at_safepoint() || used_and_free == capacity_bytes, "Accounting is wrong");
}

// Print total fragmentation for class metaspaces
void MetaspaceAux::print_class_waste(outputStream* out) {
  assert(Metaspace::using_class_space(), "class metaspace not used");
  size_t cls_specialized_waste = 0, cls_small_waste = 0, cls_medium_waste = 0;
  size_t cls_specialized_count = 0, cls_small_count = 0, cls_medium_count = 0, cls_humongous_count = 0;
  ClassLoaderDataGraphMetaspaceIterator iter;
  while (iter.repeat()) {
    Metaspace* msp = iter.get_next();
    if (msp != NULL) {
      cls_specialized_waste += msp->class_vsm()->sum_waste_in_chunks_in_use(SpecializedIndex);
      cls_specialized_count += msp->class_vsm()->sum_count_in_chunks_in_use(SpecializedIndex);
      cls_small_waste += msp->class_vsm()->sum_waste_in_chunks_in_use(SmallIndex);
      cls_small_count += msp->class_vsm()->sum_count_in_chunks_in_use(SmallIndex);
      cls_medium_waste += msp->class_vsm()->sum_waste_in_chunks_in_use(MediumIndex);
      cls_medium_count += msp->class_vsm()->sum_count_in_chunks_in_use(MediumIndex);
      cls_humongous_count += msp->class_vsm()->sum_count_in_chunks_in_use(HumongousIndex);
    }
  }
  out->print_cr(" class: " SIZE_FORMAT " specialized(s) " SIZE_FORMAT ", "
                SIZE_FORMAT " small(s) " SIZE_FORMAT ", "
                SIZE_FORMAT " medium(s) " SIZE_FORMAT ", "
                "large count " SIZE_FORMAT,
                cls_specialized_count, cls_specialized_waste,
                cls_small_count, cls_small_waste,
                cls_medium_count, cls_medium_waste, cls_humongous_count);
}

// Print total fragmentation for data and class metaspaces separately
void MetaspaceAux::print_waste(outputStream* out) {
  size_t specialized_waste = 0, small_waste = 0, medium_waste = 0;
  size_t specialized_count = 0, small_count = 0, medium_count = 0, humongous_count = 0;

  ClassLoaderDataGraphMetaspaceIterator iter;
  while (iter.repeat()) {
    Metaspace* msp = iter.get_next();
    if (msp != NULL) {
      specialized_waste += msp->vsm()->sum_waste_in_chunks_in_use(SpecializedIndex);
      specialized_count += msp->vsm()->sum_count_in_chunks_in_use(SpecializedIndex);
      small_waste += msp->vsm()->sum_waste_in_chunks_in_use(SmallIndex);
      small_count += msp->vsm()->sum_count_in_chunks_in_use(SmallIndex);
      medium_waste += msp->vsm()->sum_waste_in_chunks_in_use(MediumIndex);
      medium_count += msp->vsm()->sum_count_in_chunks_in_use(MediumIndex);
      humongous_count += msp->vsm()->sum_count_in_chunks_in_use(HumongousIndex);
    }
  }
  out->print_cr("Total fragmentation waste (words) doesn't count free space");
  out->print_cr("  data: " SIZE_FORMAT " specialized(s) " SIZE_FORMAT ", "
                        SIZE_FORMAT " small(s) " SIZE_FORMAT ", "
                        SIZE_FORMAT " medium(s) " SIZE_FORMAT ", "
                        "large count " SIZE_FORMAT,
             specialized_count, specialized_waste, small_count,
             small_waste, medium_count, medium_waste, humongous_count);
  if (Metaspace::using_class_space()) {
    print_class_waste(out);
  }
}

// Dump global metaspace things from the end of ClassLoaderDataGraph
void MetaspaceAux::dump(outputStream* out) {
  out->print_cr("All Metaspace:");
  out->print("data space: "); print_on(out, Metaspace::NonClassType);
  out->print("class space: "); print_on(out, Metaspace::ClassType);
  print_waste(out);
}

void MetaspaceAux::verify_free_chunks() {
  Metaspace::chunk_manager_metadata()->verify();
  if (Metaspace::using_class_space()) {
    Metaspace::chunk_manager_class()->verify();
  }
}

void MetaspaceAux::verify_capacity() {
#ifdef ASSERT
  size_t running_sum_capacity_bytes = capacity_bytes();
  // For purposes of the running sum of capacity, verify against capacity
  size_t capacity_in_use_bytes = capacity_bytes_slow();
  assert(running_sum_capacity_bytes == capacity_in_use_bytes,
    err_msg("capacity_words() * BytesPerWord " SIZE_FORMAT
            " capacity_bytes_slow()" SIZE_FORMAT,
            running_sum_capacity_bytes, capacity_in_use_bytes));
  for (Metaspace::MetadataType i = Metaspace::ClassType;
       i < Metaspace:: MetadataTypeCount;
       i = (Metaspace::MetadataType)(i + 1)) {
    size_t capacity_in_use_bytes = capacity_bytes_slow(i);
    assert(capacity_bytes(i) == capacity_in_use_bytes,
      err_msg("capacity_bytes(%u) " SIZE_FORMAT
              " capacity_bytes_slow(%u)" SIZE_FORMAT,
              i, capacity_bytes(i), i, capacity_in_use_bytes));
  }
#endif
}

void MetaspaceAux::verify_used() {
#ifdef ASSERT
  size_t running_sum_used_bytes = used_bytes();
  // For purposes of the running sum of used, verify against used
  size_t used_in_use_bytes = used_bytes_slow();
  assert(used_bytes() == used_in_use_bytes,
    err_msg("used_bytes() " SIZE_FORMAT
            " used_bytes_slow()" SIZE_FORMAT,
            used_bytes(), used_in_use_bytes));
  for (Metaspace::MetadataType i = Metaspace::ClassType;
       i < Metaspace:: MetadataTypeCount;
       i = (Metaspace::MetadataType)(i + 1)) {
    size_t used_in_use_bytes = used_bytes_slow(i);
    assert(used_bytes(i) == used_in_use_bytes,
      err_msg("used_bytes(%u) " SIZE_FORMAT
              " used_bytes_slow(%u)" SIZE_FORMAT,
              i, used_bytes(i), i, used_in_use_bytes));
  }
#endif
}

void MetaspaceAux::verify_metrics() {
  verify_capacity();
  verify_used();
}


// Metaspace methods

size_t Metaspace::_first_chunk_word_size = 0;
size_t Metaspace::_first_class_chunk_word_size = 0;

size_t Metaspace::_commit_alignment = 0;
size_t Metaspace::_reserve_alignment = 0;

// 构造函数用于初始化_vsm和_class_vsm，分配第一个Chunk。
// 构造方法的调用方ClassLoaderData::initialize_shared_metaspaces()是初始化启动类加载器使用的Metaspace，
// ClassLoaderData::metaspace_non_null是初始化其他的非启动类加载器使用的Metaspace，属于惰性初始化。
Metaspace::Metaspace(Mutex* lock, MetaspaceType type) {
  initialize(lock, type);
}

// 析构函数用于释放_vsm和_class_vsm，析构方法是在ClassLoaderData被删除时调用的
Metaspace::~Metaspace() {
    //释放SpaceManager
  delete _vsm;
  if (using_class_space()) {
    delete _class_vsm;
  }
}

VirtualSpaceList* Metaspace::_space_list = NULL;
VirtualSpaceList* Metaspace::_class_space_list = NULL;

ChunkManager* Metaspace::_chunk_manager_metadata = NULL;
ChunkManager* Metaspace::_chunk_manager_class = NULL;

#define VIRTUALSPACEMULTIPLIER 2

#ifdef _LP64
static const uint64_t UnscaledClassSpaceMax = (uint64_t(max_juint) + 1);

void Metaspace::set_narrow_klass_base_and_shift(address metaspace_base, address cds_base) {
  // Figure out the narrow_klass_base and the narrow_klass_shift.  The
  // narrow_klass_base is the lower of the metaspace base and the cds base
  // (if cds is enabled).  The narrow_klass_shift depends on the distance
  // between the lower base and higher address.
  address lower_base;
  address higher_address;
#if INCLUDE_CDS
  if (UseSharedSpaces) {
    higher_address = MAX2((address)(cds_base + FileMapInfo::shared_spaces_size()),
                          (address)(metaspace_base + compressed_class_space_size()));
    lower_base = MIN2(metaspace_base, cds_base);
  } else
#endif
  {
      //lower_base是起始地址，higher_address是终止地址
    higher_address = metaspace_base + compressed_class_space_size();
    lower_base = metaspace_base;

    uint64_t klass_encoding_max = UnscaledClassSpaceMax << LogKlassAlignmentInBytes;
    // If compressed class space fits in lower 32G, we don't need a base.
    if (higher_address <= (address)klass_encoding_max) {
      lower_base = 0; // effectively lower base is zero.
    }
  }

  Universe::set_narrow_klass_base(lower_base);

  if ((uint64_t)(higher_address - lower_base) <= UnscaledClassSpaceMax) {
    Universe::set_narrow_klass_shift(0);
  } else {
    assert(!UseSharedSpaces, "Cannot shift with UseSharedSpaces");
    Universe::set_narrow_klass_shift(LogKlassAlignmentInBytes);
  }
}

#if INCLUDE_CDS
// Return TRUE if the specified metaspace_base and cds_base are close enough
// to work with compressed klass pointers.
bool Metaspace::can_use_cds_with_metaspace_addr(char* metaspace_base, address cds_base) {
  assert(cds_base != 0 && UseSharedSpaces, "Only use with CDS");
  assert(UseCompressedClassPointers, "Only use with CompressedKlassPtrs");
  address lower_base = MIN2((address)metaspace_base, cds_base);
  address higher_address = MAX2((address)(cds_base + FileMapInfo::shared_spaces_size()),
                                (address)(metaspace_base + compressed_class_space_size()));
  return ((uint64_t)(higher_address - lower_base) <= UnscaledClassSpaceMax);
}
#endif

// Try to allocate the metaspace at the requested addr.
void Metaspace::allocate_metaspace_compressed_klass_ptrs(char* requested_addr, address cds_base) {
    //校验参数
  assert(using_class_space(), "called improperly");
  assert(UseCompressedClassPointers, "Only use with CompressedKlassPtrs");
  assert(compressed_class_space_size() < KlassEncodingMetaspaceMax,
         "Metaspace size is too big");
  assert_is_ptr_aligned(requested_addr, _reserve_alignment);
  assert_is_ptr_aligned(cds_base, _reserve_alignment);
  assert_is_size_aligned(compressed_class_space_size(), _reserve_alignment);

  // Don't use large pages for the class space.
    //尝试在指定起始地址处申请一段连续的内存空间
  bool large_pages = false;

#ifndef AARCH64
  ReservedSpace metaspace_rs = ReservedSpace(compressed_class_space_size(),
                                             _reserve_alignment,
                                             large_pages,
                                             requested_addr, 0);
#else // AARCH64
  ReservedSpace metaspace_rs;

  // Our compressed klass pointers may fit nicely into the lower 32
  // bits.
  if ((uint64_t)requested_addr + compressed_class_space_size() < 4*G)
    metaspace_rs = ReservedSpace(compressed_class_space_size(),
                                             _reserve_alignment,
                                             large_pages,
                                             requested_addr, 0);

  if (! metaspace_rs.is_reserved()) {
    // Try to align metaspace so that we can decode a compressed klass
    // with a single MOVK instruction.  We can do this iff the
    // compressed class base is a multiple of 4G.
    for (char *a = (char*)align_ptr_up(requested_addr, 4*G);
         a < (char*)(1024*G);
         a += 4*G) {
      if (UseSharedSpaces
          && ! can_use_cds_with_metaspace_addr(a, cds_base)) {
        // We failed to find an aligned base that will reach.  Fall
        // back to using our requested addr.
        metaspace_rs = ReservedSpace(compressed_class_space_size(),
                                             _reserve_alignment,
                                             large_pages,
                                             requested_addr, 0);
        break;
      }
      metaspace_rs = ReservedSpace(compressed_class_space_size(),
                                   _reserve_alignment,
                                   large_pages,
                                   a, 0);
      if (metaspace_rs.is_reserved())
        break;
    }
  }

#endif // AARCH64

    // 忽略起始地址，尝试重新申请，分配失败抛出异常
  if (!metaspace_rs.is_reserved()) {
#if INCLUDE_CDS
    if (UseSharedSpaces) {
      size_t increment = align_size_up(1*G, _reserve_alignment);

      // Keep trying to allocate the metaspace, increasing the requested_addr
      // by 1GB each time, until we reach an address that will no longer allow
      // use of CDS with compressed klass pointers.
      char *addr = requested_addr;
      while (!metaspace_rs.is_reserved() && (addr + increment > addr) &&
             can_use_cds_with_metaspace_addr(addr + increment, cds_base)) {
        addr = addr + increment;
        metaspace_rs = ReservedSpace(compressed_class_space_size(),
                                     _reserve_alignment, large_pages, addr, 0);
      }
    }
#endif
    // If no successful allocation then try to allocate the space anywhere.  If
    // that fails then OOM doom.  At this point we cannot try allocating the
    // metaspace as if UseCompressedClassPointers is off because too much
    // initialization has happened that depends on UseCompressedClassPointers.
    // So, UseCompressedClassPointers cannot be turned off at this point.
    if (!metaspace_rs.is_reserved()) {
      metaspace_rs = ReservedSpace(compressed_class_space_size(),
                                   _reserve_alignment, large_pages);
      if (!metaspace_rs.is_reserved()) {
        vm_exit_during_initialization(err_msg("Could not allocate metaspace: %d bytes",
                                              compressed_class_space_size()));
      }
    }
  }

  // If we got here then the metaspace got allocated.
  MemTracker::record_virtual_memory_type((address)metaspace_rs.base(), mtClass);

#if INCLUDE_CDS
  // Verify that we can use shared spaces.  Otherwise, turn off CDS.
  if (UseSharedSpaces && !can_use_cds_with_metaspace_addr(metaspace_rs.base(), cds_base)) {
    FileMapInfo::stop_sharing_and_unmap(
        "Could not allocate metaspace at a compatible address");
  }
#endif
    //重置narrow_klass_base和narrow_klass_shift
  set_narrow_klass_base_and_shift((address)metaspace_rs.base(),
                                  UseSharedSpaces ? (address)cds_base : 0);

  initialize_class_space(metaspace_rs);

    //打印日志
  if (PrintCompressedOopsMode || (PrintMiscellaneous && Verbose)) {
      print_compressed_class_space(gclog_or_tty, requested_addr);
  }
}

void Metaspace::print_compressed_class_space(outputStream* st, const char* requested_addr) {
  st->print_cr("Narrow klass base: " PTR_FORMAT ", Narrow klass shift: %d",
               p2i(Universe::narrow_klass_base()), Universe::narrow_klass_shift());
  if (_class_space_list != NULL) {
    address base = (address)_class_space_list->current_virtual_space()->bottom();
    st->print("Compressed class space size: " SIZE_FORMAT " Address: " PTR_FORMAT,
                 compressed_class_space_size(), p2i(base));
    if (requested_addr != 0) {
      st->print(" Req Addr: " PTR_FORMAT, p2i(requested_addr));
    }
    st->cr();
   }
 }

// For UseCompressedClassPointers the class space is reserved above the top of
// the Java heap.  The argument passed in is at the base of the compressed space.
// 初始化_class_space_list和_chunk_manager_class
void Metaspace::initialize_class_space(ReservedSpace rs) {
  // The reserved space size may be bigger because of alignment, esp with UseLargePages
  assert(rs.size() >= CompressedClassSpaceSize,
         err_msg(SIZE_FORMAT " != " UINTX_FORMAT, rs.size(), CompressedClassSpaceSize));
  assert(using_class_space(), "Must be using class space");
  _class_space_list = new VirtualSpaceList(rs);
  _chunk_manager_class = new ChunkManager(ClassSpecializedChunk, ClassSmallChunk, ClassMediumChunk);

  if (!_class_space_list->initialization_succeeded()) {
    vm_exit_during_initialization("Failed to setup compressed class space virtual space list.");
  }
}

#endif

// ergo_initialize用于初始化Metaspace的各种参数，如MetaspaceSize，MaxMetaspaceSize，MinMetaspaceExpansion等
void Metaspace::ergo_initialize() {
    // DumpSharedSpaces表示将共享的Metaspace空间dump到一个文件中，给其他JVM使用，默认为false
  if (DumpSharedSpaces) {
    // Using large pages when dumping the shared archive is currently not implemented.
    FLAG_SET_ERGO(bool, UseLargePagesInMetaspace, false);
  }

  size_t page_size = os::vm_page_size();
  if (UseLargePages && UseLargePagesInMetaspace) {
    page_size = os::large_page_size();
  }

    //初始化参数
  _commit_alignment  = page_size;
  _reserve_alignment = MAX2(page_size, (size_t)os::vm_allocation_granularity());

  // Do not use FLAG_SET_ERGO to update MaxMetaspaceSize, since this will
  // override if MaxMetaspaceSize was set on the command line or not.
  // This information is needed later to conform to the specification of the
  // java.lang.management.MemoryUsage API.
  //
  // Ideally, we would be able to set the default value of MaxMetaspaceSize in
  // globals.hpp to the aligned value, but this is not possible, since the
  // alignment depends on other flags being parsed.
  MaxMetaspaceSize = align_size_down_bounded(MaxMetaspaceSize, _reserve_alignment);

  if (MetaspaceSize > MaxMetaspaceSize) {
    MetaspaceSize = MaxMetaspaceSize;
  }

  MetaspaceSize = align_size_down_bounded(MetaspaceSize, _commit_alignment);

  assert(MetaspaceSize <= MaxMetaspaceSize, "MetaspaceSize should be limited by MaxMetaspaceSize");

  if (MetaspaceSize < 256*K) {
    vm_exit_during_initialization("Too small initial Metaspace size");
  }

  MinMetaspaceExpansion = align_size_down_bounded(MinMetaspaceExpansion, _commit_alignment);
  MaxMetaspaceExpansion = align_size_down_bounded(MaxMetaspaceExpansion, _commit_alignment);

  CompressedClassSpaceSize = align_size_down_bounded(CompressedClassSpaceSize, _reserve_alignment);
  set_compressed_class_space_size(CompressedClassSpaceSize);

  // Initial virtual space size will be calculated at global_initialize()
    //VIRTUALSPACEMULTIPLIER的值是2
  uintx min_metaspace_sz =
      VIRTUALSPACEMULTIPLIER * InitialBootClassLoaderMetaspaceSize;
  if (UseCompressedClassPointers) {
    if ((min_metaspace_sz + CompressedClassSpaceSize) >  MaxMetaspaceSize) {
      if (min_metaspace_sz >= MaxMetaspaceSize) {
        vm_exit_during_initialization("MaxMetaspaceSize is too small.");
      } else {
        FLAG_SET_ERGO(uintx, CompressedClassSpaceSize,
                      MaxMetaspaceSize - min_metaspace_sz);
      }
    }
  } else if (min_metaspace_sz >= MaxMetaspaceSize) {
    FLAG_SET_ERGO(uintx, InitialBootClassLoaderMetaspaceSize,
                  min_metaspace_sz);
  }

}

/**
 * 元空间就是从C堆中划出来的一片完整的区域，为了提升元数据的内存分配效率，又把元空间按若干个chunk内存块管理起来，其中chunk块又分为已使用和空间两种类型，并分别用VirtualSpaceList和ChunkManager来管理，chunk内存块之间以链表的形式关联起来，同时为了满足不同元数据占用内存大小的内存分配，chunk内存块也是有多种不同大小的chunk，如SpecializedChunk, SmallChunk, MediumChunk，分别表示128B、512B、8K大小。本章要做的工作就是在实际分配内存存放元数据前的一切准备工作。
 */
// global_initialize方法用于初始化_first_chunk_word_size，_space_list，_chunk_manager_metadata等静态属性
void Metaspace::global_initialize() {
    // 这一步就是给_capacity_until_GC参数赋值为最大元空间大小：_capacity_until_GC = MaxMetaspaceSize;表示当空间到_capacity_until_GC的值时，就会触发GC操作
  MetaspaceGC::initialize();

  // Initialize the alignment for shared spaces.
    // 初始化共享空间的对齐大小，实际上vm_allocation_granularity()里面就是去拿page size（页大小4*k）
  int max_alignment = os::vm_allocation_granularity();
  size_t cds_total = 0;

    // 设置空间对齐值
  MetaspaceShared::set_max_alignment(max_alignment);

    //DumpSharedSpaces默认为false
    /*
     * DumpSharedSpaces 表示共享空间转储到文件，默认是不开启的，为了减少HotSpot源码的复杂性，这个也按默认false处理，这条线也不走。
     * JVM可以在多个Java进程之间共享公共类元数据，以减少内存使用并缩短启动时间。该共享类数据存储在共享空间。实际生产中也不会这么做，所以可以忽略。如果想开启，可以按如下命令执行：
     * java -XX:+DumpSharedSpaces -XX:SharedArchiveFile=yourSharedSpaces.jsa -jar yourApplication.jar
     */
  if (DumpSharedSpaces) {
#if INCLUDE_CDS
    MetaspaceShared::estimate_regions_size();

    SharedReadOnlySize  = align_size_up(SharedReadOnlySize,  max_alignment);
    SharedReadWriteSize = align_size_up(SharedReadWriteSize, max_alignment);
    SharedMiscDataSize  = align_size_up(SharedMiscDataSize,  max_alignment);
    SharedMiscCodeSize  = align_size_up(SharedMiscCodeSize,  max_alignment);

    // the min_misc_code_size estimate is based on MetaspaceShared::generate_vtable_methods()
    uintx min_misc_code_size = align_size_up(
      (MetaspaceShared::num_virtuals * MetaspaceShared::vtbl_list_size) *
        (sizeof(void*) + MetaspaceShared::vtbl_method_size) + MetaspaceShared::vtbl_common_code_size,
          max_alignment);

    if (SharedMiscCodeSize < min_misc_code_size) {
      report_out_of_shared_space(SharedMiscCode);
    }

    // Initialize with the sum of the shared space sizes.  The read-only
    // and read write metaspace chunks will be allocated out of this and the
    // remainder is the misc code and data chunks.
    cds_total = FileMapInfo::shared_spaces_size();
    cds_total = align_size_up(cds_total, _reserve_alignment);
    _space_list = new VirtualSpaceList(cds_total/wordSize);
    _chunk_manager_metadata = new ChunkManager(SpecializedChunk, SmallChunk, MediumChunk);

    if (!_space_list->initialization_succeeded()) {
      vm_exit_during_initialization("Unable to dump shared archive.", NULL);
    }

#ifdef _LP64
    if (cds_total + compressed_class_space_size() > UnscaledClassSpaceMax) {
      vm_exit_during_initialization("Unable to dump shared archive.",
          err_msg("Size of archive (" SIZE_FORMAT ") + compressed class space ("
                  SIZE_FORMAT ") == total (" SIZE_FORMAT ") is larger than compressed "
                  "klass limit: " SIZE_FORMAT, cds_total, compressed_class_space_size(),
                  cds_total + compressed_class_space_size(), UnscaledClassSpaceMax));
    }

    // Set the compressed klass pointer base so that decoding of these pointers works
    // properly when creating the shared archive.
      // UseCompressedOops和UseCompressedClassPointers表示使用压缩的oop指针和Klass指针，64位下默认为true
    assert(UseCompressedOops && UseCompressedClassPointers,
      "UseCompressedOops and UseCompressedClassPointers must be set");
    Universe::set_narrow_klass_base((address)_space_list->current_virtual_space()->bottom());
    if (TraceMetavirtualspaceAllocation && Verbose) {
      gclog_or_tty->print_cr("Setting_narrow_klass_base to Address: " PTR_FORMAT,
                             _space_list->current_virtual_space()->bottom());
    }

    Universe::set_narrow_klass_shift(0);
#endif // _LP64
#endif // INCLUDE_CDS
  } else {
#if INCLUDE_CDS
    // If using shared space, open the file that contains the shared space
    // and map in the memory before initializing the rest of metaspace (so
    // the addresses don't conflict)
    address cds_address = NULL;
      /*
       * UseSharedSpaces：启用共享存档，这个空间主要是针对热点数据的存储，比如包含元数据、字节码和其他相关信息的共享存档文件。开启命令如下：
       *  java -Xshare:dump -XX:SharedArchiveFile=yourSharedArchive.jsa -jar yourApplication.jar
       *  这块默认是开启的，该文档用于存放CDS数据（类共享数据），该数据用于缩短Java应用程序的启动时间并减少内存占用。CDS的想法是创建一个包含预先计算的数据结构、类元数据和字节码的共享存档文件。该存档可以内存映射mmap到多个Java进程地址空间，允许它们共享公共类和资源（这样省去了JVM启动时对公共类的加载、分配内存等时间和内存空间）。
       */
      // UseSharedSpaces表示使用基于文件的共享Metaspace，即不同的JVM进程通过将Metaspace映射到同一个文件实现Metaspace共享，默认为false
    if (UseSharedSpaces) {
        // 找到那个存档文件，并封装成FileMapInfo
      FileMapInfo* mapinfo = new FileMapInfo();

      // Open the shared archive file, read and validate the header. If
      // initialization fails, shared spaces [UseSharedSpaces] are
      // disabled and the file is closed.
      // Map in spaces now also
        // map_shared_spaces()函数，将存档文件映射到当前Java进程的内存地址空间
      if (mapinfo->initialize() && MetaspaceShared::map_shared_spaces(mapinfo)) {
        cds_total = FileMapInfo::shared_spaces_size();
        cds_address = (address)mapinfo->region_base(0);
      } else {
        assert(!mapinfo->is_open() && !UseSharedSpaces,
               "archive file not closed or shared spaces not disabled.");
      }
    }
#endif // INCLUDE_CDS
      // 64位机器
#ifdef _LP64
    // If UseCompressedClassPointers is set then allocate the metaspace area
    // above the heap and above the CDS area (if it exists).
      //UseCompressedClassPointers为false，DumpSharedSpaces为true时，返回true，64位下默认返回true
    if (using_class_space()) {
      if (UseSharedSpaces) {
#if INCLUDE_CDS
        char* cds_end = (char*)(cds_address + cds_total);
        cds_end = (char *)align_ptr_up(cds_end, _reserve_alignment);
        allocate_metaspace_compressed_klass_ptrs(cds_end, cds_address);
#endif
      } else {
          //以堆内存的终止地址作为起始地址申请内存，避免堆内存与Metaspace的内存地址冲突
        char* base = (char*)align_ptr_up(Universe::heap()->reserved_region().end(), _reserve_alignment);
        allocate_metaspace_compressed_klass_ptrs(base, 0);
      }
    }
#endif // _LP64

    // Initialize these before initializing the VirtualSpaceList
      //计算_first_chunk_word_size和_first_class_chunk_word_size
      // 下面的操作都是内存分配钱的大小对齐和确定
    _first_chunk_word_size = InitialBootClassLoaderMetaspaceSize / BytesPerWord;
    _first_chunk_word_size = align_word_size_up(_first_chunk_word_size);
    // Make the first class chunk bigger than a medium chunk so it's not put
    // on the medium chunk list.   The next chunk will be small and progress
    // from there.  This size calculated by -version.
    _first_class_chunk_word_size = MIN2((size_t)MediumChunk*6,
                                       (CompressedClassSpaceSize/BytesPerWord)*2);
    _first_class_chunk_word_size = align_word_size_up(_first_class_chunk_word_size);
    // Arbitrarily set the initial virtual space to a multiple
    // of the boot class loader size.
      //VIRTUALSPACEMULTIPLIER的取值是2，初始化_space_list和_chunk_manager_metadata
    size_t word_size = VIRTUALSPACEMULTIPLIER * _first_chunk_word_size;
    word_size = align_size_up(word_size, Metaspace::reserve_alignment_words());

    // Initialize the list of virtual spaces.
      // 创建VirtualSpaceList来管理已使用的chunk
    _space_list = new VirtualSpaceList(word_size);
      // 创建ChunkManager来管理空闲的chunk
    _chunk_manager_metadata = new ChunkManager(SpecializedChunk, SmallChunk, MediumChunk);

      //_space_list初始化失败
    if (!_space_list->initialization_succeeded()) {
      vm_exit_during_initialization("Unable to setup metadata virtual space list.", NULL);
    }
  }

  _tracer = new MetaspaceTracer();
}

// post_initialize就调用MetaspaceGC::post_initialize方法
void Metaspace::post_initialize() {
  MetaspaceGC::post_initialize();
}

void Metaspace::initialize_first_chunk(MetaspaceType type, MetadataType mdtype) {
  Metachunk* chunk = get_initialization_chunk(type, mdtype);
  if (chunk != NULL) {
    // Add to this manager's list of chunks in use and current_chunk().
      //chunk分配成功，将其添加到SpaceManager中，将其作为当前使用的Chunk
    get_space_manager(mdtype)->add_chunk(chunk, true);
  }
}

Metachunk* Metaspace::get_initialization_chunk(MetaspaceType type, MetadataType mdtype) {
    //获取初始Chunk的大小
  size_t chunk_word_size = get_space_manager(mdtype)->get_initial_chunk_size(type);

  // Get a chunk from the chunk freelist
    //从ChunkManager管理的空闲Chunk中分配一个满足大小的chunk
  Metachunk* chunk = get_chunk_manager(mdtype)->chunk_freelist_allocate(chunk_word_size);

  if (chunk == NULL) {
      //查找失败从VirtualSpaceList中分配一个新的Chunk
    chunk = get_space_list(mdtype)->get_new_chunk(chunk_word_size,
                                                  get_space_manager(mdtype)->medium_chunk_bunch());
  }

  // For dumping shared archive, report error if allocation has failed.
  if (DumpSharedSpaces && chunk == NULL) {
      //记录分配失败
    report_insufficient_metaspace(MetaspaceAux::committed_bytes() + chunk_word_size * BytesPerWord);
  }

  return chunk;
}

void Metaspace::verify_global_initialization() {
  assert(space_list() != NULL, "Metadata VirtualSpaceList has not been initialized");
  assert(chunk_manager_metadata() != NULL, "Metadata ChunkManager has not been initialized");

  if (using_class_space()) {
    assert(class_space_list() != NULL, "Class VirtualSpaceList has not been initialized");
    assert(chunk_manager_class() != NULL, "Class ChunkManager has not been initialized");
  }
}

void Metaspace::initialize(Mutex* lock, MetaspaceType type) {
    //校验space_list等初始化完成
  verify_global_initialization();

  // Allocate SpaceManager for metadata objects.
    //初始化_vsm和_class_vsm
    // 创建 SpaceManager 对象，分配元数据（主要是符号、字符串等）需要用到
  _vsm = new SpaceManager(NonClassType, lock);

  if (using_class_space()) {
    // Allocate SpaceManager for classes.
      // 创建 SpaceManager 对象，分配类时需要用到
    _class_vsm = new SpaceManager(ClassType, lock);
  } else {
    _class_vsm = NULL;
  }

    //获取锁
  MutexLockerEx cl(SpaceManager::expand_lock(), Mutex::_no_safepoint_check_flag);

  // Allocate chunk for metadata objects
    //初始化第一个Chunk
    // 这一步就是从已分配的空闲chunk链表中找出一个适合当前分配大小（前面讲过，MetaChunk块有三种大小，取一个适合自己的就行）的MetaChunk块对象，并把它添加到 SpaceManager 对象（在这里表示 _vsm）来管理
  initialize_first_chunk(type, NonClassType);

  // Allocate chunk for class metadata objects
    // 同上，这是取出一块MetaChunk来存放类数据，并用 SpaceManager 对象（在这里表示 _class_vsm）管理
  if (using_class_space()) {
    initialize_first_chunk(type, ClassType);
  }

  _alloc_record_head = NULL;
  _alloc_record_tail = NULL;
}

size_t Metaspace::align_word_size_up(size_t word_size) {
  size_t byte_size = word_size * wordSize;
  return ReservedSpace::allocation_align_size_up(byte_size) / wordSize;
}

MetaWord* Metaspace::allocate(size_t word_size, MetadataType mdtype) {
  // DumpSharedSpaces doesn't use class metadata area (yet)
  // Also, don't use class_vsm() unless UseCompressedClassPointers is true.
    //通过SpaceManager分配内存
    // class_vsm() 和 vsm() 取出的都是SpaceManager对象
  if (is_class_space_allocation(mdtype)) {
    return  class_vsm()->allocate(word_size);
  } else {
    return  vsm()->allocate(word_size);
  }
}

// expand_and_allocate方法用于GC结束后尝试扩展Metaspace的空间并从扩展后的Metaspace分配内存
MetaWord* Metaspace::expand_and_allocate(size_t word_size, MetadataType mdtype) {
    //计算允许扩展的空间
  size_t delta_bytes = MetaspaceGC::delta_capacity_until_GC(word_size * BytesPerWord);
  assert(delta_bytes > 0, "Must be");

  size_t before = 0;
  size_t after = 0;
  bool can_retry = true;
  MetaWord* res;
  bool incremented;

  // Each thread increments the HWM at most once. Even if the thread fails to increment
  // the HWM, an allocation is still attempted. This is because another thread must then
  // have incremented the HWM and therefore the allocation might still succeed.
    //扩展失败依然尝试分配内存，因为扩展失败可能是因为其他线程已经完成了扩展
  do {
    incremented = MetaspaceGC::inc_capacity_until_GC(delta_bytes, &after, &before, &can_retry);
    res = allocate(word_size, mdtype);
  } while (!incremented && res == NULL && can_retry);

  if (incremented) {
      //记录扩展成功
    tracer()->report_gc_threshold(before, after,
                                  MetaspaceGCThresholdUpdater::ExpandAndAllocate);
    if (PrintGCDetails && Verbose) {
      gclog_or_tty->print_cr("Increase capacity to GC from " SIZE_FORMAT
          " to " SIZE_FORMAT, before, after);
    }
  }

  return res;
}

// Space allocated in the Metaspace.  This may
// be across several metadata virtual spaces.
char* Metaspace::bottom() const {
  assert(DumpSharedSpaces, "only useful and valid for dumping shared spaces");
  return (char*)vsm()->current_chunk()->bottom();
}

size_t Metaspace::used_words_slow(MetadataType mdtype) const {
  if (mdtype == ClassType) {
    return using_class_space() ? class_vsm()->sum_used_in_chunks_in_use() : 0;
  } else {
    return vsm()->sum_used_in_chunks_in_use();  // includes overhead!
  }
}

size_t Metaspace::free_words_slow(MetadataType mdtype) const {
  if (mdtype == ClassType) {
    return using_class_space() ? class_vsm()->sum_free_in_chunks_in_use() : 0;
  } else {
    return vsm()->sum_free_in_chunks_in_use();
  }
}

// Space capacity in the Metaspace.  It includes
// space in the list of chunks from which allocations
// have been made. Don't include space in the global freelist and
// in the space available in the dictionary which
// is already counted in some chunk.
size_t Metaspace::capacity_words_slow(MetadataType mdtype) const {
  if (mdtype == ClassType) {
    return using_class_space() ? class_vsm()->sum_capacity_in_chunks_in_use() : 0;
  } else {
    return vsm()->sum_capacity_in_chunks_in_use();
  }
}

size_t Metaspace::used_bytes_slow(MetadataType mdtype) const {
  return used_words_slow(mdtype) * BytesPerWord;
}

size_t Metaspace::capacity_bytes_slow(MetadataType mdtype) const {
  return capacity_words_slow(mdtype) * BytesPerWord;
}

size_t Metaspace::allocated_blocks_bytes() const {
  return vsm()->allocated_blocks_bytes() +
      (using_class_space() ? class_vsm()->allocated_blocks_bytes() : 0);
}

size_t Metaspace::allocated_chunks_bytes() const {
  return vsm()->allocated_chunks_bytes() +
      (using_class_space() ? class_vsm()->allocated_chunks_bytes() : 0);
}

void Metaspace::deallocate(MetaWord* ptr, size_t word_size, bool is_class) {
    //如果在安全点
  if (SafepointSynchronize::is_at_safepoint()) {
      //DumpSharedSpaces默认为false
    if (DumpSharedSpaces && PrintSharedSpaces) {
      record_deallocation(ptr, vsm()->get_raw_word_size(word_size));
    }

      //校验必须是VM Thread
    assert(Thread::current()->is_VM_thread(), "should be the VM thread");
    // Don't take Heap_lock
      //获取锁
    MutexLockerEx ml(vsm()->lock(), Mutex::_no_safepoint_check_flag);
      //如果word_size过小则不处理
    if (word_size < TreeChunk<Metablock, FreeList<Metablock> >::min_size()) {
      // Dark matter.  Too small for dictionary.
#ifdef ASSERT
      Copy::fill_to_words((HeapWord*)ptr, word_size, 0xf5f5f5f5);
#endif
      return;
    }
      //通过不同的SpaceManager释放，变成MetaBlock放到block_freelists中重复利用
    if (is_class && using_class_space()) {
      class_vsm()->deallocate(ptr, word_size);
    } else {
      vsm()->deallocate(ptr, word_size);
    }
  } else {
    MutexLockerEx ml(vsm()->lock(), Mutex::_no_safepoint_check_flag);

    if (word_size < TreeChunk<Metablock, FreeList<Metablock> >::min_size()) {
      // Dark matter.  Too small for dictionary.
#ifdef ASSERT
      Copy::fill_to_words((HeapWord*)ptr, word_size, 0xf5f5f5f5);
#endif
      return;
    }
    if (is_class && using_class_space()) {
      class_vsm()->deallocate(ptr, word_size);
    } else {
      vsm()->deallocate(ptr, word_size);
    }
  }
}


// allocate方法用于从Metaspace分配内存，与之对应的有个deallocate方法释放内存，将内存作为MetaBlock放入SpaceManager的放到block_freelists中重复利用。
// 因为Metaspace分配的都是元数据，一般不会被释放，除非对应的ClassLoader被垃圾回收掉了，所以该方法很少被调用，当对应的ClassLoader会垃圾回收掉了，
// 对应的Metaspace的SpaceManager使用的MetaChunk会被整体归还到ChunkManager中重新分配给其他的Metaspace。
MetaWord* Metaspace::allocate(ClassLoaderData* loader_data, size_t word_size,
                              bool read_only, MetaspaceObj::Type type, TRAPS) {
  if (HAS_PENDING_EXCEPTION) {
      //不能有未处理异常
    assert(false, "Should not allocate with exception pending");
    return NULL;  // caller does a CHECK_NULL too
  }

    //loader_data不能为空
  assert(loader_data != NULL, "Should never pass around a NULL loader_data. "
        "ClassLoaderData::the_null_class_loader_data() should have been used.");

  // Allocate in metaspaces without taking out a lock, because it deadlocks
  // with the SymbolTable_lock.  Dumping is single threaded for now.  We'll have
  // to revisit this for application class data sharing.
    // DumpSharedSpaces默认为false
  if (DumpSharedSpaces) {
    assert(type > MetaspaceObj::UnknownType && type < MetaspaceObj::_number_of_types, "sanity");
    Metaspace* space = read_only ? loader_data->ro_metaspace() : loader_data->rw_metaspace();
    MetaWord* result = space->allocate(word_size, NonClassType);
    if (result == NULL) {
      report_out_of_shared_space(read_only ? SharedReadOnly : SharedReadWrite);
    }
    if (PrintSharedSpaces) {
      space->record_allocation(result, type, space->vsm()->get_raw_word_size(word_size));
    }

    // Zero initialize.
    Copy::fill_to_aligned_words((HeapWord*)result, word_size, 0);

    return result;
  }

    // 确定要分配的内存内容的类型，类的创建都是ClassType
  MetadataType mdtype = (type == MetaspaceObj::ClassType) ? ClassType : NonClassType;

  // Try to allocate metadata.
    //获取ClassLoaderData的_metaspace，然后分配内存
    // 尝试分配元数据。loader_data->metaspace_non_null()函数，可以拿到Metaspace的指针指向的元空间对象，然后再通过allocate函数在元空间中分配内存，allocate分配看后面描述
  MetaWord* result = loader_data->metaspace_non_null()->allocate(word_size, mdtype);

  if (result == NULL) {
      //报告分配失败
    tracer()->report_metaspace_allocation_failure(loader_data, word_size, type, mdtype);

    // Allocation failed.
      // 分配失败，可能是内存不足，所以启动GC
    if (is_init_completed()) {
      // Only start a GC if the bootstrapping has completed.

      // Try to clean out some memory and retry.
        //启动完成通过GC释放部分内存，然后尝试重新分配
        // 尝试GC 清空一些内存
      result = Universe::heap()->collector_policy()->satisfy_failed_metadata_allocation(
          loader_data, word_size, mdtype);
    }
  }

    // 启动GC 后还是失败，那就直接报告oom错误
  if (result == NULL) {
      //报告分配失败，会对外抛出异常
    report_metadata_oome(loader_data, word_size, type, mdtype, CHECK_NULL);
  }

  // Zero initialize.
    //将分配的内存初始化成0
  Copy::fill_to_aligned_words((HeapWord*)result, word_size, 0);

    // 返回分配后的内存首地址
  return result;
}

size_t Metaspace::class_chunk_size(size_t word_size) {
  assert(using_class_space(), "Has to use class space");
  return class_vsm()->calc_chunk_size(word_size);
}

void Metaspace::report_metadata_oome(ClassLoaderData* loader_data, size_t word_size, MetaspaceObj::Type type, MetadataType mdtype, TRAPS) {
  tracer()->report_metadata_oom(loader_data, word_size, type, mdtype);

  // If result is still null, we are out of memory.
  if (Verbose && TraceMetadataChunkAllocation) {
    gclog_or_tty->print_cr("Metaspace allocation failed for size "
        SIZE_FORMAT, word_size);
    if (loader_data->metaspace_or_null() != NULL) {
      loader_data->dump(gclog_or_tty);
    }
    MetaspaceAux::dump(gclog_or_tty);
  }

  bool out_of_compressed_class_space = false;
  if (is_class_space_allocation(mdtype)) {
    Metaspace* metaspace = loader_data->metaspace_non_null();
    out_of_compressed_class_space =
      MetaspaceAux::committed_bytes(Metaspace::ClassType) +
      (metaspace->class_chunk_size(word_size) * BytesPerWord) >
      CompressedClassSpaceSize;
  }

  // -XX:+HeapDumpOnOutOfMemoryError and -XX:OnOutOfMemoryError support
  const char* space_string = out_of_compressed_class_space ?
    "Compressed class space" : "Metaspace";

  report_java_out_of_memory(space_string);

  if (JvmtiExport::should_post_resource_exhausted()) {
    JvmtiExport::post_resource_exhausted(
        JVMTI_RESOURCE_EXHAUSTED_OOM_ERROR,
        space_string);
  }

  if (!is_init_completed()) {
    vm_exit_during_initialization("OutOfMemoryError", space_string);
  }

  if (out_of_compressed_class_space) {
    THROW_OOP(Universe::out_of_memory_error_class_metaspace());
  } else {
    THROW_OOP(Universe::out_of_memory_error_metaspace());
  }
}

const char* Metaspace::metadata_type_name(Metaspace::MetadataType mdtype) {
  switch (mdtype) {
    case Metaspace::ClassType: return "Class";
    case Metaspace::NonClassType: return "Metadata";
    default:
      assert(false, err_msg("Got bad mdtype: %d", (int) mdtype));
      return NULL;
  }
}

void Metaspace::record_allocation(void* ptr, MetaspaceObj::Type type, size_t word_size) {
  assert(DumpSharedSpaces, "sanity");

  int byte_size = (int)word_size * HeapWordSize;
  AllocRecord *rec = new AllocRecord((address)ptr, type, byte_size);

  if (_alloc_record_head == NULL) {
    _alloc_record_head = _alloc_record_tail = rec;
  } else if (_alloc_record_tail->_ptr + _alloc_record_tail->_byte_size == (address)ptr) {
    _alloc_record_tail->_next = rec;
    _alloc_record_tail = rec;
  } else {
    // slow linear search, but this doesn't happen that often, and only when dumping
    for (AllocRecord *old = _alloc_record_head; old; old = old->_next) {
      if (old->_ptr == ptr) {
        assert(old->_type == MetaspaceObj::DeallocatedType, "sanity");
        int remain_bytes = old->_byte_size - byte_size;
        assert(remain_bytes >= 0, "sanity");
        old->_type = type;

        if (remain_bytes == 0) {
          delete(rec);
        } else {
          address remain_ptr = address(ptr) + byte_size;
          rec->_ptr = remain_ptr;
          rec->_byte_size = remain_bytes;
          rec->_type = MetaspaceObj::DeallocatedType;
          rec->_next = old->_next;
          old->_byte_size = byte_size;
          old->_next = rec;
        }
        return;
      }
    }
    assert(0, "reallocating a freed pointer that was not recorded");
  }
}

void Metaspace::record_deallocation(void* ptr, size_t word_size) {
  assert(DumpSharedSpaces, "sanity");

  for (AllocRecord *rec = _alloc_record_head; rec; rec = rec->_next) {
    if (rec->_ptr == ptr) {
      assert(rec->_byte_size == (int)word_size * HeapWordSize, "sanity");
      rec->_type = MetaspaceObj::DeallocatedType;
      return;
    }
  }

  assert(0, "deallocating a pointer that was not recorded");
}

void Metaspace::iterate(Metaspace::AllocRecordClosure *closure) {
  assert(DumpSharedSpaces, "unimplemented for !DumpSharedSpaces");

  address last_addr = (address)bottom();

  for (AllocRecord *rec = _alloc_record_head; rec; rec = rec->_next) {
    address ptr = rec->_ptr;
    if (last_addr < ptr) {
      closure->doit(last_addr, MetaspaceObj::UnknownType, ptr - last_addr);
    }
    closure->doit(ptr, rec->_type, rec->_byte_size);
    last_addr = ptr + rec->_byte_size;
  }

  address top = ((address)bottom()) + used_bytes_slow(Metaspace::NonClassType);
  if (last_addr < top) {
    closure->doit(last_addr, MetaspaceObj::UnknownType, top - last_addr);
  }
}

void Metaspace::purge(MetadataType mdtype) {
  get_space_list(mdtype)->purge(get_chunk_manager(mdtype));
}

// purge方法是Metaspace关联的ClassLoaderData因为垃圾回收或者被主动释放时用来释放Metaspace曾经使用过的因为SpaceManager被销毁导致空闲的VirtualSpaceNode
void Metaspace::purge() {
  MutexLockerEx cl(SpaceManager::expand_lock(),
                   Mutex::_no_safepoint_check_flag);
  purge(NonClassType);
  if (using_class_space()) {
    purge(ClassType);
  }
}

void Metaspace::print_on(outputStream* out) const {
  // Print both class virtual space counts and metaspace.
  if (Verbose) {
    vsm()->print_on(out);
    if (using_class_space()) {
      class_vsm()->print_on(out);
    }
  }
}

bool Metaspace::contains(const void* ptr) {
  if (UseSharedSpaces && MetaspaceShared::is_in_shared_space(ptr)) {
    return true;
  }

  if (using_class_space() && get_space_list(ClassType)->contains(ptr)) {
     return true;
  }

  return get_space_list(NonClassType)->contains(ptr);
}

void Metaspace::verify() {
  vsm()->verify();
  if (using_class_space()) {
    class_vsm()->verify();
  }
}

void Metaspace::dump(outputStream* const out) const {
  out->print_cr("\nVirtual space manager: " INTPTR_FORMAT, vsm());
  vsm()->dump(out);
  if (using_class_space()) {
    out->print_cr("\nClass space manager: " INTPTR_FORMAT, class_vsm());
    class_vsm()->dump(out);
  }
}

/////////////// Unit tests ///////////////

#ifndef PRODUCT

class TestMetaspaceAuxTest : AllStatic {
 public:
  static void test_reserved() {
    size_t reserved = MetaspaceAux::reserved_bytes();

    assert(reserved > 0, "assert");

    size_t committed  = MetaspaceAux::committed_bytes();
    assert(committed <= reserved, "assert");

    size_t reserved_metadata = MetaspaceAux::reserved_bytes(Metaspace::NonClassType);
    assert(reserved_metadata > 0, "assert");
    assert(reserved_metadata <= reserved, "assert");

    if (UseCompressedClassPointers) {
      size_t reserved_class    = MetaspaceAux::reserved_bytes(Metaspace::ClassType);
      assert(reserved_class > 0, "assert");
      assert(reserved_class < reserved, "assert");
    }
  }

  static void test_committed() {
    size_t committed = MetaspaceAux::committed_bytes();

    assert(committed > 0, "assert");

    size_t reserved  = MetaspaceAux::reserved_bytes();
    assert(committed <= reserved, "assert");

    size_t committed_metadata = MetaspaceAux::committed_bytes(Metaspace::NonClassType);
    assert(committed_metadata > 0, "assert");
    assert(committed_metadata <= committed, "assert");

    if (UseCompressedClassPointers) {
      size_t committed_class    = MetaspaceAux::committed_bytes(Metaspace::ClassType);
      assert(committed_class > 0, "assert");
      assert(committed_class < committed, "assert");
    }
  }

  static void test_virtual_space_list_large_chunk() {
    VirtualSpaceList* vs_list = new VirtualSpaceList(os::vm_allocation_granularity());
    MutexLockerEx cl(SpaceManager::expand_lock(), Mutex::_no_safepoint_check_flag);
    // A size larger than VirtualSpaceSize (256k) and add one page to make it _not_ be
    // vm_allocation_granularity aligned on Windows.
    size_t large_size = (size_t)(2*256*K + (os::vm_page_size()/BytesPerWord));
    large_size += (os::vm_page_size()/BytesPerWord);
    vs_list->get_new_chunk(large_size, 0);
  }

  static void test() {
    test_reserved();
    test_committed();
    test_virtual_space_list_large_chunk();
  }
};

void TestMetaspaceAux_test() {
  TestMetaspaceAuxTest::test();
}

class TestVirtualSpaceNodeTest {
  static void chunk_up(size_t words_left, size_t& num_medium_chunks,
                                          size_t& num_small_chunks,
                                          size_t& num_specialized_chunks) {
    num_medium_chunks = words_left / MediumChunk;
    words_left = words_left % MediumChunk;

    num_small_chunks = words_left / SmallChunk;
    words_left = words_left % SmallChunk;
    // how many specialized chunks can we get?
    num_specialized_chunks = words_left / SpecializedChunk;
    assert(words_left % SpecializedChunk == 0, "should be nothing left");
  }

 public:
  static void test() {
    MutexLockerEx ml(SpaceManager::expand_lock(), Mutex::_no_safepoint_check_flag);
    const size_t vsn_test_size_words = MediumChunk  * 4;
    const size_t vsn_test_size_bytes = vsn_test_size_words * BytesPerWord;

    // The chunk sizes must be multiples of eachother, or this will fail
    STATIC_ASSERT(MediumChunk % SmallChunk == 0);
    STATIC_ASSERT(SmallChunk % SpecializedChunk == 0);

    { // No committed memory in VSN
      ChunkManager cm(SpecializedChunk, SmallChunk, MediumChunk);
      VirtualSpaceNode vsn(vsn_test_size_bytes);
      vsn.initialize();
      vsn.retire(&cm);
      assert(cm.sum_free_chunks_count() == 0, "did not commit any memory in the VSN");
    }

    { // All of VSN is committed, half is used by chunks
      ChunkManager cm(SpecializedChunk, SmallChunk, MediumChunk);
      VirtualSpaceNode vsn(vsn_test_size_bytes);
      vsn.initialize();
      vsn.expand_by(vsn_test_size_words, vsn_test_size_words);
      vsn.get_chunk_vs(MediumChunk);
      vsn.get_chunk_vs(MediumChunk);
      vsn.retire(&cm);
      assert(cm.sum_free_chunks_count() == 2, "should have been memory left for 2 medium chunks");
      assert(cm.sum_free_chunks() == 2*MediumChunk, "sizes should add up");
    }

    const size_t page_chunks = 4 * (size_t)os::vm_page_size() / BytesPerWord;
    // This doesn't work for systems with vm_page_size >= 16K.
    if (page_chunks < MediumChunk) {
      // 4 pages of VSN is committed, some is used by chunks
      ChunkManager cm(SpecializedChunk, SmallChunk, MediumChunk);
      VirtualSpaceNode vsn(vsn_test_size_bytes);

      vsn.initialize();
      vsn.expand_by(page_chunks, page_chunks);
      vsn.get_chunk_vs(SmallChunk);
      vsn.get_chunk_vs(SpecializedChunk);
      vsn.retire(&cm);

      // committed - used = words left to retire
      const size_t words_left = page_chunks - SmallChunk - SpecializedChunk;

      size_t num_medium_chunks, num_small_chunks, num_spec_chunks;
      chunk_up(words_left, num_medium_chunks, num_small_chunks, num_spec_chunks);

      assert(num_medium_chunks == 0, "should not get any medium chunks");
      assert(cm.sum_free_chunks_count() == (num_small_chunks + num_spec_chunks), "should be space for 3 chunks");
      assert(cm.sum_free_chunks() == words_left, "sizes should add up");
    }

    { // Half of VSN is committed, a humongous chunk is used
      ChunkManager cm(SpecializedChunk, SmallChunk, MediumChunk);
      VirtualSpaceNode vsn(vsn_test_size_bytes);
      vsn.initialize();
      vsn.expand_by(MediumChunk * 2, MediumChunk * 2);
      vsn.get_chunk_vs(MediumChunk + SpecializedChunk); // Humongous chunks will be aligned up to MediumChunk + SpecializedChunk
      vsn.retire(&cm);

      const size_t words_left = MediumChunk * 2 - (MediumChunk + SpecializedChunk);
      size_t num_medium_chunks, num_small_chunks, num_spec_chunks;
      chunk_up(words_left, num_medium_chunks, num_small_chunks, num_spec_chunks);

      assert(num_medium_chunks == 0, "should not get any medium chunks");
      assert(cm.sum_free_chunks_count() == (num_small_chunks + num_spec_chunks), "should be space for 3 chunks");
      assert(cm.sum_free_chunks() == words_left, "sizes should add up");
    }

  }

#define assert_is_available_positive(word_size) \
  assert(vsn.is_available(word_size), \
    err_msg(#word_size ": " PTR_FORMAT " bytes were not available in " \
            "VirtualSpaceNode [" PTR_FORMAT ", " PTR_FORMAT ")", \
            (uintptr_t)(word_size * BytesPerWord), vsn.bottom(), vsn.end()));

#define assert_is_available_negative(word_size) \
  assert(!vsn.is_available(word_size), \
    err_msg(#word_size ": " PTR_FORMAT " bytes should not be available in " \
            "VirtualSpaceNode [" PTR_FORMAT ", " PTR_FORMAT ")", \
            (uintptr_t)(word_size * BytesPerWord), vsn.bottom(), vsn.end()));

  static void test_is_available_positive() {
    // Reserve some memory.
    VirtualSpaceNode vsn(os::vm_allocation_granularity());
    assert(vsn.initialize(), "Failed to setup VirtualSpaceNode");

    // Commit some memory.
    size_t commit_word_size = os::vm_allocation_granularity() / BytesPerWord;
    bool expanded = vsn.expand_by(commit_word_size, commit_word_size);
    assert(expanded, "Failed to commit");

    // Check that is_available accepts the committed size.
    assert_is_available_positive(commit_word_size);

    // Check that is_available accepts half the committed size.
    size_t expand_word_size = commit_word_size / 2;
    assert_is_available_positive(expand_word_size);
  }

  static void test_is_available_negative() {
    // Reserve some memory.
    VirtualSpaceNode vsn(os::vm_allocation_granularity());
    assert(vsn.initialize(), "Failed to setup VirtualSpaceNode");

    // Commit some memory.
    size_t commit_word_size = os::vm_allocation_granularity() / BytesPerWord;
    bool expanded = vsn.expand_by(commit_word_size, commit_word_size);
    assert(expanded, "Failed to commit");

    // Check that is_available doesn't accept a too large size.
    size_t two_times_commit_word_size = commit_word_size * 2;
    assert_is_available_negative(two_times_commit_word_size);
  }

  static void test_is_available_overflow() {
    // Reserve some memory.
    VirtualSpaceNode vsn(os::vm_allocation_granularity());
    assert(vsn.initialize(), "Failed to setup VirtualSpaceNode");

    // Commit some memory.
    size_t commit_word_size = os::vm_allocation_granularity() / BytesPerWord;
    bool expanded = vsn.expand_by(commit_word_size, commit_word_size);
    assert(expanded, "Failed to commit");

    // Calculate a size that will overflow the virtual space size.
    void* virtual_space_max = (void*)(uintptr_t)-1;
    size_t bottom_to_max = pointer_delta(virtual_space_max, vsn.bottom(), 1);
    size_t overflow_size = bottom_to_max + BytesPerWord;
    size_t overflow_word_size = overflow_size / BytesPerWord;

    // Check that is_available can handle the overflow.
    assert_is_available_negative(overflow_word_size);
  }

  static void test_is_available() {
    TestVirtualSpaceNodeTest::test_is_available_positive();
    TestVirtualSpaceNodeTest::test_is_available_negative();
    TestVirtualSpaceNodeTest::test_is_available_overflow();
  }
};

void TestVirtualSpaceNode_test() {
  TestVirtualSpaceNodeTest::test();
  TestVirtualSpaceNodeTest::test_is_available();
}

// The following test is placed here instead of a gtest / unittest file
// because the ChunkManager class is only available in this file.
class SpaceManagerTest : AllStatic {
  friend void SpaceManager_test_adjust_initial_chunk_size();

  static void test_adjust_initial_chunk_size(bool is_class) {
    const size_t smallest = SpaceManager::smallest_chunk_size(is_class);
    const size_t normal   = SpaceManager::small_chunk_size(is_class);
    const size_t medium   = SpaceManager::medium_chunk_size(is_class);

#define test_adjust_initial_chunk_size(value, expected, is_class_value)          \
    do {                                                                         \
      size_t v = value;                                                          \
      size_t e = expected;                                                       \
      assert(SpaceManager::adjust_initial_chunk_size(v, (is_class_value)) == e,  \
             err_msg("Expected: " SIZE_FORMAT " got: " SIZE_FORMAT, e, v));      \
    } while (0)

    // Smallest (specialized)
    test_adjust_initial_chunk_size(1,            smallest, is_class);
    test_adjust_initial_chunk_size(smallest - 1, smallest, is_class);
    test_adjust_initial_chunk_size(smallest,     smallest, is_class);

    // Small
    test_adjust_initial_chunk_size(smallest + 1, normal, is_class);
    test_adjust_initial_chunk_size(normal - 1,   normal, is_class);
    test_adjust_initial_chunk_size(normal,       normal, is_class);

    // Medium
    test_adjust_initial_chunk_size(normal + 1, medium, is_class);
    test_adjust_initial_chunk_size(medium - 1, medium, is_class);
    test_adjust_initial_chunk_size(medium,     medium, is_class);

    // Humongous
    test_adjust_initial_chunk_size(medium + 1, medium + 1, is_class);

#undef test_adjust_initial_chunk_size
  }

  static void test_adjust_initial_chunk_size() {
    test_adjust_initial_chunk_size(false);
    test_adjust_initial_chunk_size(true);
  }
};

void SpaceManager_test_adjust_initial_chunk_size() {
  SpaceManagerTest::test_adjust_initial_chunk_size();
}

// The following test is placed here instead of a gtest / unittest file
// because the ChunkManager class is only available in this file.
void ChunkManager_test_list_index() {
  ChunkManager manager(ClassSpecializedChunk, ClassSmallChunk, ClassMediumChunk);

  // Test previous bug where a query for a humongous class metachunk,
  // incorrectly matched the non-class medium metachunk size.
  {
    assert(MediumChunk > ClassMediumChunk, "Precondition for test");

    ChunkIndex index = manager.list_index(MediumChunk);

    assert(index == HumongousIndex,
           err_msg("Requested size is larger than ClassMediumChunk,"
           " so should return HumongousIndex. Got index: %d", (int)index));
  }

  // Check the specified sizes as well.
  {
    ChunkIndex index = manager.list_index(ClassSpecializedChunk);
    assert(index == SpecializedIndex, err_msg("Wrong index returned. Got index: %d", (int)index));
  }
  {
    ChunkIndex index = manager.list_index(ClassSmallChunk);
    assert(index == SmallIndex, err_msg("Wrong index returned. Got index: %d", (int)index));
  }
  {
    ChunkIndex index = manager.list_index(ClassMediumChunk);
    assert(index == MediumIndex, err_msg("Wrong index returned. Got index: %d", (int)index));
  }
  {
    ChunkIndex index = manager.list_index(ClassMediumChunk + 1);
    assert(index == HumongousIndex, err_msg("Wrong index returned. Got index: %d", (int)index));
  }
}

#endif
