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
#include "code/codeBlob.hpp"
#include "code/stubs.hpp"
#include "memory/allocation.inline.hpp"
#include "oops/oop.inline.hpp"
#include "runtime/mutexLocker.hpp"


// Implementation of StubQueue
//
// Standard wrap-around queue implementation; the queue dimensions
// are specified by the _queue_begin & _queue_end indices. The queue
// can be in two states (transparent to the outside):
//
// a) contiguous state: all queue entries in one block (or empty)
//
// Queue: |...|XXXXXXX|...............|
//        ^0  ^begin  ^end            ^size = limit
//            |_______|
//            one block
//
// b) non-contiguous state: queue entries in two blocks
//
// Queue: |XXX|.......|XXXXXXX|.......|
//        ^0  ^end    ^begin  ^limit  ^size
//        |___|       |_______|
//         1st block  2nd block
//
// In the non-contiguous state, the wrap-around point is
// indicated via the _buffer_limit index since the last
// queue entry may not fill up the queue completely in
// which case we need to know where the 2nd block's end
// is to do the proper wrap-around. When removing the
// last entry of the 2nd block, _buffer_limit is reset
// to _buffer_size.
//
// CAUTION: DO NOT MESS WITH THIS CODE IF YOU CANNOT PROVE
// ITS CORRECTNESS! THIS CODE IS MORE SUBTLE THAN IT LOOKS!


StubQueue::StubQueue(StubInterface* stub_interface, int buffer_size,
                     Mutex* lock, const char* name) : _mutex(lock) {
    //64位下BytesPerWord等于8，round_to方法将buffer_size取整，必须是后者的整数倍
  intptr_t size = round_to(buffer_size, 2*BytesPerWord);
    //分配一个BufferBlob
  BufferBlob* blob = BufferBlob::create(name, size);
  if( blob == NULL) {
      //分配Blob失败则抛出异常
    vm_exit_out_of_memory(size, OOM_MALLOC_ERROR, err_msg("CodeCache: no room for %s", name));
  }
  _stub_interface  = stub_interface;
  _buffer_size     = blob->content_size();
  _buffer_limit    = blob->content_size();
  _stub_buffer     = blob->content_begin();
  _queue_begin     = 0;
  _queue_end       = 0;
  _number_of_stubs = 0;
    //将新创建的StubQueue保存到全局的StubQueue数组中
  register_queue(this);
}


StubQueue::~StubQueue() {
  // Note: Currently StubQueues are never destroyed so nothing needs to be done here.
  //       If we want to implement the destructor, we need to release the BufferBlob
  //       allocated in the constructor (i.e., we need to keep it around or look it
  //       up via CodeCache::find_blob(...).
    // 当前实现下StubQueues永远不会自动销毁，因为它通过全局静态StubQueues数组和静态属性关联起来了
  Unimplemented();
}


Stub* StubQueue::stub_containing(address pc) const {
  if (contains(pc)) {
    for (Stub* s = first(); s != NULL; s = next(s)) {
      if (stub_contains(s, pc)) return s;
    }
  }
  return NULL;
}


Stub* StubQueue::request_committed(int code_size) {
  Stub* s = request(code_size);
  CodeStrings strings;
  if (s != NULL) commit(code_size, strings);
  return s;
}


Stub* StubQueue::request(int requested_code_size) {
  assert(requested_code_size > 0, "requested_code_size must be > 0");
    //获取锁
  if (_mutex != NULL) _mutex->lock();
    //current_stub返回最后一个Stub
  Stub* s = current_stub();
    //requested_size取整
  int requested_size = round_to(stub_code_size_to_size(requested_code_size), CodeEntryAlignment);
    //如果可用空间充足
  if (requested_size <= available_space()) {
      //如果是连续状态，即所有Stub在一个Blob中分配
    if (is_contiguous()) {
      // Queue: |...|XXXXXXX|.............|
      //        ^0  ^begin  ^end          ^size = limit
        //校验其是连续状态
      assert(_buffer_limit == _buffer_size, "buffer must be fully usable");
        //校验可用空间是否充足
      if (_queue_end + requested_size <= _buffer_size) {
        // code fits in at the end => nothing to do
        CodeStrings strings;
          //Stub的初始化
        stub_initialize(s, requested_size, strings);
        return s;
      } else {
        // stub doesn't fit in at the queue end
        // => reduce buffer limit & wrap around
          //校验是否非空
        assert(!is_empty(), "just checkin'");
          //将_buffer_limit置为_queue_end，_queue_end置0，即将连续状态变成非连续状态
        _buffer_limit = _queue_end;
        _queue_end = 0;
      }
    }
  }
  if (requested_size <= available_space()) {
      //校验其是非连续状态
    assert(!is_contiguous(), "just checkin'");
    assert(_buffer_limit <= _buffer_size, "queue invariant broken");
    // Queue: |XXX|.......|XXXXXXX|.......|
    //        ^0  ^end    ^begin  ^limit  ^size
      //获取当前的Stub
    s = current_stub();
    CodeStrings strings;
      //初始化
    stub_initialize(s, requested_size, strings);
    return s;
  }
  // Not enough space left
    //没有足够的剩余空间，解锁
  if (_mutex != NULL) _mutex->unlock();
  return NULL;
}


void StubQueue::commit(int committed_code_size, CodeStrings& strings) {
  assert(committed_code_size > 0, "committed_code_size must be > 0");
    //committed_code_size取整
  int committed_size = round_to(stub_code_size_to_size(committed_code_size), CodeEntryAlignment);
  Stub* s = current_stub();
    //校验committed_size必须小于等于s的大小
  assert(committed_size <= stub_size(s), "committed size must not exceed requested size");
    //初始化
  stub_initialize(s, committed_size, strings);
    //_queue_end增加，_number_of_stubs增加
  _queue_end += committed_size;
  _number_of_stubs++;
    //解锁
  if (_mutex != NULL) _mutex->unlock();
  debug_only(stub_verify(s);)
}


void StubQueue::remove_first() {
    //StubQueue为空
  if (number_of_stubs() == 0) return;
    //获取_queue_begin对应的Stub
  Stub* s = first();
  debug_only(stub_verify(s);)
  stub_finalize(s);
    //_queue_begin往前移动
  _queue_begin += stub_size(s);
    //校验_queue_begin必须小于_buffer_limit
  assert(_queue_begin <= _buffer_limit, "sanity check");
  if (_queue_begin == _queue_end) {
    // buffer empty
    // => reset queue indices
      //StubQueue清空了，将其重置，此时队列处于连续状态下
    _queue_begin  = 0;
    _queue_end    = 0;
    _buffer_limit = _buffer_size;
  } else if (_queue_begin == _buffer_limit) {
    // buffer limit reached
    // => reset buffer limit & wrap around
      // 此时队列处于非连续状态下，即begin到_buffer_limit之间的部分被清空了，重置恢复到连续状态
    _buffer_limit = _buffer_size;
    _queue_begin = 0;
  }
    //将个数减一
  _number_of_stubs--;
}


void StubQueue::remove_first(int n) {
    //取出stubs数量和n的最小值
  int i = MIN2(n, number_of_stubs());
    //不断遍历，移除第一个stub
  while (i-- > 0) remove_first();
}


void StubQueue::remove_all(){
  debug_only(verify();)
    //将所有的stub都删除
  remove_first(number_of_stubs());
  assert(number_of_stubs() == 0, "sanity check");
}


enum { StubQueueLimit = 10 };  // there are only a few in the world
static StubQueue* registered_stub_queues[StubQueueLimit];

void StubQueue::register_queue(StubQueue* sq) {
  for (int i = 0; i < StubQueueLimit; i++) {
      //找一个空的数组元素保存sq
    if (registered_stub_queues[i] == NULL) {
      registered_stub_queues[i] = sq;
      return;
    }
  }
  ShouldNotReachHere();
}


// 让所有的StubQueue执行某个函数
void StubQueue::queues_do(void f(StubQueue* sq)) {
    //遍历全局的registered_stub_queues数组
  for (int i = 0; i < StubQueueLimit; i++) {
    if (registered_stub_queues[i] != NULL) {
      f(registered_stub_queues[i]);
    }
  }
}


// 让所有的Stub执行某个函数
void StubQueue::stubs_do(void f(Stub* s)) {
  debug_only(verify();)
    //获取锁
  MutexLockerEx lock(_mutex);
    //逐一遍历所有的Stub
  for (Stub* s = first(); s != NULL; s = next(s)) f(s);
}


void StubQueue::verify() {
  // verify only if initialized
  if (_stub_buffer == NULL) return;
  MutexLockerEx lock(_mutex);
  // verify index boundaries
  guarantee(0 <= _buffer_size, "buffer size must be positive");
  guarantee(0 <= _buffer_limit && _buffer_limit <= _buffer_size , "_buffer_limit out of bounds");
  guarantee(0 <= _queue_begin  && _queue_begin  <  _buffer_limit, "_queue_begin out of bounds");
  guarantee(0 <= _queue_end    && _queue_end    <= _buffer_limit, "_queue_end   out of bounds");
  // verify alignment
  guarantee(_buffer_size  % CodeEntryAlignment == 0, "_buffer_size  not aligned");
  guarantee(_buffer_limit % CodeEntryAlignment == 0, "_buffer_limit not aligned");
  guarantee(_queue_begin  % CodeEntryAlignment == 0, "_queue_begin  not aligned");
  guarantee(_queue_end    % CodeEntryAlignment == 0, "_queue_end    not aligned");
  // verify buffer limit/size relationship
  if (is_contiguous()) {
    guarantee(_buffer_limit == _buffer_size, "_buffer_limit must equal _buffer_size");
  }
  // verify contents
  int n = 0;
  for (Stub* s = first(); s != NULL; s = next(s)) {
    stub_verify(s);
    n++;
  }
  guarantee(n == number_of_stubs(), "number of stubs inconsistent");
  guarantee(_queue_begin != _queue_end || n == 0, "buffer indices must be the same");
}


void StubQueue::print() {
  MutexLockerEx lock(_mutex);
  for (Stub* s = first(); s != NULL; s = next(s)) {
    stub_print(s);
  }
}
