/*
 * Copyright (c) 1997, 2014, Oracle and/or its affiliates. All rights reserved.
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
#include "memory/allocation.inline.hpp"
#include "oops/constantPool.hpp"
#include "oops/oop.inline.hpp"
#include "runtime/handles.inline.hpp"
#include "runtime/thread.inline.hpp"
#ifdef TARGET_OS_FAMILY_linux
# include "os_linux.inline.hpp"
#endif
#ifdef TARGET_OS_FAMILY_solaris
# include "os_solaris.inline.hpp"
#endif
#ifdef TARGET_OS_FAMILY_windows
# include "os_windows.inline.hpp"
#endif
#ifdef TARGET_OS_FAMILY_bsd
# include "os_bsd.inline.hpp"
#endif

PRAGMA_FORMAT_MUTE_WARNINGS_FOR_GCC

#ifdef ASSERT
oop* HandleArea::allocate_handle(oop obj) {
  assert(_handle_mark_nesting > 1, "memory leak: allocating handle outside HandleMark");
  assert(_no_handle_mark_nesting == 0, "allocating handle inside NoHandleMark");
  assert(obj->is_oop(), err_msg("not an oop: " INTPTR_FORMAT, (intptr_t*) obj));
  return real_allocate_handle(obj);
}

Handle::Handle(Thread* thread, oop obj) {
  assert(thread == Thread::current(), "sanity check");
  if (obj == NULL) {
    _handle = NULL;
  } else {
    _handle = thread->handle_area()->allocate_handle(obj);
  }
}

#endif

static uintx chunk_oops_do(OopClosure* f, Chunk* chunk, char* chunk_top) {
  oop* bottom = (oop*) chunk->bottom();
  oop* top    = (oop*) chunk_top;
  uintx handles_visited = top - bottom;
  assert(top >= bottom && top <= (oop*) chunk->top(), "just checking");
  // during GC phase 3, a handle may be a forward pointer that
  // is not yet valid, so loosen the assertion
  while (bottom < top) {
    // This test can be moved up but for now check every oop.

    // JFR is known to set mark word to 0 for duration of leak analysis VM operaiton
    assert((*bottom)->is_oop(INCLUDE_JFR), "handle should point to oop");

    f->do_oop(bottom++);
  }
  return handles_visited;
}

// Used for debugging handle allocation.
NOT_PRODUCT(jint _nof_handlemarks  = 0;)

void HandleArea::oops_do(OopClosure* f) {
  uintx handles_visited = 0;
  // First handle the current chunk. It is filled to the high water mark.
  handles_visited += chunk_oops_do(f, _chunk, _hwm);
  // Then handle all previous chunks. They are completely filled.
  Chunk* k = _first;
  while(k != _chunk) {
    handles_visited += chunk_oops_do(f, k, k->top());
    k = k->next();
  }

  // The thread local handle areas should not get very large
  if (TraceHandleAllocation && handles_visited > TotalHandleAllocationLimit) {
#ifdef ASSERT
    warning("%d: Visited in HandleMark : %d",
      _nof_handlemarks, handles_visited);
#else
    warning("Visited in HandleMark : %d", handles_visited);
#endif
  }
  if (_prev != NULL) _prev->oops_do(f);
}

/**
 * 参考HandleArea等的描述可知，创建一个新的HandleMark以后，新的HandleMark保存当前线程的area的当前chunk，_hwm ，_max等属性，
 * 执行方法期间新创建的Handle实例是在当前线程的area中分配内存，这会导致当前线程的area的当前chunk，_hwm ，_max等属性发生变更，
 * 因此方法执行完成需要将这些属性恢复成方法调用前的状态，并把方法调用过程中新创建的Handle实例的内存给释放掉。
 * @param thread
 */
void HandleMark::initialize(Thread* thread) {
    //将当前线程的_area的属性保存到新的HandleMark实例中
  _thread = thread;
  // Save area 保存当前线程的area
  _area  = thread->handle_area();
  // Save current top 保存当前线程的area的相关属性
  _chunk = _area->_chunk;
  _hwm   = _area->_hwm;
  _max   = _area->_max;
  _size_in_bytes = _area->_size_in_bytes;
  debug_only(_area->_handle_mark_nesting++);
  assert(_area->_handle_mark_nesting > 0, "must stack allocate HandleMarks");
  debug_only(Atomic::inc(&_nof_handlemarks);)

  // Link this in the thread
    //将当前HandleMark实例同线程关联起来
  set_previous_handle_mark(thread->last_handle_mark());
  thread->set_last_handle_mark(this);
}


HandleMark::~HandleMark() {
  HandleArea* area = _area;   // help compilers with poor alias analysis
    //检查当前线程的HandleArea是否改变
  assert(area == _thread->handle_area(), "sanity check");
  assert(area->_handle_mark_nesting > 0, "must stack allocate HandleMarks" );
  debug_only(area->_handle_mark_nesting--);

  // Debug code to trace the number of handles allocated per mark/
#ifdef ASSERT
  if (TraceHandleAllocation) {
    size_t handles = 0;
    Chunk *c = _chunk->next();
    if (c == NULL) {
      handles = area->_hwm - _hwm; // no new chunk allocated
    } else {
      handles = _max - _hwm;      // add rest in first chunk
      while(c != NULL) {
        handles += c->length();
        c = c->next();
      }
      handles -= area->_max - area->_hwm; // adjust for last trunk not full
    }
    handles /= sizeof(void *); // Adjust for size of a handle
    if (handles > HandleAllocationLimit) {
      // Note: _nof_handlemarks is only set in debug mode
      warning("%d: Allocated in HandleMark : %d", _nof_handlemarks, handles);
    }

    tty->print_cr("Handles %d", handles);
  }
#endif

  // Delete later chunks
    //如果存在下一个Chunk
  if( _chunk->next() ) {
    // reset arena size before delete chunks. Otherwise, the total
    // arena size could exceed total chunk size
    //校验area当前的大小大于HandleMark在构造函数中保存的大小
    assert(area->size_in_bytes() > size_in_bytes(), "Sanity check");
      //恢复area的大小到HandleMark构造时的状态
    area->set_size_in_bytes(size_in_bytes());
      //删除当前Chunk以后的所有Chunk，即在方法调用期间新创建的Chunk
    _chunk->next_chop();
  } else {
      //如果没有下一个Chunk，说明未分配新的Chunk，则area的大小应该保持不变
    assert(area->size_in_bytes() == size_in_bytes(), "Sanity check");
  }
  // Roll back arena to saved top markers
    //恢复area的属性到HandleMark构造时的状态
  area->_chunk = _chunk;
  area->_hwm = _hwm;
  area->_max = _max;
#ifdef ASSERT
  // clear out first chunk (to detect allocation bugs)
  if (ZapVMHandleArea) {
    memset(_hwm, badHandleValue, _max - _hwm);
  }
  Atomic::dec(&_nof_handlemarks);
#endif

  // Unlink this from the thread
    //解除当前HandleMark跟线程的关联
  _thread->set_last_handle_mark(previous_handle_mark());
}

void* HandleMark::operator new(size_t size) throw() {
  return AllocateHeap(size, mtThread);
}

void* HandleMark::operator new [] (size_t size) throw() {
  return AllocateHeap(size, mtThread);
}

void HandleMark::operator delete(void* p) {
  FreeHeap(p, mtThread);
}

void HandleMark::operator delete[](void* p) {
  FreeHeap(p, mtThread);
}

#ifdef ASSERT

NoHandleMark::NoHandleMark() {
  HandleArea* area = Thread::current()->handle_area();
  area->_no_handle_mark_nesting++;
  assert(area->_no_handle_mark_nesting > 0, "must stack allocate NoHandleMark" );
}


NoHandleMark::~NoHandleMark() {
  HandleArea* area = Thread::current()->handle_area();
  assert(area->_no_handle_mark_nesting > 0, "must stack allocate NoHandleMark" );
  area->_no_handle_mark_nesting--;
}


ResetNoHandleMark::ResetNoHandleMark() {
  HandleArea* area = Thread::current()->handle_area();
  _no_handle_mark_nesting = area->_no_handle_mark_nesting;
  area->_no_handle_mark_nesting = 0;
}


ResetNoHandleMark::~ResetNoHandleMark() {
  HandleArea* area = Thread::current()->handle_area();
  area->_no_handle_mark_nesting = _no_handle_mark_nesting;
}

#endif
