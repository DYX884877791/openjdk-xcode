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

#ifndef SHARE_VM_RUNTIME_HANDLES_HPP
#define SHARE_VM_RUNTIME_HANDLES_HPP

#include "oops/klass.hpp"

//------------------------------------------------------------------------------------------------------------------------
// In order to preserve oops during garbage collection, they should be
// allocated and passed around via Handles within the VM. A handle is
// simply an extra indirection allocated in a thread local handle area.
//
// A handle is a ValueObj, so it can be passed around as a value, can
// be used as a parameter w/o using &-passing, and can be returned as a
// return value.
//
// oop parameters and return types should be Handles whenever feasible.
//
// Handles are declared in a straight-forward manner, e.g.
//
//   oop obj = ...;
//   Handle h1(obj);              // allocate new handle
//   Handle h2(thread, obj);      // faster allocation when current thread is known
//   Handle h3;                   // declare handle only, no allocation occurs
//   ...
//   h3 = h1;                     // make h3 refer to same indirection as h1
//   oop obj2 = h2();             // get handle value
//   h1->print();                 // invoking operation on oop
//
// Handles are specialized for different oop types to provide extra type
// information and avoid unnecessary casting. For each oop type xxxOop
// there is a corresponding handle called xxxHandle, e.g.
//
//   oop           Handle
//   Method*       methodHandle
//   instanceOop   instanceHandle
// Handle模型
//      持有其他三种模型的引用，相比直接访问其他三种模型效率偏低，但是灵活性好

//------------------------------------------------------------------------------------------------------------------------
// Base class for all handles. Provides overloading of frequently
// used operators for ease of use.
// handle主要用于封装oop和klass,因此声明handle是直接将oop或者klass传递进去,同时JVM执行java类的方法是最终也是通过handle拿到对应的oop和klass
class Handle VALUE_OBJ_CLASS_SPEC {
 private:
    // Handle类内部只有一个成员变量_handle，该变量类型是oop*，因此该变量最终指向的就是一个oop的首地址
    // 在访问 Java 类时一定是通过 handle 内部指针得到 oop 实例的，再通过 oop 就能拿到 klass ，如此 handle 最终便能操纵 oop 的行为了
    // （注意，如果是调用JVM内部C++类型所对应的oop的函数 ，则不需要通过 handle 来中转，直接通过 oop 拿到指定的 klass便能实现）。
    // klass 不仅包含自己所固有的行为接口，而且也能够操作 Java 类的函数。由于Java 函数在JVM内部都被表示成虚函数，因此handle模型其实就是 Java  类行为的表达。
  oop* _handle;

 protected:
  oop     obj() const                            { return _handle == NULL ? (oop)NULL : *_handle; }
  oop     non_null_obj() const                   { assert(_handle != NULL, "resolving NULL handle"); return *_handle; }

 public:
  // Constructors
  Handle()                                       { _handle = NULL; }
  Handle(oop obj);
  Handle(Thread* thread, oop obj);

  // General access
  oop     operator () () const                   { return obj(); }
  // oop和klass被handle封装之后,JVM内部大部分堆oop和klass的函数调用都要经过Handle一次中间寻址,因此重载了-> 运算符
  oop     operator -> () const                   { return non_null_obj(); }
  bool    operator == (oop o) const              { return obj() == o; }
  bool    operator == (const Handle& h) const          { return obj() == h.obj(); }

  // Null checks
  bool    is_null() const                        { return _handle == NULL; }
  bool    not_null() const                       { return _handle != NULL; }

  // Debugging
  void    print()                                { obj()->print(); }

  // Direct interface, use very sparingly.
  // Used by JavaCalls to quickly convert handles and to create handles static data structures.
  // Constructor takes a dummy argument to prevent unintentional type conversion in C++.
  Handle(oop *handle, bool dummy)                { _handle = handle; }

  // Raw handle access. Allows easy duplication of Handles. This can be very unsafe
  // since duplicates is only valid as long as original handle is alive.
  oop* raw_value()                               { return _handle; }
  static oop raw_resolve(oop *handle)            { return handle == NULL ? (oop)NULL : *handle; }
};

// Specific Handles for different oop types
#define DEF_HANDLE(type, is_a)                   \
  class type##Handle: public Handle {            \
   protected:                                    \
    type##Oop    obj() const                     { return (type##Oop)Handle::obj(); } \
    type##Oop    non_null_obj() const            { return (type##Oop)Handle::non_null_obj(); } \
                                                 \
   public:                                       \
    /* Constructors */                           \
    type##Handle ()                              : Handle()                 {} \
    type##Handle (type##Oop obj) : Handle((oop)obj) {                         \
      assert(is_null() || ((oop)obj)->is_a(),                                 \
             "illegal type");                                                 \
    }                                                                         \
    type##Handle (Thread* thread, type##Oop obj) : Handle(thread, (oop)obj) { \
      assert(is_null() || ((oop)obj)->is_a(), "illegal type");                \
    }                                                                         \
    \
    /* Operators for ease of use */              \
    type##Oop    operator () () const            { return obj(); } \
    type##Oop    operator -> () const            { return non_null_obj(); } \
  };

//OopHandle
DEF_HANDLE(instance         , is_instance         )
/**
 * 宏扩展为：
 * class instanceHandle: public Handle {
 *    protected:
 *     instanceOop    obj() const                     { return (instanceOop)Handle::obj(); }
 *     instanceOop    non_null_obj() const            { return (instanceOop)Handle::non_null_obj(); }
 *
 *    public:
 *      instanceHandle ()                              : Handle()                 {}
 *      instanceHandle (instanceOop obj) : Handle((oop)obj) {
 *          assert(is_null() || ((oop)obj)->is_instance(), "illegal type");
 *      }
 *      instanceHandle (Thread* thread, instanceOop obj) : Handle(thread, (oop)obj) {
 *          assert(is_null() || ((oop)obj)->is_instance(), "illegal type");
 *      }
 *
 *    instanceOop    operator () () const            { return obj(); }
 *    instanceOop    operator -> () const            { return non_null_obj(); }
 * };
 */
DEF_HANDLE(array            , is_array            )
DEF_HANDLE(objArray         , is_objArray         )
DEF_HANDLE(typeArray        , is_typeArray        )

//------------------------------------------------------------------------------------------------------------------------

// Metadata Handles.  Unlike oop Handles these are needed to prevent metadata
// from being reclaimed by RedefineClasses.

// Specific Handles for different oop types
#define DEF_METADATA_HANDLE(name, type)          \
  class name##Handle;                            \
  class name##Handle : public StackObj {         \
    type*     _value;                            \
    Thread*   _thread;                           \
   protected:                                    \
    type*        obj() const                     { return _value; } \
    type*        non_null_obj() const            { assert(_value != NULL, "resolving NULL _value"); return _value; } \
                                                 \
   public:                                       \
    /* Constructors */                           \
    name##Handle () : _value(NULL), _thread(NULL) {}   \
    name##Handle (type* obj);                    \
    name##Handle (Thread* thread, type* obj);    \
                                                 \
    name##Handle (const name##Handle &h);        \
    name##Handle& operator=(const name##Handle &s); \
                                                 \
    /* Destructor */                             \
    ~name##Handle ();                            \
    void remove();                               \
                                                 \
    /* Operators for ease of use */              \
    type*        operator () () const            { return obj(); } \
    type*        operator -> () const            { return non_null_obj(); } \
                                                 \
    bool    operator == (type* o) const          { return obj() == o; } \
    bool    operator == (const name##Handle& h) const  { return obj() == h.obj(); } \
                                                 \
    /* Null checks */                            \
    bool    is_null() const                      { return _value == NULL; } \
    bool    not_null() const                     { return _value != NULL; } \
  };


DEF_METADATA_HANDLE(method, Method)
DEF_METADATA_HANDLE(constantPool, ConstantPool)

// Writing this class explicitly, since DEF_METADATA_HANDLE(klass) doesn't
// provide the necessary Klass* <-> Klass* conversions. This Klass
// could be removed when we don't have the Klass* typedef anymore.
class KlassHandle : public StackObj {
  Klass* _value;
 protected:
   Klass* obj() const          { return _value; }
   Klass* non_null_obj() const { assert(_value != NULL, "resolving NULL _value"); return _value; }

 public:
   KlassHandle()                                 : _value(NULL) {}
   KlassHandle(const Klass* obj)                 : _value(const_cast<Klass *>(obj)) {};
   KlassHandle(Thread* thread, const Klass* obj) : _value(const_cast<Klass *>(obj)) {};

   Klass* operator () () const { return obj(); }
   Klass* operator -> () const { return non_null_obj(); }

   bool operator == (Klass* o) const             { return obj() == o; }
   bool operator == (const KlassHandle& h) const { return obj() == h.obj(); }

    bool is_null() const  { return _value == NULL; }
    bool not_null() const { return _value != NULL; }
};

class instanceKlassHandle : public KlassHandle {
 public:
  /* Constructors */
  instanceKlassHandle () : KlassHandle() {}
  instanceKlassHandle (const Klass* k) : KlassHandle(k) {
    assert(k == NULL || k->oop_is_instance(),
           "illegal type");
  }
  instanceKlassHandle (Thread* thread, const Klass* k) : KlassHandle(thread, k) {
    assert(k == NULL || k->oop_is_instance(),
           "illegal type");
  }
  /* Access to klass part */
  InstanceKlass*       operator () () const { return (InstanceKlass*)obj(); }
  InstanceKlass*       operator -> () const { return (InstanceKlass*)obj(); }
};


//------------------------------------------------------------------------------------------------------------------------
// Thread local handle area
// HandleArea的定义同样位于hotspot/src/share/vm/runtime/handles.hpp中，表示一片专门用于分配Handle的内存区域
class HandleArea: public Arena {
  friend class HandleMark;
  friend class NoHandleMark;
  friend class ResetNoHandleMark;
#ifdef ASSERT
  int _handle_mark_nesting;
  int _no_handle_mark_nesting;
#endif
  // HandleArea只添加了一个私有属性HandleArea* _prev，表示HandleArea链中前一个HandleArea实例。
  HandleArea* _prev;          // link to outer (older) area
 public:
  // Constructor
  HandleArea(HandleArea* prev) : Arena(mtThread, Chunk::tiny_size) {
    debug_only(_handle_mark_nesting    = 0);
    debug_only(_no_handle_mark_nesting = 0);
    _prev = prev;
  }

  // Handle allocation
 private:
  oop* real_allocate_handle(oop obj) {
#ifdef ASSERT
    oop* handle = (oop*) (UseMallocOnly ? internal_malloc_4(oopSize) : Amalloc_4(oopSize));
#else
    oop* handle = (oop*) Amalloc_4(oopSize);
#endif
      //将handle指向的内存赋值成obj
    *handle = obj;
    return handle;
  }
 public:
#ifdef ASSERT
    // 用于分配保存oop的内存
  oop* allocate_handle(oop obj);
#else
  oop* allocate_handle(oop obj) { return real_allocate_handle(obj); }
#endif

  // Garbage collection support
  // 垃圾回收时遍历对应引用
  void oops_do(OopClosure* f);

  // Number of handles in use
  // 获取已经使用的内存大小
  size_t used() const     { return Arena::used() / oopSize; }

  debug_only(bool no_handle_mark_active() { return _no_handle_mark_nesting > 0; })
};


//------------------------------------------------------------------------------------------------------------------------
// Handles are allocated in a (growable) thread local handle area. Deallocation
// is managed using a HandleMark. It should normally not be necessary to use
// HandleMarks manually.
//
// A HandleMark constructor will record the current handle area top, and the
// desctructor will reset the top, destroying all handles allocated in between.
// The following code will therefore NOT work:
//
//   Handle h;
//   {
//     HandleMark hm;
//     h = Handle(obj);
//   }
//   h()->print();       // WRONG, h destroyed by HandleMark destructor.
//
// If h has to be preserved, it can be converted to an oop or a local JNI handle
// across the HandleMark boundary.

// The base class of HandleMark should have been StackObj but we also heap allocate
// a HandleMark when a thread is created. The operator new is for this special case.
// 在执行Java方法调用前会先构建一个JavaCallWrapper实例，然后再构造一个HandleMark实例。HandleMark的定义在hotspot/src/share/vm/runtime/handles.hpp中，
// 主要用于记录当前线程的HandleArea的内存地址top，执行方法调用后，HandleMark实例自动销毁，在HandleMark的析构函数中会将HandleArea的
// 当前内存地址到方法调用前的内存地址top之间的所有分配的oop都销毁掉，然后恢复当前线程的HandleArea的内存地址top到方法调用前的状态。
// HandleMark一般情况下直接在线程栈内存上分配，应该继承自StackObj，但是部分情况下HandleMark也需要在堆内存上分配，所以没有继承自StackObj，并且为了支持在堆内存上分配，重载了new和delete方法。
class HandleMark {
    // HandleMark定义的属性都是私有属性
 private:
  Thread *_thread;              // thread that owns this mark 表示拥有此HandleMark实例的Thread
  HandleArea *_area;            // saved handle area 此HandleMark实例保存的HandleArea
  Chunk *_chunk;                // saved arena chunk 保存HandleArea的Chunk
  char *_hwm, *_max;            // saved arena info 保存的HandleArea的信息
  size_t _size_in_bytes;        // size of handle area 保存的HandleArea的大小
  // Link to previous active HandleMark in thread 当前线程的上一个活跃的HandleMark实例
  HandleMark* _previous_handle_mark;

  /**
   * 除构造析构函数外，HandleMark定义的方法有三种：
   * 属性相关的，如set_previous_handle_mark，previous_handle_mark，size_in_bytes
   * HandleMarkCleaner调用的方法，push在HandleMarkCleaner构造时调用，pop_and_restore在HandleMarkCleaner销毁时调用
   * 重载的new和delete方法，支持从堆内存中分配。
   */
  void initialize(Thread* thread);                // common code for constructors
  void set_previous_handle_mark(HandleMark* mark) { _previous_handle_mark = mark; }
  HandleMark* previous_handle_mark() const        { return _previous_handle_mark; }

  size_t size_in_bytes() const { return _size_in_bytes; }
 public:
    // HandleMark的核心方法就是构造和析构方法
  HandleMark();                            // see handles_inline.hpp
  HandleMark(Thread* thread)                      { initialize(thread); }
  ~HandleMark();

  // Functions used by HandleMarkCleaner
  // called in the constructor of HandleMarkCleaner
  void push();
  // called in the destructor of HandleMarkCleaner
  void pop_and_restore();
  // overloaded operators
  void* operator new(size_t size) throw();
  void* operator new [](size_t size) throw();
  void operator delete(void* p);
  void operator delete[](void* p);
};

//------------------------------------------------------------------------------------------------------------------------
// A NoHandleMark stack object will verify that no handles are allocated
// in its scope. Enabled in debug mode only.

class NoHandleMark: public StackObj {
 public:
#ifdef ASSERT
  NoHandleMark();
  ~NoHandleMark();
#else
  NoHandleMark()  {}
  ~NoHandleMark() {}
#endif
};


class ResetNoHandleMark: public StackObj {
  int _no_handle_mark_nesting;
 public:
#ifdef ASSERT
  ResetNoHandleMark();
  ~ResetNoHandleMark();
#else
  ResetNoHandleMark()  {}
  ~ResetNoHandleMark() {}
#endif
};

#endif // SHARE_VM_RUNTIME_HANDLES_HPP
