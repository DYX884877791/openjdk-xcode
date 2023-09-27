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

#include "precompiled.hpp"
#include "classfile/systemDictionary.hpp"
#include "classfile/vmSymbols.hpp"
#include "code/nmethod.hpp"
#include "compiler/compileBroker.hpp"
#include "interpreter/interpreter.hpp"
#include "interpreter/linkResolver.hpp"
#include "memory/universe.inline.hpp"
#include "oops/oop.inline.hpp"
#include "prims/jniCheck.hpp"
#include "runtime/compilationPolicy.hpp"
#include "runtime/handles.inline.hpp"
#include "runtime/interfaceSupport.hpp"
#include "runtime/javaCalls.hpp"
#include "runtime/mutexLocker.hpp"
#include "runtime/signature.hpp"
#include "runtime/stubRoutines.hpp"
#include "runtime/thread.inline.hpp"
#include "utilities/slog.hpp"

// -----------------------------------------------------
// Implementation of JavaCallWrapper

// 构造方法
JavaCallWrapper::JavaCallWrapper(methodHandle callee_method, Handle receiver, JavaValue* result, TRAPS) {
  JavaThread* thread = (JavaThread *)THREAD;
  bool clear_pending_exception = true;

    //校验是否是Java线程
  guarantee(thread->is_Java_thread(), "crucial check - the VM thread cannot and must not escape to Java code");
    //校验当前线程是否有未释放的锁，即本地代码执行Java方法前必须释放所有的锁
  assert(!thread->owns_locks(), "must release all locks when leaving VM");
    //确保当前线程不是JIT编译线程
  guarantee(!thread->is_Compiler_thread(), "cannot make java calls from the compiler");
  _result   = result;

  // Allocate handle block for Java code. This must be done before we change thread_state to _thread_in_Java_or_stub,
  // since it can potentially block.
    // 分配一个新的JNIHandleBlock
  JNIHandleBlock* new_handles = JNIHandleBlock::allocate_block(thread);

  // After this, we are official in JavaCode. This needs to be done before we change any of the thread local
  // info, since we cannot find oops before the new information is set up completely.
    //将当前线程的状态从_thread_in_vm转换成_thread_in_Java
  ThreadStateTransition::transition(thread, _thread_in_vm, _thread_in_Java);

  // Make sure that we handle asynchronous stops and suspends _before_ we clear all thread state
  // in JavaCallWrapper::JavaCallWrapper(). This way, we can decide if we need to do any pd actions
  // to prepare for stop/suspend (flush register windows on sparcs, cache sp, or other state).
  if (thread->has_special_runtime_exit_condition()) {
      //判断当前线程是否符合特殊的运行时退出条件，如果是则暂停或者终止执行
    thread->handle_special_runtime_exit_condition();
    if (HAS_PENDING_EXCEPTION) {
      clear_pending_exception = false;
    }
  }


  // Make sure to set the oop's after the thread transition - since we can block there. No one is GC'ing
  // the JavaCallWrapper before the entry frame is on the stack.
  _callee_method = callee_method();
  _receiver = receiver();

#ifdef CHECK_UNHANDLED_OOPS
  THREAD->allow_unhandled_oop(&_receiver);
#endif // CHECK_UNHANDLED_OOPS

  _thread       = (JavaThread *)thread;
    //保存当前线程的active_handles
  _handles      = _thread->active_handles();    // save previous handle block & Java frame linkage

  // For the profiler, the last_Java_frame information in thread must always be in
  // legal state. We have no last Java frame if last_Java_sp == NULL so
  // the valid transition is to clear _last_Java_sp and then reset the rest of
  // the (platform specific) state.
    //保存当前线程的执行状态
  _anchor.copy(_thread->frame_anchor());
    //清除当前线程的执行状态
  _thread->frame_anchor()->clear();

  debug_only(_thread->inc_java_call_counter());
    //将新分配的JNIHandleBlock作为线程的active_handles
  _thread->set_active_handles(new_handles);     // install new handle block and reset Java frame linkage

    //校验线程状态不能是_thread_in_native
  assert (_thread->thread_state() != _thread_in_native, "cannot set native pc to NULL");

  // clear any pending exception in thread (native calls start with no exception pending)
  if(clear_pending_exception) {
    _thread->clear_pending_exception();
  }

  if (_anchor.last_Java_sp() == NULL) {
      //记录调用栈的基地址
      _thread->record_base_of_stack_pointer();
  }
}

// 析构方法
JavaCallWrapper::~JavaCallWrapper() {
    //校验执行析构的是同一个Java线程
  assert(_thread == JavaThread::current(), "must still be the same thread");

  // restore previous handle block & Java frame linkage
    //获取当前线程的active_handles
  JNIHandleBlock *_old_handles = _thread->active_handles();
    //恢复方法调用前的active_handles
  _thread->set_active_handles(_handles);

  _thread->frame_anchor()->zap();

  debug_only(_thread->dec_java_call_counter());

  if (_anchor.last_Java_sp() == NULL) {
    _thread->set_base_of_stack_pointer(NULL);
  }


  // Old thread-local info. has been restored. We are not back in the VM.
    //将当前线程的状态恢复成_thread_in_vm
  ThreadStateTransition::transition_from_java(_thread, _thread_in_vm);

  // State has been restored now make the anchor frame visible for the profiler.
  // Do this after the transition because this allows us to put an assert
  // the Java->vm transition which checks to see that stack is not walkable
  // on sparc/ia64 which will catch violations of the reseting of last_Java_frame
  // invariants (i.e. _flags always cleared on return to Java)

    //恢复到方法调用前的线程状态
  _thread->frame_anchor()->copy(&_anchor);

  // Release handles after we are marked as being inside the VM again, since this
  // operation might block
    //释放方法调用中新分配的JNIHandleBlock
  JNIHandleBlock::release_block(_old_handles, _thread);
}


void JavaCallWrapper::oops_do(OopClosure* f) {
  f->do_oop((oop*)&_receiver);
  handles()->oops_do(f);
}


// Helper methods
static BasicType runtime_type_from(JavaValue* result) {
  switch (result->get_type()) {
    case T_BOOLEAN: // fall through
    case T_CHAR   : // fall through
    case T_SHORT  : // fall through
    case T_INT    : // fall through
#ifndef _LP64
    case T_OBJECT : // fall through
    case T_ARRAY  : // fall through
#endif
    case T_BYTE   : // fall through
    case T_VOID   : return T_INT;
    case T_LONG   : return T_LONG;
    case T_FLOAT  : return T_FLOAT;
    case T_DOUBLE : return T_DOUBLE;
#ifdef _LP64
    case T_ARRAY  : // fall through
    case T_OBJECT:  return T_OBJECT;
#endif
  }
  ShouldNotReachHere();
  return T_ILLEGAL;
}

// ===== object constructor calls =====

void JavaCalls::call_default_constructor(JavaThread* thread, methodHandle method, Handle receiver, TRAPS) {
  assert(method->name() == vmSymbols::object_initializer_name(),    "Should only be called for default constructor");
  assert(method->signature() == vmSymbols::void_method_signature(), "Should only be called for default constructor");

  InstanceKlass* ik = method->method_holder();
  if (ik->is_initialized() && ik->has_vanilla_constructor()) {
    // safe to skip constructor call
  } else {
    static JavaValue result(T_VOID);
    JavaCallArguments args(receiver);
    call(&result, method, &args, CHECK);
  }
}

// ============ Virtual calls ============

void JavaCalls::call_virtual(JavaValue* result, KlassHandle spec_klass, Symbol* name, Symbol* signature, JavaCallArguments* args, TRAPS) {
  CallInfo callinfo;
  Handle receiver = args->receiver();
  KlassHandle recvrKlass(THREAD, receiver.is_null() ? (Klass*)NULL : receiver->klass());
  LinkResolver::resolve_virtual_call(
          callinfo, receiver, recvrKlass, spec_klass, name, signature,
          KlassHandle(), false, true, CHECK);
  methodHandle method = callinfo.selected_method();
  assert(method.not_null(), "should have thrown exception");

  // Invoke the method
  JavaCalls::call(result, method, args, CHECK);
}


void JavaCalls::call_virtual(JavaValue* result, Handle receiver, KlassHandle spec_klass, Symbol* name, Symbol* signature, TRAPS) {
  JavaCallArguments args(receiver); // One oop argument
  call_virtual(result, spec_klass, name, signature, &args, CHECK);
}


void JavaCalls::call_virtual(JavaValue* result, Handle receiver, KlassHandle spec_klass, Symbol* name, Symbol* signature, Handle arg1, TRAPS) {
  JavaCallArguments args(receiver); // One oop argument
  args.push_oop(arg1);
  call_virtual(result, spec_klass, name, signature, &args, CHECK);
}



void JavaCalls::call_virtual(JavaValue* result, Handle receiver, KlassHandle spec_klass, Symbol* name, Symbol* signature, Handle arg1, Handle arg2, TRAPS) {
  JavaCallArguments args(receiver); // One oop argument
  args.push_oop(arg1);
  args.push_oop(arg2);
  call_virtual(result, spec_klass, name, signature, &args, CHECK);
}


// ============ Special calls ============

void JavaCalls::call_special(JavaValue* result, KlassHandle klass, Symbol* name, Symbol* signature, JavaCallArguments* args, TRAPS) {
  CallInfo callinfo;
  LinkResolver::resolve_special_call(callinfo, args->receiver(), klass, name, signature, KlassHandle(), false, CHECK);
  methodHandle method = callinfo.selected_method();
  assert(method.not_null(), "should have thrown exception");

  // Invoke the method
  JavaCalls::call(result, method, args, CHECK);
}


void JavaCalls::call_special(JavaValue* result, Handle receiver, KlassHandle klass, Symbol* name, Symbol* signature, TRAPS) {
  JavaCallArguments args(receiver); // One oop argument
  call_special(result, klass, name, signature, &args, CHECK);
}


void JavaCalls::call_special(JavaValue* result, Handle receiver, KlassHandle klass, Symbol* name, Symbol* signature, Handle arg1, TRAPS) {
  JavaCallArguments args(receiver); // One oop argument
  args.push_oop(arg1);
  call_special(result, klass, name, signature, &args, CHECK);
}


void JavaCalls::call_special(JavaValue* result, Handle receiver, KlassHandle klass, Symbol* name, Symbol* signature, Handle arg1, Handle arg2, TRAPS) {
  JavaCallArguments args(receiver); // One oop argument
  args.push_oop(arg1);
  args.push_oop(arg2);
  call_special(result, klass, name, signature, &args, CHECK);
}


// ============ Static calls ============

void JavaCalls::call_static(JavaValue* result, KlassHandle klass, Symbol* name, Symbol* signature, JavaCallArguments* args, TRAPS) {
  CallInfo callinfo;
  LinkResolver::resolve_static_call(callinfo, klass, name, signature, KlassHandle(), false, true, CHECK);
  methodHandle method = callinfo.selected_method();
  assert(method.not_null(), "should have thrown exception");

  // Invoke the method
  JavaCalls::call(result, method, args, CHECK);
}


void JavaCalls::call_static(JavaValue* result, KlassHandle klass, Symbol* name, Symbol* signature, TRAPS) {
  JavaCallArguments args; // No argument
  call_static(result, klass, name, signature, &args, CHECK);
}


void JavaCalls::call_static(JavaValue* result, KlassHandle klass, Symbol* name, Symbol* signature, Handle arg1, TRAPS) {
  JavaCallArguments args(arg1); // One oop argument
  call_static(result, klass, name, signature, &args, CHECK);
}


void JavaCalls::call_static(JavaValue* result, KlassHandle klass, Symbol* name, Symbol* signature, Handle arg1, Handle arg2, TRAPS) {
  JavaCallArguments args; // One oop argument
  args.push_oop(arg1);
  args.push_oop(arg2);
  call_static(result, klass, name, signature, &args, CHECK);
}


// -------------------------------------------------
// Implementation of JavaCalls (low level)


void JavaCalls::call(JavaValue* result, methodHandle method, JavaCallArguments* args, TRAPS) {
  // Check if we need to wrap a potential OS exception handler around thread
  // This is used for e.g. Win32 structured exception handlers
  assert(THREAD->is_Java_thread(), "only JavaThreads can make JavaCalls");
  // Need to wrap each and everytime, since there might be native code down the
  // stack that has installed its own exception handlers
  // 用os::os_exception_wrapper()包装起来，目的是设置HotSpot VM的C++层面的异常处理，真实调用的函数就是call_helper
  os::os_exception_wrapper(call_helper, result, &method, args, THREAD);
}

void JavaCalls::call_helper(JavaValue* result, methodHandle* m, JavaCallArguments* args, TRAPS) {
//  slog_debug("JavaCalls::call_helper函数被调用了...");
  // During dumping, Java execution environment is not fully initialized. Also, Java execution
  // may cause undesirable side-effects in the class metadata.
  assert(!DumpSharedSpaces, "must not execute Java bytecodes when dumping");

  methodHandle method = *m;
    //线程
  JavaThread* thread = (JavaThread*)THREAD;
    //校验当前线程是Java线程
  assert(thread->is_Java_thread(), "must be called by a java thread");
    //校验方法不为空
  assert(method.not_null(), "must have a method to call");
    //校验当前没有在安全点
  assert(!SafepointSynchronize::is_at_safepoint(), "call to Java code during VM operation");
    //校验当前线程的handle_area没有执行handle_mark，即GC标记
  assert(!thread->handle_area()->no_handle_mark_active(), "cannot call out to Java here");


  CHECK_UNHANDLED_OOPS_ONLY(thread->clear_unhandled_oops();)

  // Verify the arguments

  if (CheckJNICalls)  {
    args->verify(method, result->get_type());
  }
  else debug_only(args->verify(method, result->get_type()));

  // Ignore call if method is empty
  // 检查目标方法是否为空方法，是的话直接返回
  if (method->is_empty_method()) {
    assert(result->get_type() == T_VOID, "an empty method must return a void value");
    return;
  }


#ifdef ASSERT
  { InstanceKlass* holder = method->method_holder();
    // A klass might not be initialized since JavaCall's might be used during the executing of
    // the <clinit>. For example, a Thread.start might start executing on an object that is
    // not fully initialized! (bad Java programming style)
    assert(holder->is_linked(), "rewriting must have taken place");
  }
#endif


  // 检查目标方法是否“首次执行前就必须被编译”，是的话调用JIT编译器去编译目标方法
  // 对于main()方法来说，如果配置了-Xint选项，则是以解释模式执行的，所以并不会走这里的compile_method()函数的逻辑。
  assert(!thread->is_Compiler_thread(), "cannot compile from the compiler");
  if (CompilationPolicy::must_be_compiled(method)) {
    CompileBroker::compile_method(method, InvocationEntryBci,
                                  CompilationPolicy::policy()->initial_compile_level(),
                                  methodHandle(), 0, "must_be_compiled", CHECK);
  }

  // 获取目标方法的解释模式入口from_interpreted_entry，下面将其称为entry_point
  // 获取的entry_point就是为Java方法调用准备栈桢，并把代码调用指针指向method的第一个字节码的内存地址。
  // entry_point相当于是method的封装，不同的method类型有不同的entry_point。
  // Since the call stub sets up like the interpreter we call the from_interpreted_entry
  // so we can go compiled via a i2c. Otherwise initial entry method will always
  // run interpreted.
  // 方法对应的entry_point
  // 这个参数会做为实参传递给StubRoutines::call_stub()函数指针指向的“函数”
  // EntryPoint例程:是从当前要执行的Java方法中获取的： hotspot/src/share/vm/oops/method.hpp
  address entry_point = method->from_interpreted_entry();
  slog_trace("获取目标方法[%s]的解释模式入口地址为[%p]", method->name_and_sig_as_C_string(), entry_point);
  if (JvmtiExport::can_post_interpreter_events() && thread->is_interp_only_mode()) {
    entry_point = method->interpreter_entry();
  }

  // Figure out if the result value is an oop or not (Note: This is a different value
  // than result_type. result_type will be T_INT of oops. (it is about size)
    //返回类型
  BasicType result_type = runtime_type_from(result);
  bool oop_result_flag = (result->get_type() == T_OBJECT || result->get_type() == T_ARRAY);

  // NOTE: if we move the computation of the result_val_address inside
  // the call to call_stub, the optimizer produces wrong code.
    //返回值
  intptr_t* result_val_address = (intptr_t*)(result->get_value_addr());

  // Find receiver
  Handle receiver = (!method->is_static()) ? args->receiver() : Handle();

  // When we reenter Java, we need to reenable the yellow zone which
  // might already be disabled when we are in VM.
  if (thread->stack_yellow_zone_disabled()) {
    thread->reguard_stack();
  }

  // 确保Java栈溢出检查机制正确启动
  // Check that there are shadow pages available before changing thread state
  // to Java
  if (!os::stack_shadow_pages_available(THREAD, method)) {
    // Throw stack overflow exception with preinitialized exception.
    Exceptions::throw_stack_overflow_exception(THREAD, __FILE__, __LINE__, method);
    return;
  } else {
    // Touch pages checked if the OS needs them to be touched to be mapped.
    os::bang_stack_shadow_pages();
  }

  // 创建一个JavaCallWrapper，用于管理JNIHandleBlock的分配与释放，
  // 以及在调用Java方法前后保存和恢复Java的frame pointer/stack pointer
  // do call
  { JavaCallWrapper link(method, receiver, result, CHECK);
    { HandleMark hm(thread);  // HandleMark used by HandleMarkCleaner

    // call_helper中最终是通过StubRoutines::call_stub（）的返回值来调用java方法的；由此可知，call_stub（）返回的肯定也是个函数指针之类的
    // hotspot/src/share/vm/runtime/stubRoutines.hpp
    // call_stub是一个宏，将一个固定调用点转换为CallStub函数，
    // StubRoutines::call_stub()返回一个指向call stub的函数指针，
    // 紧接着调用这个call stub，传入前面获取的entry_point和要传给Java方法的参数等信息
    // call stub是在VM初始化时生成的。对应的代码在StubGenerator::generate_call_stub()函数中
    /**
     * 调用StubRoutines::call_stub()函数返回一个函数指针，然后通过函数指针来调用函数指针指向的函数。
     * 通过函数指针调用和通过函数名调用的方式一样，这里我们需要清楚的是，调用的目标函数仍然是C/C++函数，所以由C/C++函数调用另外一个C/C++函数时，要遵守调用约定。
     * 这个调用约定会规定怎么给被调用函数（Callee）传递参数，以及被调用函数的返回值将存储在什么地方。
     *
     * 下面我们就来简单说说Linux X86架构下的C/C++函数调用约定，在这个约定下，以下寄存器用于传递参数：
     *
     * 第1个参数：rdi c_rarg0
     * 第2个参数：rsi c_rarg1
     * 第3个参数：rdx c_rarg2
     * 第4个参数：rcx c_rarg3
     * 第5个参数：r8 c_rarg4
     * 第6个参数：r9 c_rarg5
     *
     * 在函数调用时，6个及小于6个用如下寄存器来传递，在HotSpot中通过更易理解的别名c_rarg* 来使用对应的寄存器。如果参数超过六个，那么程序将会用调用栈来传递那些额外的参数。
     *
     * 数一下我们通过函数指针调用时传递了几个参数？8个，那么后面的2个就需要通过调用函数（Caller）的栈来传递，
     * 这两个参数就是args->size_of_parameters()和CHECK（这是个宏，扩展后就是传递线程对象）。
     */
      StubRoutines::call_stub()(
              //方法链接
        (address)&link,                  // 此变量的类型为JavaCallWrapper，这个变量对于栈展开过程非常重要
        // (intptr_t*)&(result->_value), // see NOTE above (compiler problem)
        result_val_address,          // see NOTE above (compiler problem) result_val_address 函数返回值地址；
        result_type,                 // 函数返回类型；
        method(),                    // 当前要执行的方法。通过此参数可以获取到Java方法所有的元数据信息，包括最重要的字节码信息，这样就可以根据字节码信息解释执行这个方法了；
        // 用于后续帧开辟, entry_point 会从 method() 中获取出 Java Method 的第一个字节码命令(Opcode),也就是整个 Java Method 的调用入口
        entry_point,                 // HotSpot每次在调用Java函数时，必然会调用CallStub函数指针，这个函数指针的值取自_call_stub_entry，HotSpot通过_call_stub_entry指向被调用函数地址。在调用函数之前，必须要先经过entry_point，HotSpot实际是通过entry_point从method()对象上拿到Java方法对应的第1个字节码命令，这也是整个函数的调用入口；
        args->parameters(),          // 描述Java函数的入参信息；
        args->size_of_parameters(),  // 参数需要占用的，以字为单位的内存大小
        CHECK                        // 当前线程对象。
      );

      result = link.result();  // circumvent MS C++ 5.0 compiler bug (result is clobbered across call)
      // Preserve oop return value across possible gc points
      if (oop_result_flag) {
        thread->set_vm_result((oop) result->get_jobject());
      }
    }
  } // Exit JavaCallWrapper (can block - potential return oop must be preserved)

  // Check if a thread stop or suspend should be executed
  // The following assert was not realistic.  Thread.stop can set that bit at any moment.
  //assert(!thread->has_special_runtime_exit_condition(), "no async. exceptions should be installed");

  // Restore possible oop return
  if (oop_result_flag) {
    result->set_jobject((jobject)thread->vm_result());
    thread->set_vm_result(NULL);
  }
}


//--------------------------------------------------------------------------------------
// Implementation of JavaCallArguments

inline bool is_value_state_indirect_oop(uint state) {
  assert(state != JavaCallArguments::value_state_oop,
         "Checking for handles after removal");
  assert(state < JavaCallArguments::value_state_limit, "Invalid value state");
  return state != JavaCallArguments::value_state_primitive;
}

inline oop resolve_indirect_oop(intptr_t value, uint state) {
  switch (state) {
  case JavaCallArguments::value_state_handle:
  {
    oop* ptr = reinterpret_cast<oop*>(value);
    return Handle::raw_resolve(ptr);
  }

  case JavaCallArguments::value_state_jobject:
  {
    jobject obj = reinterpret_cast<jobject>(value);
    return JNIHandles::resolve(obj);
  }

  default:
    ShouldNotReachHere();
    return NULL;
  }
}

intptr_t* JavaCallArguments::parameters() {
  // First convert all handles to oops
  for(int i = 0; i < _size; i++) {
    uint state = _value_state[i];
    assert(state != value_state_oop, "Multiple handle conversions");
    if (is_value_state_indirect_oop(state)) {
      oop obj = resolve_indirect_oop(_value[i], state);
      _value[i] = cast_from_oop<intptr_t>(obj);
      _value_state[i] = value_state_oop;
    }
  }
  // Return argument vector
  return _value;
}


class SignatureChekker : public SignatureIterator {
 private:
  int _pos;
  BasicType _return_type;
  u_char* _value_state;
  intptr_t* _value;

 public:
  bool _is_return;

  SignatureChekker(Symbol* signature,
                   BasicType return_type,
                   bool is_static,
                   u_char* value_state,
                   intptr_t* value) :
    SignatureIterator(signature),
    _pos(0),
    _return_type(return_type),
    _value_state(value_state),
    _value(value),
    _is_return(false)
  {
    if (!is_static) {
      check_value(true); // Receiver must be an oop
    }
  }

  void check_value(bool type) {
    uint state = _value_state[_pos++];
    if (type) {
      guarantee(is_value_state_indirect_oop(state),
                "signature does not match pushed arguments");
    } else {
      guarantee(state == JavaCallArguments::value_state_primitive,
                "signature does not match pushed arguments");
    }
  }

  void check_doing_return(bool state) { _is_return = state; }

  void check_return_type(BasicType t) {
    guarantee(_is_return && t == _return_type, "return type does not match");
  }

  void check_int(BasicType t) {
    if (_is_return) {
      check_return_type(t);
      return;
    }
    check_value(false);
  }

  void check_double(BasicType t) { check_long(t); }

  void check_long(BasicType t) {
    if (_is_return) {
      check_return_type(t);
      return;
    }

    check_value(false);
    check_value(false);
  }

  void check_obj(BasicType t) {
    if (_is_return) {
      check_return_type(t);
      return;
    }

    intptr_t v = _value[_pos];
    if (v != 0) {
      // v is a "handle" referring to an oop, cast to integral type.
      // There shouldn't be any handles in very low memory.
      guarantee((size_t)v >= (size_t)os::vm_page_size(),
                "Bad JNI oop argument");
      // Verify the pointee.
      oop vv = resolve_indirect_oop(v, _value_state[_pos]);
      guarantee(vv->is_oop_or_null(true),
                "Bad JNI oop argument");
    }

    check_value(true);          // Verify value state.
  }

  void do_bool()                       { check_int(T_BOOLEAN);       }
  void do_char()                       { check_int(T_CHAR);          }
  void do_float()                      { check_int(T_FLOAT);         }
  void do_double()                     { check_double(T_DOUBLE);     }
  void do_byte()                       { check_int(T_BYTE);          }
  void do_short()                      { check_int(T_SHORT);         }
  void do_int()                        { check_int(T_INT);           }
  void do_long()                       { check_long(T_LONG);         }
  void do_void()                       { check_return_type(T_VOID);  }
  void do_object(int begin, int end)   { check_obj(T_OBJECT);        }
  void do_array(int begin, int end)    { check_obj(T_OBJECT);        }
};


void JavaCallArguments::verify(methodHandle method, BasicType return_type) {
  guarantee(method->size_of_parameters() == size_of_parameters(), "wrong no. of arguments pushed");

  // Treat T_OBJECT and T_ARRAY as the same
  if (return_type == T_ARRAY) return_type = T_OBJECT;

  // Check that oop information is correct
  Symbol* signature = method->signature();

  SignatureChekker sc(signature,
                      return_type,
                      method->is_static(),
                      _value_state,
                      _value);
  sc.iterate_parameters();
  sc.check_doing_return(true);
  sc.iterate_returntype();
}
