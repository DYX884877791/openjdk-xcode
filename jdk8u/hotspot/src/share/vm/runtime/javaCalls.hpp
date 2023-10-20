/*
 * Copyright (c) 1997, 2017, Oracle and/or its affiliates. All rights reserved.
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

#ifndef SHARE_VM_RUNTIME_JAVACALLS_HPP
#define SHARE_VM_RUNTIME_JAVACALLS_HPP

#include "memory/allocation.hpp"
#include "oops/method.hpp"
#include "runtime/handles.hpp"
#include "runtime/javaFrameAnchor.hpp"
#include "runtime/thread.inline.hpp"
#include "runtime/vmThread.hpp"
#ifdef TARGET_ARCH_x86
# include "jniTypes_x86.hpp"
#endif
#ifdef TARGET_ARCH_aarch64
# include "jniTypes_aarch64.hpp"
#endif
#ifdef TARGET_ARCH_sparc
# include "jniTypes_sparc.hpp"
#endif
#ifdef TARGET_ARCH_zero
# include "jniTypes_zero.hpp"
#endif
#ifdef TARGET_ARCH_arm
# include "jniTypes_arm.hpp"
#endif
#ifdef TARGET_ARCH_ppc
# include "jniTypes_ppc.hpp"
#endif

// A JavaCallWrapper is constructed before each JavaCall and destructed after the call.
// Its purpose is to allocate/deallocate a new handle block and to save/restore the last
// Java fp/sp. A pointer to the JavaCallWrapper is stored on the stack.

/**
 * JavaCallWrapper的定义位于每次执行Java方法调用时都需要创建一个新的JavaCallWrapper实例，然后在方法调用结束销毁这个实例，
 * 通过JavaCallWrapper实例的创建和销毁来保存方法调用前当前线程的上一个栈帧，重新分配或者销毁一个handle block，保存和重置Java调用栈的fp/sp。
 * JavaCallWrapper实例的指针保存在调用栈上。
 *
 * JavaCallWrapper的核心方法就是其构造方法和析构方法，其他方法都是读取相关属性的
 */
class JavaCallWrapper: StackObj {
  friend class VMStructs;
 private:
    // 关联的Java线程实例
  JavaThread*      _thread;                 // the thread to which this call belongs
    //实际保存JNI引用的内存块的指针
  JNIHandleBlock*  _handles;                // the saved handle block
    //准备调用的Java方法
  Method*          _callee_method;          // to be able to collect arguments if entry frame is top frame
    //执行方法调用的接受对象实例
  oop              _receiver;               // the receiver of the call (if a non-static call)

    //用于记录线程的执行状态，比如pc计数器
  JavaFrameAnchor  _anchor;                 // last thread anchor state that we must restore

    //保存方法调用结果对象
  JavaValue*       _result;                 // result value

 public:
  // Construction/destruction
    /**
     * C++中用构造函数和析构函数来初始化和清理对象，这两个函数将会被编译器自动调用。对象的初始化和清理是非常重要的，如果我们不提供构造函数与析构函数，编译器会自动提供两个函数的空实现。
     *
     * 构造函数：主要作用于创建函数时对对象成员的属性赋值。
     * 析构函数：主要作用于在对象销毁前，执行一些清理工作（如释放new开辟在堆区的空间）。
     *
     * 主要特点：
     * 构造函数语法：类名(){}
     * 1.构造函数，没有返回值也不写void
     * 2.函数名称与类名相同
     * 3.构造函数可以有参数，因此可以发生重载
     * 4.程序在调用对象时候会自动调用构造，无须手动调用，而且只会调用一次
     *
     * 析构函数语法： ~类名(){}
     * 1.析构函数，没有返回值也不写void
     * 2.函数名称与类名相同,在名称前加上符号 ~
     * 3.析构函数不可以有参数，因此不可以发生重载
     * 4.程序在对象销毁前会自动调用析构，无须手动调用，而且只会调用一次
     *
     * 其余特点：
     *
     * 构造函数和析构函数是一种特殊的公有成员函数，每一个类都有一个默认的构造函数和析构函数；
     * 构造函数在类定义时由系统自动调用，析构函数在类被销毁时由系统自动调用；
     * 构造函数的名称和类名相同，一个类可以有多个构造函数，只能有一个析构函数。不同的构造函数之间通过参数个数和参数类型来区分；
     * 我们可以在构造函数中给类分配资源，在类的析构函数中释放对应的资源。
     * 如果程序员没有提供构造和析构，系统会默认提供，空实现
     * 构造函数 和 析构函数，必须定义在public里面，才可以调用
     */
   JavaCallWrapper(methodHandle callee_method, Handle receiver, JavaValue* result, TRAPS);
  ~JavaCallWrapper();

  // Accessors
  JavaThread*      thread() const           { return _thread; }
  JNIHandleBlock*  handles() const          { return _handles; }

  JavaFrameAnchor* anchor(void)             { return &_anchor; }

  JavaValue*       result() const           { return _result; }
  // GC support
  Method*          callee_method()          { return _callee_method; }
  oop              receiver()               { return _receiver; }
  void             oops_do(OopClosure* f);

  bool             is_first_frame() const   { return _anchor.last_Java_sp() == NULL; }

};


// Encapsulates arguments to a JavaCall (faster, safer, and more convenient than using var-args)
class JavaCallArguments : public StackObj {
 private:
  enum Constants {
   _default_size = 8    // Must be at least # of arguments in JavaCalls methods
  };

  intptr_t    _value_buffer      [_default_size + 1];
  u_char      _value_state_buffer[_default_size + 1];

  intptr_t*   _value;
  u_char*     _value_state;
  int         _size;
  int         _max_size;
  bool        _start_at_zero;      // Support late setting of receiver

  void initialize() {
    // Starts at first element to support set_receiver.
    _value       = &_value_buffer[1];
    _value_state = &_value_state_buffer[1];

    _max_size = _default_size;
    _size = 0;
    _start_at_zero = false;
  }

  // Helper for push_oop and the like.  The value argument is a
  // "handle" that refers to an oop.  We record the address of the
  // handle rather than the designated oop.  The handle is later
  // resolved to the oop by parameters().  This delays the exposure of
  // naked oops until it is GC-safe.
  template<typename T>
  inline int push_oop_impl(T handle, int size) {
    // JNITypes::put_obj expects an oop value, so we play fast and
    // loose with the type system.  The cast from handle type to oop
    // *must* use a C-style cast.  In a product build it performs a
    // reinterpret_cast. In a debug build (more accurately, in a
    // CHECK_UNHANDLED_OOPS build) it performs a static_cast, invoking
    // the debug-only oop class's conversion from void* constructor.
    JNITypes::put_obj((oop)handle, _value, size); // Updates size.
    return size;                // Return the updated size.
  }

 public:
  JavaCallArguments() { initialize(); }

  JavaCallArguments(Handle receiver) {
    initialize();
    push_oop(receiver);
  }

  JavaCallArguments(int max_size) {
    if (max_size > _default_size) {
      _value = NEW_RESOURCE_ARRAY(intptr_t, max_size + 1);
      _value_state = NEW_RESOURCE_ARRAY(u_char, max_size + 1);

      // Reserve room for potential receiver in value and state
      _value++;
      _value_state++;

      _max_size = max_size;
      _size = 0;
      _start_at_zero = false;
    } else {
      initialize();
    }
  }

  // The possible values for _value_state elements.
  enum {
    value_state_primitive,
    value_state_oop,
    value_state_handle,
    value_state_jobject,
    value_state_limit
  };

  inline void push_oop(Handle h) {
    _value_state[_size] = value_state_handle;
    _size = push_oop_impl(h.raw_value(), _size);
  }

  inline void push_jobject(jobject h) {
    _value_state[_size] = value_state_jobject;
    _size = push_oop_impl(h, _size);
  }

  inline void push_int(int i) {
    _value_state[_size] = value_state_primitive;
    JNITypes::put_int(i, _value, _size);
  }

  inline void push_double(double d) {
    _value_state[_size] = value_state_primitive;
    _value_state[_size + 1] = value_state_primitive;
    JNITypes::put_double(d, _value, _size);
  }

  inline void push_long(jlong l) {
    _value_state[_size] = value_state_primitive;
    _value_state[_size + 1] = value_state_primitive;
    JNITypes::put_long(l, _value, _size);
  }

  inline void push_float(float f) {
    _value_state[_size] = value_state_primitive;
    JNITypes::put_float(f, _value, _size);
  }

  // receiver
  Handle receiver() {
    assert(_size > 0, "must at least be one argument");
    assert(_value_state[0] == value_state_handle,
           "first argument must be an oop");
    assert(_value[0] != 0, "receiver must be not-null");
    return Handle((oop*)_value[0], false);
  }

  void set_receiver(Handle h) {
    assert(_start_at_zero == false, "can only be called once");
    _start_at_zero = true;
    _value_state--;
    _value--;
    _size++;
    _value_state[0] = value_state_handle;
    push_oop_impl(h.raw_value(), 0);
  }

  // Converts all Handles to oops, and returns a reference to parameter vector
  intptr_t* parameters() ;
  int   size_of_parameters() const { return _size; }

  // Verify that pushed arguments fits a given method
  void verify(methodHandle method, BasicType return_type);
};

// All calls to Java have to go via JavaCalls. Sets up the stack frame
// and makes sure that the last_Java_frame pointers are chained correctly.
//

class JavaCalls: AllStatic {
  static void call_helper(JavaValue* result, methodHandle* method, JavaCallArguments* args, TRAPS);
 public:
  // Optimized Constuctor call
  static void call_default_constructor(JavaThread* thread, methodHandle method, Handle receiver, TRAPS);

  // call_special
  // ------------
  // The receiver must be first oop in argument list
  static void call_special(JavaValue* result, KlassHandle klass, Symbol* name, Symbol* signature, JavaCallArguments* args, TRAPS);

  static void call_special(JavaValue* result, Handle receiver, KlassHandle klass, Symbol* name, Symbol* signature, TRAPS); // No args
  static void call_special(JavaValue* result, Handle receiver, KlassHandle klass, Symbol* name, Symbol* signature, Handle arg1, TRAPS);
  static void call_special(JavaValue* result, Handle receiver, KlassHandle klass, Symbol* name, Symbol* signature, Handle arg1, Handle arg2, TRAPS);

  // virtual call
  // ------------

  // The receiver must be first oop in argument list
  static void call_virtual(JavaValue* result, KlassHandle spec_klass, Symbol* name, Symbol* signature, JavaCallArguments* args, TRAPS);

  static void call_virtual(JavaValue* result, Handle receiver, KlassHandle spec_klass, Symbol* name, Symbol* signature, TRAPS); // No args
  static void call_virtual(JavaValue* result, Handle receiver, KlassHandle spec_klass, Symbol* name, Symbol* signature, Handle arg1, TRAPS);
  static void call_virtual(JavaValue* result, Handle receiver, KlassHandle spec_klass, Symbol* name, Symbol* signature, Handle arg1, Handle arg2, TRAPS);

  // Static call
  // -----------
  static void call_static(JavaValue* result, KlassHandle klass, Symbol* name, Symbol* signature, JavaCallArguments* args, TRAPS);

  static void call_static(JavaValue* result, KlassHandle klass, Symbol* name, Symbol* signature, TRAPS);
  static void call_static(JavaValue* result, KlassHandle klass, Symbol* name, Symbol* signature, Handle arg1, TRAPS);
  static void call_static(JavaValue* result, KlassHandle klass, Symbol* name, Symbol* signature, Handle arg1, Handle arg2, TRAPS);

  // Low-level interface
  static void call(JavaValue* result, methodHandle method, JavaCallArguments* args, TRAPS);
};

#endif // SHARE_VM_RUNTIME_JAVACALLS_HPP
