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

#ifndef SHARE_VM_CODE_STUBS_HPP
#define SHARE_VM_CODE_STUBS_HPP

#include "asm/codeBuffer.hpp"
#include "memory/allocation.hpp"
#ifdef TARGET_OS_FAMILY_linux
# include "os_linux.inline.hpp"
#endif
#ifdef TARGET_OS_FAMILY_solaris
# include "os_solaris.inline.hpp"
#endif
#ifdef TARGET_OS_FAMILY_windows
# include "os_windows.inline.hpp"
#endif
#ifdef TARGET_OS_FAMILY_aix
# include "os_aix.inline.hpp"
#endif
#ifdef TARGET_OS_FAMILY_bsd
# include "os_bsd.inline.hpp"
#endif

// The classes in this file provide a simple framework for the
// management of little pieces of machine code - or stubs -
// created on the fly and frequently discarded. In this frame-
// work stubs are stored in a queue.


// Stub serves as abstract base class. A concrete stub
// implementation is a subclass of Stub, implementing
// all (non-virtual!) functions required sketched out
// in the Stub class.
//
// A concrete stub layout may look like this (both data
// and code sections could be empty as well):
//
//                ________
// stub       -->|        | <--+
//               |  data  |    |
//               |________|    |
// code_begin -->|        |    |
//               |        |    |
//               |  code  |    | size
//               |        |    |
//               |________|    |
// code_end   -->|        |    |
//               |  data  |    |
//               |________|    |
//                          <--+

// Stub只定义了基础方法，没有任何属性，且所有方法的实现都是ShouldNotCallThis()，即要求子类实现所有的方法，其定义在hotspot/src/share/vm/code/stubs.hpp中
// 什么是Stub？
// Stub代码是HotSpot生成的固定调用点的代码。为什么需要Stub代码，HotSpot内部与Java代码调用的地方有两种形式JNI和Stub。
// JNI调用方式需要Java代码与JNI代码一一对应，每一个Java方法都对应一个JNI函数。而Stub是HosSpot内部为了统一调用Java函数而生成的固定调用点。
// 通过手工汇编编写的一段存储于内存中的统一调用点。HotSpot内部按Java方法功能类别生成了多个调用点的Stub代码。
// 当虚拟机执行到一个Java方法调用时，会统一转到合适的Stub调用点。该调用点会进行栈帧创建，参数传递处理，大大简化了设计。
// 比如， JavaCalls::call_virtual()就是Stub调用的一个用例
class Stub VALUE_OBJ_CLASS_SPEC {
 public:
  // Initialization/finalization
  void    initialize(int size,
                     CodeStrings& strings)       { ShouldNotCallThis(); }                // called to initialize/specify the stub's size
  void    finalize()                             { ShouldNotCallThis(); }                // called before the stub is deallocated

  // General info/converters
  int     size() const                           { ShouldNotCallThis(); return 0; }      // must return the size provided by initialize
  static  int code_size_to_size(int code_size)   { ShouldNotCallThis(); return 0; }      // computes the size given the code size

  // Code info
  address code_begin() const                     { ShouldNotCallThis(); return NULL; }   // points to the first byte of    the code
  address code_end() const                       { ShouldNotCallThis(); return NULL; }   // points to the first byte after the code

  // Debugging
  void    verify()                               { ShouldNotCallThis(); }                // verifies the Stub
  void    print()                                { ShouldNotCallThis(); }                // prints some information about the stub
};


// A stub interface defines the interface between a stub queue
// and the stubs it queues. In order to avoid a vtable and
// (and thus the extra word) in each stub, a concrete stub
// interface object is created and associated with a stub
// buffer which in turn uses the stub interface to interact
// with its stubs.
//
// StubInterface serves as an abstract base class. A concrete
// stub interface implementation is a subclass of StubInterface,
// forwarding its virtual function calls to non-virtual calls
// of the concrete stub (see also macro below). There's exactly
// one stub interface instance required per stub queue.
// StubInterface的定义在stubs.hpp中，StubInterface定义了stub和保存stub的StubQueue之间调用接口，为了避免在Stub中使用虚函数表，
// 每个StubQueue都保存了一个关联的StubInterface的实例，通过这个StubInterface的实例来调用Stub的对应方法。
//
//  StubInterface定义的都是虚方法，且不提供空实现
class StubInterface: public CHeapObj<mtCode> {
 public:
  // Initialization/finalization
  virtual void    initialize(Stub* self, int size,
                             CodeStrings& strings)         = 0; // called after creation (called twice if allocated via (request, commit))
  virtual void    finalize(Stub* self)                     = 0; // called before deallocation

  // General info/converters
  virtual int     size(Stub* self) const                   = 0; // the total size of the stub in bytes (must be a multiple of CodeEntryAlignment)
  virtual int     code_size_to_size(int code_size) const   = 0; // computes the total stub size in bytes given the code size in bytes

  // Code info
  virtual address code_begin(Stub* self) const             = 0; // points to the first code byte
  virtual address code_end(Stub* self) const               = 0; // points to the first byte after the code

  // Debugging
  virtual void    verify(Stub* self)                       = 0; // verifies the stub
  virtual void    print(Stub* self)                        = 0; // prints information about the stub
};


// DEF_STUB_INTERFACE is used to create a concrete stub interface
// class, forwarding stub interface calls to the corresponding
// stub calls.

#define DEF_STUB_INTERFACE(stub)                           \
  class stub##Interface: public StubInterface {            \
   private:                                                \
    static stub*    cast(Stub* self)                       { return (stub*)self; }                 \
                                                           \
   public:                                                 \
    /* Initialization/finalization */                      \
    virtual void    initialize(Stub* self, int size,       \
                               CodeStrings& strings)       { cast(self)->initialize(size, strings); } \
    virtual void    finalize(Stub* self)                   { cast(self)->finalize(); }             \
                                                           \
    /* General info */                                     \
    virtual int     size(Stub* self) const                 { return cast(self)->size(); }          \
    virtual int     code_size_to_size(int code_size) const { return stub::code_size_to_size(code_size); } \
                                                           \
    /* Code info */                                        \
    virtual address code_begin(Stub* self) const           { return cast(self)->code_begin(); }    \
    virtual address code_end(Stub* self) const             { return cast(self)->code_end(); }      \
                                                           \
    /* Debugging */                                        \
    virtual void    verify(Stub* self)                     { cast(self)->verify(); }               \
    virtual void    print(Stub* self)                      { cast(self)->print(); }                \
  };


// A StubQueue maintains a queue of stubs.
// Note: All sizes (spaces) are given in bytes.
// StubQueue表示一个用来保存Stub的队列，其定义位于stubs.hpp中。StubQueue的属性都是私有属性
// StubQueue通过_queue_begin和_queue_end来标识队列的范围，依据队列状态的不同在内存中的先后顺序不同。
// 队列有两种状态，一种是连续状态，即begin和end之间的部分都已经分配了，end大于begin，此时所有的Stub在内存上是连续的，另一种是非连续状态，
// 即begin和end之间的部分是未分配的，end小于begin，此时Stub在内存上是不连续的，被end和begin之间的空白部分分隔开了
// StubQueue定义的public方法主要有以下几类：
//
//      属性操作相关，如is_empty，total_space，number_of_stubs，code_start等
//      分配Stub的，如request_committed，request，commit
//      队列操作的，如remove_first，queues_do，stubs_do，next等
class StubQueue: public CHeapObj<mtCode> {
  friend class VMStructs;
 private:
  // 该队列关联的StubInterface实例
  StubInterface* _stub_interface;                // the interface prototype
  // 存储Stub的buffer的起始地址
  address        _stub_buffer;                   // where all stubs are stored
  // 存储Stub的buffer的内存大小
  int            _buffer_size;                   // the buffer size in bytes
  // 存储Stub的buffer的limit的位置，能够分配Stub的上限
  int            _buffer_limit;                  // the (byte) index of the actual buffer limit (_buffer_limit <= _buffer_size)
  // 队列中第一个Stub相对于_stub_buffer的偏移量
  int            _queue_begin;                   // the (byte) index of the first queue entry (word-aligned)
  // 队列后第一个Stub相对于_stub_buffer的偏移量，即下一个可分配Stub的地址
  int            _queue_end;                     // the (byte) index of the first entry after the queue (word-aligned)
  // 队列中保存的stub的个数
  int            _number_of_stubs;               // the number of buffered stubs
  // 执行队列操作用到的锁
  Mutex* const   _mutex;                         // the lock used for a (request, commit) transaction

  void  check_index(int i) const                 { assert(0 <= i && i < _buffer_limit && i % CodeEntryAlignment == 0, "illegal index"); }
  bool  is_contiguous() const                    { return _queue_begin <= _queue_end; }
  int   index_of(Stub* s) const                  { int i = (address)s - _stub_buffer; check_index(i); return i; }
  Stub* stub_at(int i) const                     { check_index(i); return (Stub*)(_stub_buffer + i); }
  Stub* current_stub() const                     { return stub_at(_queue_end); }

  // Stub functionality accessed via interface
  void  stub_initialize(Stub* s, int size,
                        CodeStrings& strings)    { assert(size % CodeEntryAlignment == 0, "size not aligned"); _stub_interface->initialize(s, size, strings); }
  void  stub_finalize(Stub* s)                   { _stub_interface->finalize(s); }
  int   stub_size(Stub* s) const                 { return _stub_interface->size(s); }
  bool  stub_contains(Stub* s, address pc) const { return _stub_interface->code_begin(s) <= pc && pc < _stub_interface->code_end(s); }
  int   stub_code_size_to_size(int code_size) const { return _stub_interface->code_size_to_size(code_size); }
  void  stub_verify(Stub* s)                     { _stub_interface->verify(s); }
  void  stub_print(Stub* s)                      { _stub_interface->print(s); }

  static void register_queue(StubQueue*);

 public:
  StubQueue(StubInterface* stub_interface, int buffer_size, Mutex* lock,
            const char* name);
  ~StubQueue();

  // General queue info
  bool  is_empty() const                         { return _queue_begin == _queue_end; }
  int   total_space() const                      { return _buffer_size - 1; }
    //连续状态下_queue_begin小于_queue_end，d小于0，此时可用空间是end到size之间的部分
    //非连续状态下_queue_begin大于_queue_end，d大于0，此时可用空间就是begin和end之间的部分
  int   available_space() const                  { int d = _queue_begin - _queue_end - 1; return d < 0 ? d + _buffer_size : d; }
  int   used_space() const                       { return total_space() - available_space(); }
  int   number_of_stubs() const                  { return _number_of_stubs; }
  bool  contains(address pc) const               { return _stub_buffer <= pc && pc < _stub_buffer + _buffer_limit; }
  Stub* stub_containing(address pc) const;
  address code_start() const                     { return _stub_buffer; }
  address code_end() const                       { return _stub_buffer + _buffer_limit; }

  // Stub allocation (atomic transactions) 分配Stub
  // request方法负责加锁，请求内存，commit方法负责更新_queue_end等属性并解锁，request_committed方法是先调用request方法，请求内存成功后再调用commit方法
  Stub* request_committed(int code_size);        // request a stub that provides exactly code_size space for code
  Stub* request(int requested_code_size);        // request a stub with a (maximum) code space - locks the queue
  void  commit (int committed_code_size,
                CodeStrings& strings);           // commit the previously requested stub - unlocks the queue

  // Stub deallocation
  // 分配新的Stub就是不断的移动_queue_end，而_queue_begin保持不变，移除Stub就是不断移动_queue_begin而_queue_end保持不变，
  // 两者在StubQueue创建时都是初始化为0，后期独立改变。而所谓的连续状态和非连续状态是指内存上是否连续的，并非说Stub在两个Blob中分配，
  // 实际一个StubQueue永远只有一个Blob。
  void  remove_first();                          // remove the first stub in the queue
  void  remove_first(int n);                     // remove the first n stubs in the queue
  void  remove_all();                            // remove all stubs in the queue

  // Iteration
  static void queues_do(void f(StubQueue* s));   // call f with each StubQueue
  void  stubs_do(void f(Stub* s));               // call f with all stubs
  Stub* first() const                            { return number_of_stubs() > 0 ? stub_at(_queue_begin) : NULL; }

  Stub* next(Stub* s) const                      {
      //index_of返回Stub s的偏移量，加上s的内存大小，得到的就是下一个Stub的偏移量了
                                                   int i = index_of(s) + stub_size(s);
      //如果等于_buffer_limit，说明遍历完了，从头开始遍历，此时队列处于非连续状态
                                                   if (i == _buffer_limit) i = 0;
      //如果等于_queue_end说明遍历完了
                                                   return (i == _queue_end) ? NULL : stub_at(i);
                                                 }

  address stub_code_begin(Stub* s) const         { return _stub_interface->code_begin(s); }
  address stub_code_end(Stub* s) const           { return _stub_interface->code_end(s);   }

  // Debugging/printing
  void  verify();                                // verifies the stub queue
  void  print();                                 // prints information about the stub queue
};

#endif // SHARE_VM_CODE_STUBS_HPP
