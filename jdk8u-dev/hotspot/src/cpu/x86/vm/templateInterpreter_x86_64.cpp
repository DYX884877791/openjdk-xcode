/*
 * Copyright (c) 2003, 2017, Oracle and/or its affiliates. All rights reserved.
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
#include "asm/macroAssembler.hpp"
#include "interpreter/bytecodeHistogram.hpp"
#include "interpreter/interpreter.hpp"
#include "interpreter/interpreterGenerator.hpp"
#include "interpreter/interpreterRuntime.hpp"
#include "interpreter/templateTable.hpp"
#include "oops/arrayOop.hpp"
#include "oops/methodData.hpp"
#include "oops/method.hpp"
#include "oops/oop.inline.hpp"
#include "prims/jvmtiExport.hpp"
#include "prims/jvmtiThreadState.hpp"
#include "runtime/arguments.hpp"
#include "runtime/deoptimization.hpp"
#include "runtime/frame.inline.hpp"
#include "runtime/sharedRuntime.hpp"
#include "runtime/stubRoutines.hpp"
#include "runtime/synchronizer.hpp"
#include "runtime/timer.hpp"
#include "runtime/vframeArray.hpp"
#include "utilities/debug.hpp"
#include "utilities/macros.hpp"
#include "utilities/slog.hpp"

#define __ _masm->

#ifndef CC_INTERP

const int method_offset = frame::interpreter_frame_method_offset * wordSize;
const int bci_offset    = frame::interpreter_frame_bcx_offset    * wordSize;
const int locals_offset = frame::interpreter_frame_locals_offset * wordSize;

//-----------------------------------------------------------------------------

address TemplateInterpreterGenerator::generate_StackOverflowError_handler() {
  address entry = __ pc();

#ifdef ASSERT
  {
    Label L;
    __ lea(rax, Address(rbp,
                        frame::interpreter_frame_monitor_block_top_offset *
                        wordSize));
    __ cmpptr(rax, rsp); // rax = maximal rsp for current rbp (stack
                         // grows negative)
    __ jcc(Assembler::aboveEqual, L); // check if frame is complete
    __ stop ("interpreter frame not set up");
    __ bind(L);
  }
#endif // ASSERT
  // Restore bcp under the assumption that the current frame is still
  // interpreted
  __ restore_bcp();

  // expression stack must be empty before entering the VM if an
  // exception happened
  __ empty_expression_stack();
  // throw exception
  __ call_VM(noreg,
             CAST_FROM_FN_PTR(address,
                              InterpreterRuntime::throw_StackOverflowError));
  return entry;
}

address TemplateInterpreterGenerator::generate_ArrayIndexOutOfBounds_handler(
        const char* name) {
  address entry = __ pc();
  // expression stack must be empty before entering the VM if an
  // exception happened
  __ empty_expression_stack();
  // setup parameters
  // ??? convention: expect aberrant index in register ebx
  __ lea(c_rarg1, ExternalAddress((address)name));
  __ call_VM(noreg,
             CAST_FROM_FN_PTR(address,
                              InterpreterRuntime::
                              throw_ArrayIndexOutOfBoundsException),
             c_rarg1, rbx);
  return entry;
}

address TemplateInterpreterGenerator::generate_ClassCastException_handler() {
  address entry = __ pc();

  // object is at TOS
  __ pop(c_rarg1);

  // expression stack must be empty before entering the VM if an
  // exception happened
  __ empty_expression_stack();

  __ call_VM(noreg,
             CAST_FROM_FN_PTR(address,
                              InterpreterRuntime::
                              throw_ClassCastException),
             c_rarg1);
  return entry;
}

address TemplateInterpreterGenerator::generate_exception_handler_common(
        const char* name, const char* message, bool pass_oop) {
  assert(!pass_oop || message == NULL, "either oop or message but not both");
  address entry = __ pc();
  if (pass_oop) {
    // object is at TOS
    __ pop(c_rarg2);
  }
  // expression stack must be empty before entering the VM if an
  // exception happened
  __ empty_expression_stack();
  // setup parameters
  __ lea(c_rarg1, ExternalAddress((address)name));
  if (pass_oop) {
    __ call_VM(rax, CAST_FROM_FN_PTR(address,
                                     InterpreterRuntime::
                                     create_klass_exception),
               c_rarg1, c_rarg2);
  } else {
    // kind of lame ExternalAddress can't take NULL because
    // external_word_Relocation will assert.
    if (message != NULL) {
      __ lea(c_rarg2, ExternalAddress((address)message));
    } else {
      __ movptr(c_rarg2, NULL_WORD);
    }
    __ call_VM(rax,
               CAST_FROM_FN_PTR(address, InterpreterRuntime::create_exception),
               c_rarg1, c_rarg2);
  }
  // throw exception
  __ jump(ExternalAddress(Interpreter::throw_exception_entry()));
  return entry;
}


address TemplateInterpreterGenerator::generate_continuation_for(TosState state) {
  address entry = __ pc();
  // NULL last_sp until next java call
  __ movptr(Address(rbp, frame::interpreter_frame_last_sp_offset * wordSize), (int32_t)NULL_WORD);
  __ dispatch_next(state);
  return entry;
}


address TemplateInterpreterGenerator::generate_return_entry_for(TosState state, int step, size_t index_size) {
    slog_trace("进入hotspot/src/cpu/x86/vm/templateInterpreter_x86_64.cpp中的TemplateInterpreterGenerator::generate_return_entry_for函数...");
  address entry = __ pc();

  // Restore stack bottom in case i2c adjusted stack
  __ movptr(rsp, Address(rbp, frame::interpreter_frame_last_sp_offset * wordSize));
  // and NULL it as marker that esp is now tos until next java call
  __ movptr(Address(rbp, frame::interpreter_frame_last_sp_offset * wordSize), (int32_t)NULL_WORD);

  __ restore_bcp();
  __ restore_locals();

  if (state == atos) {
    Register mdp = rbx;
    Register tmp = rcx;
    __ profile_return_type(mdp, rax, tmp);
  }

  const Register cache = rbx;
  const Register index = rcx;
  __ get_cache_and_index_at_bcp(cache, index, 1, index_size);

  const Register flags = cache;
  __ movl(flags, Address(cache, index, Address::times_ptr, ConstantPoolCache::base_offset() + ConstantPoolCacheEntry::flags_offset()));
  __ andl(flags, ConstantPoolCacheEntry::parameter_size_mask);
  __ lea(rsp, Address(rsp, flags, Interpreter::stackElementScale()));
  __ dispatch_next(state, step);

  return entry;
}


address TemplateInterpreterGenerator::generate_deopt_entry_for(TosState state,
                                                               int step) {
  address entry = __ pc();
  // NULL last_sp until next java call
  __ movptr(Address(rbp, frame::interpreter_frame_last_sp_offset * wordSize), (int32_t)NULL_WORD);
  __ restore_bcp();
  __ restore_locals();
  // handle exceptions
  {
    Label L;
    __ cmpptr(Address(r15_thread, Thread::pending_exception_offset()), (int32_t) NULL_WORD);
    __ jcc(Assembler::zero, L);
    __ call_VM(noreg,
               CAST_FROM_FN_PTR(address,
                                InterpreterRuntime::throw_pending_exception));
    __ should_not_reach_here();
    __ bind(L);
  }
  __ dispatch_next(state, step);
  return entry;
}

int AbstractInterpreter::BasicType_as_index(BasicType type) {
  int i = 0;
  switch (type) {
    case T_BOOLEAN: i = 0; break;
    case T_CHAR   : i = 1; break;
    case T_BYTE   : i = 2; break;
    case T_SHORT  : i = 3; break;
    case T_INT    : i = 4; break;
    case T_LONG   : i = 5; break;
    case T_VOID   : i = 6; break;
    case T_FLOAT  : i = 7; break;
    case T_DOUBLE : i = 8; break;
    case T_OBJECT : i = 9; break;
    case T_ARRAY  : i = 9; break;
    default       : ShouldNotReachHere();
  }
  assert(0 <= i && i < AbstractInterpreter::number_of_result_handlers,
         "index out of bounds");
  return i;
}


address TemplateInterpreterGenerator::generate_result_handler_for(
        BasicType type) {
  address entry = __ pc();
  switch (type) {
  case T_BOOLEAN: __ c2bool(rax);            break;
  case T_CHAR   : __ movzwl(rax, rax);       break;
  case T_BYTE   : __ sign_extend_byte(rax);  break;
  case T_SHORT  : __ sign_extend_short(rax); break;
  case T_INT    : /* nothing to do */        break;
  case T_LONG   : /* nothing to do */        break;
  case T_VOID   : /* nothing to do */        break;
  case T_FLOAT  : /* nothing to do */        break;
  case T_DOUBLE : /* nothing to do */        break;
  case T_OBJECT :
    // retrieve result from frame
    __ movptr(rax, Address(rbp, frame::interpreter_frame_oop_temp_offset*wordSize));
    // and verify it
    __ verify_oop(rax);
    break;
  default       : ShouldNotReachHere();
  }
  __ ret(0);                                   // return from result handler
  return entry;
}

address TemplateInterpreterGenerator::generate_safept_entry_for(
        TosState state,
        address runtime_entry) {
  address entry = __ pc();
  __ push(state);
  __ call_VM(noreg, runtime_entry);
  __ dispatch_via(vtos, Interpreter::_normal_table.table_for(vtos));
  return entry;
}



// Helpers for commoning out cases in the various type of method entries.
//


// increment invocation count & check for overflow
//
// Note: checking for negative value instead of overflow
//       so we have a 'sticky' overflow test
//
// rbx: method
// ecx: invocation counter
// InterpreterGenerator::generate_counter_incr方法主要用于增加MethodData或者MethodCounters中的调用计数属性，
// 并在超过阈值时跳转到特定的分支，具体跳转的目标地址需要结合InterpreterGenerator::generate_normal_entry方法的实现
void InterpreterGenerator::generate_counter_incr(
        Label* overflow,
        Label* profile_method,
        Label* profile_method_continue) {
  Label done;
  // Note: In tiered we increment either counters in Method* or in MDO depending if we're profiling or not.
    //如果启用分级编译，server模式下默认启用
  if (TieredCompilation) {
      //因为InvocationCounter的_counter中调用计数部分是前29位，所以增加一次调用计数不是从1开始，而是1<<3即8
    int increment = InvocationCounter::count_increment;
      //Tier0InvokeNotifyFreqLog默认值是7，count_shift是_counter属性中非调用计数部分的位数，这里是3
    int mask = ((1 << Tier0InvokeNotifyFreqLog)  - 1) << InvocationCounter::count_shift;
    Label no_mdo;
      //如果开启性能收集
    if (ProfileInterpreter) {
      // Are we profiling?
        //校验Method中的_method_data属性非空，如果为空则跳转到no_mdo
      __ movptr(rax, Address(rbx, Method::method_data_offset()));
      __ testptr(rax, rax);
      __ jccb(Assembler::zero, no_mdo);
      // Increment counter in the MDO
        //获取MethodData的_invocation_counter属性的_counter属性的地址
      const Address mdo_invocation_counter(rax, in_bytes(MethodData::invocation_counter_offset()) +
                                                in_bytes(InvocationCounter::counter_offset()));
        //此时rcx中的值无意义
      __ increment_mask_and_jump(mdo_invocation_counter, increment, mask, rcx, false, Assembler::zero, overflow);
      __ jmp(done);
    }
    __ bind(no_mdo);
    // Increment counter in MethodCounters
      //获取MethodCounters的_invocation_counter属性的_counter属性的地址，get_method_counters方法会将MethodCounters的地址放入rax中
    const Address invocation_counter(rax,
                  MethodCounters::invocation_counter_offset() +
                  InvocationCounter::counter_offset());
      //获取MethodCounters的地址并将其放入rax中
    __ get_method_counters(rbx, rax, done);
      //增加计数
    __ increment_mask_and_jump(invocation_counter, increment, mask, rcx,
                               false, Assembler::zero, overflow);
    __ bind(done);
  } else {
      //获取MethodCounters的_backedge_counter属性的_counter属性的地址
    const Address backedge_counter(rax,
                  MethodCounters::backedge_counter_offset() +
                  InvocationCounter::counter_offset());
      //获取MethodCounters的_invocation_counter属性的_counter属性的地址
    const Address invocation_counter(rax,
                  MethodCounters::invocation_counter_offset() +
                  InvocationCounter::counter_offset());
      //获取MethodCounters的地址并将其放入rax中
    __ get_method_counters(rbx, rax, done);
      //如果开启性能收集
    if (ProfileInterpreter) {
        //因为value为0，所以这里啥都不做
      __ incrementl(Address(rax,
              MethodCounters::interpreter_invocation_counter_offset()));
    }
    // Update standard invocation counters
      //更新invocation_counter
    __ movl(rcx, invocation_counter);
    __ incrementl(rcx, InvocationCounter::count_increment);
    __ movl(invocation_counter, rcx); // save invocation count

    __ movl(rax, backedge_counter);   // load backedge counter
      //计算出status的位
    __ andl(rax, InvocationCounter::count_mask_value); // mask out the status bits
      //将rcx中的调用计数同rax中的status做且运算
    __ addl(rcx, rax);                // add both counters

    // profile_method is non-null only for interpreted method so
    // profile_method != NULL == !native_call

    if (ProfileInterpreter && profile_method != NULL) {
      // Test to see if we should create a method data oop
        //如果rcx的值小于InterpreterProfileLimit，则跳转到profile_method_continue
      __ cmp32(rcx, ExternalAddress((address)&InvocationCounter::InterpreterProfileLimit));
      __ jcc(Assembler::less, *profile_method_continue);

      // if no method data exists, go to profile_method
        //如果大于，则校验methodData是否存在，如果不存在则跳转到profile_method
      __ test_method_data_pointer(rax, *profile_method);
    }
    //比较rcx的值是否超过InterpreterInvocationLimit，如果大于等于则跳转到overflow
    __ cmp32(rcx, ExternalAddress((address)&InvocationCounter::InterpreterInvocationLimit));
    __ jcc(Assembler::aboveEqual, *overflow);
    __ bind(done);
  }
}

// InterpreterGenerator::generate_counter_overflow方法用于处理方法调用计数超过阈值的情形，是触发方法编译的入口
// generate_counter_overflow方法触发的主要是方法调用次数超过阈值这种情形下的方法编译，这种编译不是立即执行的，
// 不需要做栈上替换，而是提交一个任务给后台编译线程，编译线程编译完成后自动完成相关替换。
void InterpreterGenerator::generate_counter_overflow(Label* do_continue) {

  // Asm interpreter on entry
  // r14 - locals
  // r13 - bcp
  // rbx - method
  // edx - cpool --- DOES NOT APPEAR TO BE TRUE
  // rbp - interpreter frame

  // On return (i.e. jump to entry_point) [ back to invocation of interpreter ]
  // Everything as it was on entry
  // rdx is not restored. Doesn't appear to really be set.

  // InterpreterRuntime::frequency_counter_overflow takes two
  // arguments, the first (thread) is passed by call_VM, the second
  // indicates if the counter overflow occurs at a backwards branch
  // (NULL bcp).  We pass zero for it.  The call returns the address
  // of the verified entry point for the method or NULL if the
  // compilation did not complete (either went background or bailed
  // out).
    //InterpreterRuntime::frequency_counter_overflow需要两个参数，第一个参数thread在执行call_VM时传递，第二个参数表明
    //调用计数超过阈值是否发生在循环分支上，如果否则传递NULL，我们传递0，即NULL，如果是则传该循环的跳转分支地址
    //这个方法返回编译后的方法的入口地址，如果编译没有完成则返回NULL
  __ movl(c_rarg1, 0);
  __ call_VM(noreg,
             CAST_FROM_FN_PTR(address,
                              InterpreterRuntime::frequency_counter_overflow),
             c_rarg1);
    //恢复rbx中的Method*，method_offset是全局变量
  __ movptr(rbx, Address(rbp, method_offset));   // restore Method*
  // Preserve invariant that r13/r14 contain bcp/locals of sender frame
  // and jump to the interpreted entry.
    //跳转到do_continue标签
  __ jmp(*do_continue, relocInfo::none);
}

// See if we've got enough room on the stack for locals plus overhead.
// The expression stack grows down incrementally, so the normal guard
// page mechanism will work for that.
//
// NOTE: Since the additional locals are also always pushed (wasn't
// obvious in generate_method_entry) so the guard should work for them
// too.
//
// Args:
//      rdx: number of additional locals this frame needs (what we must check)
//      rbx: Method*
//
// Kills:
//      rax
void InterpreterGenerator::generate_stack_overflow_check(void) {

  // monitor entry size: see picture of stack set
  // (generate_method_entry) and frame_amd64.hpp
  const int entry_size = frame::interpreter_frame_monitor_size() * wordSize;

  // total overhead size: entry_size + (saved rbp through expr stack
  // bottom).  be sure to change this if you add/subtract anything
  // to/from the overhead area
  const int overhead_size =
    -(frame::interpreter_frame_initial_sp_offset * wordSize) + entry_size;

  const int page_size = os::vm_page_size();

  Label after_frame_check;

  // see if the frame is greater than one page in size. If so,
  // then we need to verify there is enough stack space remaining
  // for the additional locals.
  __ cmpl(rdx, (page_size - overhead_size) / Interpreter::stackElementSize);
  __ jcc(Assembler::belowEqual, after_frame_check);

  // compute rsp as if this were going to be the last frame on
  // the stack before the red zone

  const Address stack_base(r15_thread, Thread::stack_base_offset());
  const Address stack_size(r15_thread, Thread::stack_size_offset());

  // locals + overhead, in bytes
  __ mov(rax, rdx);
  __ shlptr(rax, Interpreter::logStackElementSize);  // 2 slots per parameter.
  __ addptr(rax, overhead_size);

#ifdef ASSERT
  Label stack_base_okay, stack_size_okay;
  // verify that thread stack base is non-zero
  __ cmpptr(stack_base, (int32_t)NULL_WORD);
  __ jcc(Assembler::notEqual, stack_base_okay);
  __ stop("stack base is zero");
  __ bind(stack_base_okay);
  // verify that thread stack size is non-zero
  __ cmpptr(stack_size, 0);
  __ jcc(Assembler::notEqual, stack_size_okay);
  __ stop("stack size is zero");
  __ bind(stack_size_okay);
#endif

  // Add stack base to locals and subtract stack size
  __ addptr(rax, stack_base);
  __ subptr(rax, stack_size);

  // Use the maximum number of pages we might bang.
  const int max_pages = StackShadowPages > (StackRedPages+StackYellowPages) ? StackShadowPages :
                                                                              (StackRedPages+StackYellowPages);

  // add in the red and yellow zone sizes
  __ addptr(rax, max_pages * page_size);

  // check against the current stack bottom
  __ cmpptr(rsp, rax);
  __ jcc(Assembler::above, after_frame_check);

  // Restore sender's sp as SP. This is necessary if the sender's
  // frame is an extended compiled frame (see gen_c2i_adapter())
  // and safer anyway in case of JSR292 adaptations.

  __ pop(rax); // return address must be moved if SP is changed
  __ mov(rsp, r13);
  __ push(rax);

  // Note: the restored frame is not necessarily interpreted.
  // Use the shared runtime version of the StackOverflowError.
  assert(StubRoutines::throw_StackOverflowError_entry() != NULL, "stub not yet generated");
  __ jump(ExternalAddress(StubRoutines::throw_StackOverflowError_entry()));

  // all done with frame size check
  __ bind(after_frame_check);
}

// Allocate monitor and lock method (asm interpreter)
//
// Args:
//      rbx: Method*
//      r14: locals
//
// Kills:
//      rax
//      c_rarg0, c_rarg1, c_rarg2, c_rarg3, ...(param regs)
//      rscratch1, rscratch2 (scratch regs)
void InterpreterGenerator::lock_method(void) {
  // synchronize method
  const Address access_flags(rbx, Method::access_flags_offset());
  const Address monitor_block_top(
        rbp,
        frame::interpreter_frame_monitor_block_top_offset * wordSize);
  const int entry_size = frame::interpreter_frame_monitor_size() * wordSize;

#ifdef ASSERT
  {
    Label L;
    __ movl(rax, access_flags);
    __ testl(rax, JVM_ACC_SYNCHRONIZED);
    __ jcc(Assembler::notZero, L);
    __ stop("method doesn't need synchronization");
    __ bind(L);
  }
#endif // ASSERT

  // get synchronization object
  {
    const int mirror_offset = in_bytes(Klass::java_mirror_offset());
    Label done;
      //将方法的access_flags拷贝到rax中
    __ movl(rax, access_flags);
      //校验这个方法是否是静态方法
    __ testl(rax, JVM_ACC_STATIC);
    // get receiver (assume this is frequent case)
      //将栈顶的执行方法调用的实例拷贝到rax中
    __ movptr(rax, Address(r14, Interpreter::local_offset_in_bytes(0)));
      //如果不是静态方法，则跳转到done
    __ jcc(Assembler::zero, done);
      //如果是静态方法,获取该Method对应的真实Klass，即pool_holder属性
    __ movptr(rax, Address(rbx, Method::const_offset()));
    __ movptr(rax, Address(rax, ConstMethod::constants_offset()));
    __ movptr(rax, Address(rax,
                           ConstantPool::pool_holder_offset_in_bytes()));
      //将Klass的java_mirror属性复制到到rax中，即某个类对应的class实例
    __ movptr(rax, Address(rax, mirror_offset));

#ifdef ASSERT
    {
      Label L;
      __ testptr(rax, rax);
      __ jcc(Assembler::notZero, L);
      __ stop("synchronization object is NULL");
      __ bind(L);
    }
#endif // ASSERT

    __ bind(done);
  }

  // add space for monitor & lock
    //将rsp往下，即低地址端移动entry_size，即一个BasicObjectLock的大小
  __ subptr(rsp, entry_size); // add space for a monitor entry
    //将rsp地址写入栈帧中monitor_block_top地址
  __ movptr(monitor_block_top, rsp);  // set new monitor block top
  // store object
    //将rax即跟锁关联的对象保存到BasicObjectLock的obj属性
  __ movptr(Address(rsp, BasicObjectLock::obj_offset_in_bytes()), rax);
    //将rsp地址拷贝到c_rarg1，即BasicObjectLock实例的地址
  __ movptr(c_rarg1, rsp); // object address
    //调用lock_object加锁，实现加锁的lock_object方法跟 synchronized修饰代码块时调用方法是一样的，都是InterpreterMacroAssembler::lock_object方法
  __ lock_object(c_rarg1);
}

// Generate a fixed interpreter frame. This is identical setup for
// interpreted methods and for native methods hence the shared code.
//
// Args:
//      rax: return address %rax寄存器中存储的是返回地址，注意rax中保存的返回地址，因为在generate_call_stub()函数中通过__ call(c_rarg1) 语句调用了由generate_normal_entry()函数生成的entry_point，所以当entry_point执行完成后，还会返回到generate_call_stub()函数中继续执行__ call(c_rarg1) 语句下面的代码
//      rbx: Method* 要执行的Java方法的指针
//      r14: pointer to locals 本地变量表指针
//      r13: sender sp 调用者的栈顶
//      rdx: cp cache 常量池的地址

// 大概逻辑：
// 1. 返回地址重新入栈，开辟新栈帧
// 2. rbcp值入栈，空出rbcp寄存器
// 3. rbcp指向codebase
// 4. Method*入栈
// 5. Method*镜像压栈
// 6. ConstantPoolCache压栈
// 7. 局部变量表压栈
// 8. 第一条字节码指令压栈
// 9. 操作数栈底压栈

// 对于普通的Java方法来说，生成的汇编代码如下：
// push   %rax
// push   %rbp
// mov    %rsp,%rbp
// push   %r13
// pushq  $0x0
// mov    0x10(%rbx),%r13
// lea    0x30(%r13),%r13 // lea指令获取内存地址本身
// push   %rbx
// mov    0x18(%rbx),%rdx
// test   %rdx,%rdx
// je     0x00007fffed01b27d
// add    $0x90,%rdx
// push   %rdx
// mov    0x10(%rbx),%rdx
// mov    0x8(%rdx),%rdx
// mov    0x18(%rdx),%rdx
// push   %rdx
// push   %r14
// push   %r13
// pushq  $0x0
// mov    %rsp,(%rsp)

void TemplateInterpreterGenerator::generate_fixed_frame(bool native_call) {
    slog_trace("进入hotspot/src/cpu/x86/vm/templateInterpreter_x86_64.cpp中的TemplateInterpreterGenerator::generate_fixed_frame函数...");
  // initialize fixed part of activation frame
    // 把返回地址紧接着局部变量区保存
    //保存rax中的java方法返回地址到栈帧中，局部变量入栈以后将返回地址重新入栈
  __ push(rax);        // save return address
    // 为Java方法创建栈帧
    // 新栈帧，调整sp和bp
    //保存rbp，将rsp的值复制到rbp中
  __ enter();          // save old & set new rbp
    // 保存调用者的栈顶地址
    //将 sender sp保存到栈帧中
  __ push(r13);        // set sender sp
    // 暂时将last_sp属性的值设置为NULL_WORD
    //push一个0，用来保存last_sp
  __ push((int)NULL_WORD); // leave last_sp as null
    // 获取ConstMethod*并保存到r13中
    //将Method的ConstMethod属性的地址放到r13中
  __ movptr(r13, Address(rbx, Method::const_offset()));      // get ConstMethod*
    // 保存Java方法字节码的地址到r13中
    //获取保存字节码的的内存地址，保存到r13中
  __ lea(r13, Address(r13, ConstMethod::codes_offset())); // get codebase
    // 保存Method*到堆栈上
    //将Method*保存到rbx中
  __ push(rbx);        // save Method*
    // ProfileInterpreter属性的默认值为true，
    // 表示需要对解释执行的方法进行相关信息的统计
    //如果统计解释器性能
  if (ProfileInterpreter) {
    Label method_data_continue;
      // MethodData结构基础是ProfileData，
      // 记录函数运行状态下的数据
      // MethodData里面分为3个部分，
      // 一个是函数类型等运行相关统计数据，
      // 一个是参数类型运行相关统计数据，
      // 还有一个是extra扩展区保存着deoptimization的相关信息
      // 获取Method中的_method_data属性的值并保存到rdx中
    __ movptr(rdx, Address(rbx, in_bytes(Method::method_data_offset())));
    __ testptr(rdx, rdx);
    __ jcc(Assembler::zero, method_data_continue);
      // 执行到这里，说明_method_data已经进行了初始化，
      // 通过MethodData来获取_data属性的值并存储到rdx中
    __ addptr(rdx, in_bytes(MethodData::data_offset()));
    __ bind(method_data_continue);
    __ push(rdx);      // set the mdp (method data pointer)
  } else {
    __ push(0);
  }

    // 获取ConstMethod*存储到rdx
    //获取Method的ConstMethod属性的地址
  __ movptr(rdx, Address(rbx, Method::const_offset()));
    // 获取ConstantPool*存储到rdx
    //获取ConstMethod的ConstantPool地址
  __ movptr(rdx, Address(rdx, ConstMethod::constants_offset()));
    // 获取ConstantPoolCache*并存储到rdx
    //获取ConstantPool的ConstantPoolCache的地址
  __ movptr(rdx, Address(rdx, ConstantPool::cache_offset_in_bytes()));
    // 保存ConstantPoolCache*到堆栈上
    //将ConstantPool的ConstantPoolCache的地址保存到栈帧中
  __ push(rdx); // set constant pool cache
    // 保存第1个参数的地址到堆栈上
    //将方法入参的地址，即本地变量表的起始地址放入栈帧中
  __ push(r14); // set locals pointer
  if (native_call) {
      // native方法调用时，不需要保存Java方法的字节码地址，因为没有字节码
    __ push(0); // no bcp
  } else {
      // 保存Java方法字节码地址到堆栈上，
      // 注意上面对r13寄存器的值进行了更改
      //将字节码的起始地址放入栈帧中
    __ push(r13); // set bcp
  }
    // 预先保留一个slot，后面有大用处
    //将0放入栈帧中，标识栈顶
  __ push(0); // reserve word for pointer to expression stack bottom
    // 将栈底地址保存到这个slot上
    //将rsp的地址放入栈帧中
  __ movptr(Address(rsp, 0), rsp); // set expression stack bottom
}

// End of helpers

// Various method entries
//------------------------------------------------------------------------------------------------------------------------
//
//

// Call an accessor method (assuming it is resolved, otherwise drop
// into vanilla (slow path) entry
address InterpreterGenerator::generate_accessor_entry(void) {
  // rbx: Method*

  // r13: senderSP must preserver for slow path, set SP to it on fast path

  address entry_point = __ pc();
  Label xreturn_path;

  // do fastpath for resolved accessor methods
  if (UseFastAccessorMethods) {
    // Code: _aload_0, _(i|a)getfield, _(i|a)return or any rewrites
    //       thereof; parameter size = 1
    // Note: We can only use this code if the getfield has been resolved
    //       and if we don't have a null-pointer exception => check for
    //       these conditions first and use slow path if necessary.
    Label slow_path;
    // If we need a safepoint check, generate full interpreter entry.
    __ cmp32(ExternalAddress(SafepointSynchronize::address_of_state()),
             SafepointSynchronize::_not_synchronized);

    __ jcc(Assembler::notEqual, slow_path);
    // rbx: method
    __ movptr(rax, Address(rsp, wordSize));

    // check if local 0 != NULL and read field
    __ testptr(rax, rax);
    __ jcc(Assembler::zero, slow_path);

    // read first instruction word and extract bytecode @ 1 and index @ 2
    __ movptr(rdx, Address(rbx, Method::const_offset()));
    __ movptr(rdi, Address(rdx, ConstMethod::constants_offset()));
    __ movl(rdx, Address(rdx, ConstMethod::codes_offset()));
    // Shift codes right to get the index on the right.
    // The bytecode fetched looks like <index><0xb4><0x2a>
    __ shrl(rdx, 2 * BitsPerByte);
    __ shll(rdx, exact_log2(in_words(ConstantPoolCacheEntry::size())));
    __ movptr(rdi, Address(rdi, ConstantPool::cache_offset_in_bytes()));

    // rax: local 0
    // rbx: method
    // rdx: constant pool cache index
    // rdi: constant pool cache

    // check if getfield has been resolved and read constant pool cache entry
    // check the validity of the cache entry by testing whether _indices field
    // contains Bytecode::_getfield in b1 byte.
    assert(in_words(ConstantPoolCacheEntry::size()) == 4,
           "adjust shift below");
    __ movl(rcx,
            Address(rdi,
                    rdx,
                    Address::times_8,
                    ConstantPoolCache::base_offset() +
                    ConstantPoolCacheEntry::indices_offset()));
    __ shrl(rcx, 2 * BitsPerByte);
    __ andl(rcx, 0xFF);
    __ cmpl(rcx, Bytecodes::_getfield);
    __ jcc(Assembler::notEqual, slow_path);

    // Note: constant pool entry is not valid before bytecode is resolved
    __ movptr(rcx,
              Address(rdi,
                      rdx,
                      Address::times_8,
                      ConstantPoolCache::base_offset() +
                      ConstantPoolCacheEntry::f2_offset()));
    // edx: flags
    __ movl(rdx,
            Address(rdi,
                    rdx,
                    Address::times_8,
                    ConstantPoolCache::base_offset() +
                    ConstantPoolCacheEntry::flags_offset()));

    Label notObj, notInt, notByte, notBool, notShort;
    const Address field_address(rax, rcx, Address::times_1);

    // Need to differentiate between igetfield, agetfield, bgetfield etc.
    // because they are different sizes.
    // Use the type from the constant pool cache
    __ shrl(rdx, ConstantPoolCacheEntry::tos_state_shift);
    // Make sure we don't need to mask edx after the above shift
    ConstantPoolCacheEntry::verify_tos_state_shift();

    __ cmpl(rdx, atos);
    __ jcc(Assembler::notEqual, notObj);
    // atos
    __ load_heap_oop(rax, field_address);
    __ jmp(xreturn_path);

    __ bind(notObj);
    __ cmpl(rdx, itos);
    __ jcc(Assembler::notEqual, notInt);
    // itos
    __ movl(rax, field_address);
    __ jmp(xreturn_path);

    __ bind(notInt);
    __ cmpl(rdx, btos);
    __ jcc(Assembler::notEqual, notByte);
    // btos
    __ load_signed_byte(rax, field_address);
    __ jmp(xreturn_path);

    __ bind(notByte);
    __ cmpl(rdx, ztos);
    __ jcc(Assembler::notEqual, notBool);
    // ztos
    __ load_signed_byte(rax, field_address);
    __ jmp(xreturn_path);

    __ bind(notBool);
    __ cmpl(rdx, stos);
    __ jcc(Assembler::notEqual, notShort);
    // stos
    __ load_signed_short(rax, field_address);
    __ jmp(xreturn_path);

    __ bind(notShort);
#ifdef ASSERT
    Label okay;
    __ cmpl(rdx, ctos);
    __ jcc(Assembler::equal, okay);
    __ stop("what type is this?");
    __ bind(okay);
#endif
    // ctos
    __ load_unsigned_short(rax, field_address);

    __ bind(xreturn_path);

    // _ireturn/_areturn
    __ pop(rdi);
    __ mov(rsp, r13);
    __ jmp(rdi);
    __ ret(0);

    // generate a vanilla interpreter entry as the slow path
    __ bind(slow_path);
    (void) generate_normal_entry(false);
  } else {
    (void) generate_normal_entry(false);
  }

  return entry_point;
}

// Method entry for java.lang.ref.Reference.get.
address InterpreterGenerator::generate_Reference_get_entry(void) {
#if INCLUDE_ALL_GCS
  // Code: _aload_0, _getfield, _areturn
  // parameter size = 1
  //
  // The code that gets generated by this routine is split into 2 parts:
  //    1. The "intrinsified" code for G1 (or any SATB based GC),
  //    2. The slow path - which is an expansion of the regular method entry.
  //
  // Notes:-
  // * In the G1 code we do not check whether we need to block for
  //   a safepoint. If G1 is enabled then we must execute the specialized
  //   code for Reference.get (except when the Reference object is null)
  //   so that we can log the value in the referent field with an SATB
  //   update buffer.
  //   If the code for the getfield template is modified so that the
  //   G1 pre-barrier code is executed when the current method is
  //   Reference.get() then going through the normal method entry
  //   will be fine.
  // * The G1 code can, however, check the receiver object (the instance
  //   of java.lang.Reference) and jump to the slow path if null. If the
  //   Reference object is null then we obviously cannot fetch the referent
  //   and so we don't need to call the G1 pre-barrier. Thus we can use the
  //   regular method entry code to generate the NPE.
  //
  // This code is based on generate_accessor_enty.
  //
  // rbx: Method*

  // r13: senderSP must preserve for slow path, set SP to it on fast path

  address entry = __ pc();

  const int referent_offset = java_lang_ref_Reference::referent_offset;
  guarantee(referent_offset > 0, "referent offset not initialized");

  if (UseG1GC) {
    Label slow_path;
    // rbx: method

    // Check if local 0 != NULL
    // If the receiver is null then it is OK to jump to the slow path.
    __ movptr(rax, Address(rsp, wordSize));

    __ testptr(rax, rax);
    __ jcc(Assembler::zero, slow_path);

    // rax: local 0
    // rbx: method (but can be used as scratch now)
    // rdx: scratch
    // rdi: scratch

    // Generate the G1 pre-barrier code to log the value of
    // the referent field in an SATB buffer.

    // Load the value of the referent field.
    const Address field_address(rax, referent_offset);
    __ load_heap_oop(rax, field_address);

    // Generate the G1 pre-barrier code to log the value of
    // the referent field in an SATB buffer.
    __ g1_write_barrier_pre(noreg /* obj */,
                            rax /* pre_val */,
                            r15_thread /* thread */,
                            rbx /* tmp */,
                            true /* tosca_live */,
                            true /* expand_call */);

    // _areturn
    __ pop(rdi);                // get return address
    __ mov(rsp, r13);           // set sp to sender sp
    __ jmp(rdi);
    __ ret(0);

    // generate a vanilla interpreter entry as the slow path
    __ bind(slow_path);
    (void) generate_normal_entry(false);

    return entry;
  }
#endif // INCLUDE_ALL_GCS

  // If G1 is not enabled then attempt to go through the accessor entry point
  // Reference.get is an accessor
  return generate_accessor_entry();
}

/**
 * Method entry for static native methods:
 *   int java.util.zip.CRC32.update(int crc, int b)
 */
address InterpreterGenerator::generate_CRC32_update_entry() {
  if (UseCRC32Intrinsics) {
    address entry = __ pc();

    // rbx,: Method*
    // r13: senderSP must preserved for slow path, set SP to it on fast path
    // c_rarg0: scratch (rdi on non-Win64, rcx on Win64)
    // c_rarg1: scratch (rsi on non-Win64, rdx on Win64)

    Label slow_path;
    // If we need a safepoint check, generate full interpreter entry.
    ExternalAddress state(SafepointSynchronize::address_of_state());
    __ cmp32(ExternalAddress(SafepointSynchronize::address_of_state()),
             SafepointSynchronize::_not_synchronized);
    __ jcc(Assembler::notEqual, slow_path);

    // We don't generate local frame and don't align stack because
    // we call stub code and there is no safepoint on this path.

    // Load parameters
    const Register crc = rax;  // crc
    const Register val = c_rarg0;  // source java byte value
    const Register tbl = c_rarg1;  // scratch

    // Arguments are reversed on java expression stack
    __ movl(val, Address(rsp,   wordSize)); // byte value
    __ movl(crc, Address(rsp, 2*wordSize)); // Initial CRC

    __ lea(tbl, ExternalAddress(StubRoutines::crc_table_addr()));
    __ notl(crc); // ~crc
    __ update_byte_crc32(crc, val, tbl);
    __ notl(crc); // ~crc
    // result in rax

    // _areturn
    __ pop(rdi);                // get return address
    __ mov(rsp, r13);           // set sp to sender sp
    __ jmp(rdi);

    // generate a vanilla native entry as the slow path
    __ bind(slow_path);

    (void) generate_native_entry(false);

    return entry;
  }
  return generate_native_entry(false);
}

/**
 * Method entry for static native methods:
 *   int java.util.zip.CRC32.updateBytes(int crc, byte[] b, int off, int len)
 *   int java.util.zip.CRC32.updateByteBuffer(int crc, long buf, int off, int len)
 */
address InterpreterGenerator::generate_CRC32_updateBytes_entry(AbstractInterpreter::MethodKind kind) {
  if (UseCRC32Intrinsics) {
    address entry = __ pc();

    // rbx,: Method*
    // r13: senderSP must preserved for slow path, set SP to it on fast path

    Label slow_path;
    // If we need a safepoint check, generate full interpreter entry.
    ExternalAddress state(SafepointSynchronize::address_of_state());
    __ cmp32(ExternalAddress(SafepointSynchronize::address_of_state()),
             SafepointSynchronize::_not_synchronized);
    __ jcc(Assembler::notEqual, slow_path);

    // We don't generate local frame and don't align stack because
    // we call stub code and there is no safepoint on this path.

    // Load parameters
    const Register crc = c_rarg0;  // crc
    const Register buf = c_rarg1;  // source java byte array address
    const Register len = c_rarg2;  // length
    const Register off = len;      // offset (never overlaps with 'len')

    // Arguments are reversed on java expression stack
    // Calculate address of start element
    if (kind == Interpreter::java_util_zip_CRC32_updateByteBuffer) {
      __ movptr(buf, Address(rsp, 3*wordSize)); // long buf
      __ movl2ptr(off, Address(rsp, 2*wordSize)); // offset
      __ addq(buf, off); // + offset
      __ movl(crc,   Address(rsp, 5*wordSize)); // Initial CRC
    } else {
      __ movptr(buf, Address(rsp, 3*wordSize)); // byte[] array
      __ addptr(buf, arrayOopDesc::base_offset_in_bytes(T_BYTE)); // + header size
      __ movl2ptr(off, Address(rsp, 2*wordSize)); // offset
      __ addq(buf, off); // + offset
      __ movl(crc,   Address(rsp, 4*wordSize)); // Initial CRC
    }
    // Can now load 'len' since we're finished with 'off'
    __ movl(len, Address(rsp, wordSize)); // Length

    __ super_call_VM_leaf(CAST_FROM_FN_PTR(address, StubRoutines::updateBytesCRC32()), crc, buf, len);
    // result in rax

    // _areturn
    __ pop(rdi);                // get return address
    __ mov(rsp, r13);           // set sp to sender sp
    __ jmp(rdi);

    // generate a vanilla native entry as the slow path
    __ bind(slow_path);

    (void) generate_native_entry(false);

    return entry;
  }
  return generate_native_entry(false);
}

// Interpreter stub for calling a native method. (asm interpreter)
// This sets up a somewhat different looking stack for calling the
// native method than the typical interpreter frame setup.
// InterpreterGenerator::generate_native_entry用来生成本地方法的调用Stub
//用于生成一个从解释器中调用本地方法的调用地址，为了能够执行本地方法需要适当调整栈帧
/**
 * 对比generate_normal_entry方法的实现，两者实现热点代码编译的方法是一样的，不同的是跳转的分支和跳转逻辑稍微有差异，因为本地方法只有普通方法调用触发的编译，
 * 没有循环次数超过阈值触发的栈上替换编译，另外本地方法执行也不需要开启profile收集性能信息。参考CompileBroker::compile_method方法的分析可知，
 * 本地方法不需要走C1/C2编译器编译，因为本地方法的实现本身就已经是经过GCC等编译器优化过的代码，本地方法的编译只是生成了一个更高效的适配器而已，
 * 参考下面的AdapterHandlerLibrary::create_native_wrapper方法的实现。

 * 另外因为本地方法的执行跟普通Java方法不同，generate_native_entry在真正调用本地方法对应的本地代码前会通过InterpreterRuntime::prepare_native_call方法完成本地代码和signature_handler的安装，
 * 然后调用signature_handler完成本地方法调用的参数解析等工作。本地代码调用完成，最终通过result_handler将本地方法执行的结果转换成Java代码可以执行使用的，
 * result_handler是signature_handler解析方法签名获取方法调用结果类型后，根据结果类型设置匹配的result_handler，参考下面的SignatureHandlerGenerator的实现分析。

 * 注意generate_native_entry生成的本地方法的调用Stub跟generate_normal_entry生成的普通Java方法调用Stub一样最终都是赋值给Method的_from_interpreted_entry属性，
 * 即通过Java代码通过解释器调用的，跟普通Java方法触发热点编译，生成的编译代码的入口地址完全是两码事。
 * 本地方法调用普通的Java方法或者本地方法都是通过JNI完成，调用JVM自身的类的相关方法是通过正常的C/C++方法调用完成，
 * JNI调用就是通过JavaCalls::call_helper 到StubRoutines::call_stub到执行generate_normal_entry生成的调用Stub完成的。
 */
address InterpreterGenerator::generate_native_entry(bool synchronized) {
  // determine code generation flags
  bool inc_counter  = UseCompiler || CountCompiledCalls;

  // rbx: Method*
  // r13: sender sp

  address entry_point = __ pc();
    //rbx保存的是Method的地址，根据偏移计算constMethod属性的地址，access_flags属性的地址
  const Address constMethod       (rbx, Method::const_offset());
  const Address access_flags      (rbx, Method::access_flags_offset());
    //根据ConstMethod的地址和size_of_parameters属性的偏移计算该属性的地址
  const Address size_of_parameters(rcx, ConstMethod::
                                        size_of_parameters_offset());


  // get parameter size (always needed)
    //获取方法参数的个数
  __ movptr(rcx, constMethod);
  __ load_unsigned_short(rcx, size_of_parameters);

  // native calls don't need the stack size check since they have no
  // expression stack and the arguments are already on the stack and
  // we only add a handful of words to the stack

  // rbx: Method*
  // rcx: size of parameters
  // r13: sender sp
    //将栈顶的方法调用返回地址pop掉，放入rax中
  __ pop(rax);                                       // get return address

  // for natives the size of locals is zero

  // compute beginning of parameters (r14)
    //根据rsp的地址和参数个数计算起始参数的地址
  __ lea(r14, Address(rsp, rcx, Address::times_8, -wordSize));

  // add 2 zero-initialized slots for native calls
  // initialize result_handler slot
    // 为本地调用初始化两个4字节的数据，其中一个保存result_handler，一个保存oop temp
  __ push((int) NULL_WORD);
  // slot for oop temp
  // (static native method holder mirror/jni oop result)
  __ push((int) NULL_WORD);

  // initialize fixed part of activation frame
    //初始化一个新的栈帧
  generate_fixed_frame(true);

  // make sure method is native & not abstract
#ifdef ASSERT
  __ movl(rax, access_flags);
  {
    Label L;
    __ testl(rax, JVM_ACC_NATIVE);
    __ jcc(Assembler::notZero, L);
    __ stop("tried to execute non-native method as native");
    __ bind(L);
  }
  {
    Label L;
    __ testl(rax, JVM_ACC_ABSTRACT);
    __ jcc(Assembler::zero, L);
    __ stop("tried to execute abstract method in interpreter");
    __ bind(L);
  }
#endif

  // Since at this point in the method invocation the exception handler
  // would try to exit the monitor of synchronized methods which hasn't
  // been entered yet, we set the thread local variable
  // _do_not_unlock_if_synchronized to true. The remove_activation will
  // check this flag.
    //将当前线程的do_not_unlock_if_synchronized置为true
  const Address do_not_unlock_if_synchronized(r15_thread,
        in_bytes(JavaThread::do_not_unlock_if_synchronized_offset()));
  __ movbool(do_not_unlock_if_synchronized, true);

  // increment invocation count & check for overflow
  Label invocation_counter_overflow;
    //如果开启方法编译
  if (inc_counter) {
      //增加方法调用计数
    generate_counter_incr(&invocation_counter_overflow, NULL, NULL);
  }

  Label continue_after_compile;
  __ bind(continue_after_compile);
    //检查shadow_pages，跟异常处理有关
  bang_stack_shadow_pages(true);

  // reset the _do_not_unlock_if_synchronized flag
    // 将do_not_unlock_if_synchronized置为false
  __ movbool(do_not_unlock_if_synchronized, false);

  // check for synchronized methods
  // Must happen AFTER invocation_counter check and stack overflow check,
  // so method is not locked if overflows.
    // 必须在invocation_counter之后执行
  if (synchronized) {
      //获取方法的锁
    lock_method();
  } else {
    // no synchronization necessary ASSERT代码块用于检测该方法的flags不包含ACC_SYNCHRONIZED，即不是synchronized关键字修饰的方法
#ifdef ASSERT
    {
      Label L;
      __ movl(rax, access_flags);
      __ testl(rax, JVM_ACC_SYNCHRONIZED);
      __ jcc(Assembler::zero, L);
      __ stop("method needs synchronization");
      __ bind(L);
    }
#endif
  }

  // start execution
#ifdef ASSERT
  {
    Label L;
    const Address monitor_block_top(rbp,
                 frame::interpreter_frame_monitor_block_top_offset * wordSize);
    __ movptr(rax, monitor_block_top);
    __ cmpptr(rax, rsp);
    __ jcc(Assembler::equal, L);
    __ stop("broken stack frame setup in interpreter");
    __ bind(L);
  }
#endif

  // jvmti support
  //发布JVMTI事件
  __ notify_method_entry();

  // work registers
  const Register method = rbx;
  const Register t      = r11;

  // allocate space for parameters
    //从栈帧中将Method的地址放入rbx中
  __ get_method(method);
  __ movptr(t, Address(method, Method::const_offset()));
    //将方法参数个数放入t中
  __ load_unsigned_short(t, Address(t, ConstMethod::size_of_parameters_offset()));
    //shll为逻辑左移指定位的指令
  __ shll(t, Interpreter::logStackElementSize);
    //重新计算栈顶指针的位置
  __ subptr(rsp, t);
  __ subptr(rsp, frame::arg_reg_save_area_bytes); // windows
  __ andptr(rsp, -16); // must be 16 byte boundary (see amd64 ABI)

  // get signature handler
  {
    Label L;
      //校验method的signature_handler属性非空，该属性只有本地方法有
    __ movptr(t, Address(method, Method::signature_handler_offset()));
    __ testptr(t, t);
    __ jcc(Assembler::notZero, L);
      //调用prepare_native_call确保本地方法已绑定且安装了方法签名解析代码
    __ call_VM(noreg,
               CAST_FROM_FN_PTR(address,
                                InterpreterRuntime::prepare_native_call),
               method);
    __ get_method(method);
      //将signature_handler放入t中
    __ movptr(t, Address(method, Method::signature_handler_offset()));
    __ bind(L);
  }

  // call signature handler
    //86_x64下的from的实现就是返回r14的地址，即起始参数的地址
  assert(InterpreterRuntime::SignatureHandlerGenerator::from() == r14,
         "adjust this code");
  assert(InterpreterRuntime::SignatureHandlerGenerator::to() == rsp,
         "adjust this code");
  assert(InterpreterRuntime::SignatureHandlerGenerator::temp() == rscratch1,
          "adjust this code");

  // The generated handlers do not touch RBX (the method oop).
  // However, large signatures cannot be cached and are generated
  // each time here.  The slow-path generator can do a GC on return,
  // so we must reload it after the call.
    //调用signature_handler，解析方法参数，整个过程一般不会改变rbx，但是慢速处理时可能导致gc，所以调用完成最好重新获取Method
  __ call(t);
  __ get_method(method);        // slow path can do a GC, reload RBX


  // result handler is in rax
  // set result handler
    //将rax中的result handler方法栈帧中，result handler是执行signature_handler返回的，根据方法签名的返回类型获取的
  __ movptr(Address(rbp,
                    (frame::interpreter_frame_result_handler_offset) * wordSize),
            rax);

  // pass mirror handle if static call
  {
    Label L;
    const int mirror_offset = in_bytes(Klass::java_mirror_offset());
    __ movl(t, Address(method, Method::access_flags_offset()));
      //判断是否是static本地方法，如果不是则跳转到L
    __ testl(t, JVM_ACC_STATIC);
    __ jcc(Assembler::zero, L);
    // get mirror
      //如果是static本地方法
    __ movptr(t, Address(method, Method::const_offset()));
    __ movptr(t, Address(t, ConstMethod::constants_offset()));
    __ movptr(t, Address(t, ConstantPool::pool_holder_offset_in_bytes()));
      //获取mirror klass的地址
    __ movptr(t, Address(t, mirror_offset));
    // copy mirror into activation frame
      // 将mirror klass拷贝到栈帧中
    __ movptr(Address(rbp, frame::interpreter_frame_oop_temp_offset * wordSize),
            t);
    // pass handle to mirror
      //  将mirror klass拷贝到c_rarg1中作为静态方法调用的第一个入参
    __ lea(c_rarg1,
           Address(rbp, frame::interpreter_frame_oop_temp_offset * wordSize));
    __ bind(L);
  }

  // get native function entry point
  {
    Label L;
      //获取native_function的地址拷贝到rax中
    __ movptr(rax, Address(method, Method::native_function_offset()));
    ExternalAddress unsatisfied(SharedRuntime::native_method_throw_unsatisfied_link_error_entry());
    __ movptr(rscratch2, unsatisfied.addr());
      //判断rax中的地址是否是native_method_throw_unsatisfied_link_error_entry的地址，如果是说明本地方法未绑定
    __ cmpptr(rax, rscratch2);
      //如果不等于，即已绑定，则跳转到L
    __ jcc(Assembler::notEqual, L);
      //调用InterpreterRuntime::prepare_native_call重试，完成方法绑定
    __ call_VM(noreg,
               CAST_FROM_FN_PTR(address,
                                InterpreterRuntime::prepare_native_call),
               method);
    __ get_method(method);
      //获取native_function的地址拷贝到rax中
    __ movptr(rax, Address(method, Method::native_function_offset()));
    __ bind(L);
  }

  // pass JNIEnv
    // 将当前线程的JNIEnv属性放入c_rarg0
  __ lea(c_rarg0, Address(r15_thread, JavaThread::jni_environment_offset()));

  // It is enough that the pc() points into the right code
  // segment. It does not have to be the correct return pc.
    //保存上一次调用的Java栈帧的rsp，rbp
  __ set_last_Java_frame(rsp, rbp, (address) __ pc());

  // change thread state
#ifdef ASSERT
  {
    Label L;
    __ movl(t, Address(r15_thread, JavaThread::thread_state_offset()));
    __ cmpl(t, _thread_in_Java);
    __ jcc(Assembler::equal, L);
    __ stop("Wrong thread state in native stub");
    __ bind(L);
  }
#endif

  // Change state to native
    // 将线程的状态改成_thread_in_native
  __ movl(Address(r15_thread, JavaThread::thread_state_offset()),
          _thread_in_native);

  // Call the native method.
    //调用本地方法
  __ call(rax);
  // result potentially in rax or xmm0

  // Verify or restore cpu control state after JNI call
    // 方法调用结束校验或者恢复CPU控制状态
  __ restore_cpu_control_state_after_jni();

  // NOTE: The order of these pushes is known to frame::interpreter_frame_result
  // in order to extract the result of a method call. If the order of these
  // pushes change or anything else is added to the stack then the code in
  // interpreter_frame_result must also change.
    //保存rax寄存中方法调用的结果
  __ push(dtos);
  __ push(ltos);

  // change thread state
    //改变线程的状态到_thread_in_native_trans
  __ movl(Address(r15_thread, JavaThread::thread_state_offset()),
          _thread_in_native_trans);
    //如果是多处理器系统
  if (os::is_MP()) {
      //如果使用内存栅栏
    if (UseMembar) {
      // Force this write out before the read below
        //强制内存修改同步到多个处理器，在下面的读开始之前
      __ membar(Assembler::Membar_mask_bits(
           Assembler::LoadLoad | Assembler::LoadStore |
           Assembler::StoreLoad | Assembler::StoreStore));
    } else {
      // Write serialization page so VM thread can do a pseudo remote membar.
      // We use the current thread pointer to calculate a thread specific
      // offset to write to within the page. This minimizes bus traffic
      // due to cache line collision.
      __ serialize_memory(r15_thread, rscratch2);
    }
  }

  // check for safepoint operation in progress and/or pending suspend requests
  {
    Label Continue;
      //判断安全点的状态是否是_not_synchronized
    __ cmp32(ExternalAddress(SafepointSynchronize::address_of_state()),
             SafepointSynchronize::_not_synchronized);

    Label L;
      //如果不等于，即处于安全点则跳转到L
    __ jcc(Assembler::notEqual, L);
    __ cmpl(Address(r15_thread, JavaThread::suspend_flags_offset()), 0);
      //判断当前线程的suspend_flags是否等于0，如果等于则跳转到Continue
    __ jcc(Assembler::equal, Continue);
    __ bind(L);

    // Don't use call_VM as it will see a possible pending exception
    // and forward it and never return here preventing us from
    // clearing _last_native_pc down below.  Also can't use
    // call_VM_leaf either as it will check to see if r13 & r14 are
    // preserved and correspond to the bcp/locals pointers. So we do a
    // runtime call by hand.
    //
      //安全点检查相关操作
    __ mov(c_rarg0, r15_thread);
    __ mov(r12, rsp); // remember sp (can only use r12 if not using call_VM)
    __ subptr(rsp, frame::arg_reg_save_area_bytes); // windows
    __ andptr(rsp, -16); // align stack as required by ABI
    __ call(RuntimeAddress(CAST_FROM_FN_PTR(address, JavaThread::check_special_condition_for_native_trans)));
    __ mov(rsp, r12); // restore sp
    __ reinit_heapbase();
    __ bind(Continue);
  }

  // change thread state
    //线程状态调整成_thread_in_Java
  __ movl(Address(r15_thread, JavaThread::thread_state_offset()), _thread_in_Java);

  // reset_last_Java_frame
    //恢复最近一次的Java栈帧
  __ reset_last_Java_frame(r15_thread, true);

  // reset handle block
    //重置当前线程的JNIHandleBlock
  __ movptr(t, Address(r15_thread, JavaThread::active_handles_offset()));
  __ movl(Address(t, JNIHandleBlock::top_offset_in_bytes()), (int32_t)NULL_WORD);

  // If result is an oop unbox and store it in frame where gc will see it
  // and result handler will pick it up

  {
    Label no_oop;
    __ lea(t, ExternalAddress(AbstractInterpreter::result_handler(T_OBJECT)));
      //比较方法的结果处理程序result_handler是否是T_OBJECT类型的
    __ cmpptr(t, Address(rbp, frame::interpreter_frame_result_handler_offset*wordSize));
      //如果不是则跳转到no_oop
    __ jcc(Assembler::notEqual, no_oop);
    // retrieve result
      //如果是，先把栈顶的long类型的数据即oop地址pop出来放到rax中
    __ pop(ltos);
    // Unbox oop result, e.g. JNIHandles::resolve value.
      //oop校验相关，做必要的垃圾回收处理
    __ resolve_jobject(rax /* value */,
                       r15_thread /* thread */,
                       t /* tmp */);
      //将rax中的oop保存了到栈帧中
    __ movptr(Address(rbp, frame::interpreter_frame_oop_temp_offset*wordSize), rax);
    // keep stack depth as expected by pushing oop which will eventually be discarde
      //重新将rax中的oop放到栈顶
    __ push(ltos);
    __ bind(no_oop);
  }


  {
    Label no_reguard;
      //判断当前线程的_stack_guard_state属性是否是stack_guard_yellow_disabled，即是否发生了stack overflow
    __ cmpl(Address(r15_thread, JavaThread::stack_guard_state_offset()),
            JavaThread::stack_guard_yellow_disabled);
      //如果不等于，即没有发生stack overflow，则跳转到no_reguard
    __ jcc(Assembler::notEqual, no_reguard);
      //如果等于，即发生stack overflow，则调用reguard_yellow_pages做必要的处理
    __ pusha(); // XXX only save smashed registers
    __ mov(r12, rsp); // remember sp (can only use r12 if not using call_VM)
    __ subptr(rsp, frame::arg_reg_save_area_bytes); // windows
    __ andptr(rsp, -16); // align stack as required by ABI
    __ call(RuntimeAddress(CAST_FROM_FN_PTR(address, SharedRuntime::reguard_yellow_pages)));
    __ mov(rsp, r12); // restore sp
    __ popa(); // XXX only restore smashed registers
    __ reinit_heapbase();

    __ bind(no_reguard);
  }


  // The method register is junk from after the thread_in_native transition
  // until here.  Also can't call_VM until the bcp has been
  // restored.  Need bcp for throwing exception below so get it now.
    //重新加载Method
  __ get_method(method);

  // restore r13 to have legal interpreter frame, i.e., bci == 0 <=>
  // r13 == code_base()
  __ movptr(r13, Address(method, Method::const_offset()));   // get ConstMethod*
  __ lea(r13, Address(r13, ConstMethod::codes_offset()));    // get codebase
  // handle exceptions (exception handling will handle unlocking!)
    //处理异常
  {
    Label L;
      //判断当前线程的_pending_exception属性是否为空，即是否发生了异常
    __ cmpptr(Address(r15_thread, Thread::pending_exception_offset()), (int32_t) NULL_WORD);
      //如果为空，即没有异常，跳转到L
    __ jcc(Assembler::zero, L);
    // Note: At some point we may want to unify this with the code
    // used in call_VM_base(); i.e., we should use the
    // StubRoutines::forward_exception code. For now this doesn't work
    // here because the rsp is not correctly set at this point.
    // 调用throw_pending_exception方法抛出异常
    __ MacroAssembler::call_VM(noreg,
                               CAST_FROM_FN_PTR(address,
                               InterpreterRuntime::throw_pending_exception));
    __ should_not_reach_here();
    __ bind(L);
  }

  // do unlocking if necessary
  // 本地方法synchronized解锁逻辑：
  {
    Label L;
      //获取方法的access_flags
    __ movl(t, Address(method, Method::access_flags_offset()));
      //判断目标方法是否是SYNCHRONIZED方法，如果是则需要解锁，如果不是则跳转到L
    __ testl(t, JVM_ACC_SYNCHRONIZED);
      //如果不是synchronized方法跳转到L
    __ jcc(Assembler::zero, L);
    // the code below should be shared with interpreter macro
    // assembler implementation
      //如果是synchronized方法执行解锁
    {
      Label unlock;
      // BasicObjectLock will be first in list, since this is a
      // synchronized method. However, need to check that the object
      // has not been unlocked by an explicit monitorexit bytecode.
        //获取偏向锁BasicObjectLock的地址
        //取出放在Java栈帧头部的BasicObjectLock
      const Address monitor(rbp,
                            (intptr_t)(frame::interpreter_frame_initial_sp_offset *
                                       wordSize - sizeof(BasicObjectLock)));

      // monitor expect in c_rarg1 for slow unlock path
        // 将monitor的地址即关联的BasicObjectLock的地址放入c_rarg1中
      __ lea(c_rarg1, monitor); // address of first monitor
        //获取偏向锁的_obj属性的地址，将BasicObjectLock的obj属性赋值到t中
      __ movptr(t, Address(c_rarg1, BasicObjectLock::obj_offset_in_bytes()));
        //判断_obj属性是否为空
      __ testptr(t, t);
        //如果不为空即未解锁，跳转到unlock完成解锁
      __ jcc(Assembler::notZero, unlock);

      // Entry already unlocked, need to throw exception
        //如果已解锁，说明锁的状态有问题，抛出异常
      __ MacroAssembler::call_VM(noreg,
                                 CAST_FROM_FN_PTR(address,
                   InterpreterRuntime::throw_illegal_monitor_state_exception));
      __ should_not_reach_here();
        //解锁
      __ bind(unlock);
      __ unlock_object(c_rarg1);
    }
    __ bind(L);
  }

  // jvmti support
  // Note: This must happen _after_ handling/throwing any exceptions since
  //       the exception handler code notifies the runtime of method exits
  //       too. If this happens before, method entry/exit notifications are
  //       not properly paired (was bug - gri 11/22/99).
    // 发布jvmti事件
  __ notify_method_exit(vtos, InterpreterMacroAssembler::NotifyJVMTI);

  // restore potential result in edx:eax, call result handler to
  // restore potential result in ST0 & handle result
    //将栈顶的代表方法调用结果的数据pop到rax中
  __ pop(ltos);
  __ pop(dtos);
    //获取result_handler的地址
  __ movptr(t, Address(rbp,
                       (frame::interpreter_frame_result_handler_offset) * wordSize));
    //调用result_handler处理方法调用结果
  __ call(t);

  // remove activation
    // 获取sender sp，开始恢复到上一个Java栈帧
  __ movptr(t, Address(rbp,
                       frame::interpreter_frame_sender_sp_offset *
                       wordSize)); // get sender sp
  __ leave();                                // remove frame anchor
  __ pop(rdi);                               // get return address
  __ mov(rsp, t);                            // set sp to sender sp
  __ jmp(rdi);

  if (inc_counter) {
    // Handle overflow of counter and compile method
      //如果调用计数超过阈值会跳转到此行代码，触发方法的编译
    __ bind(invocation_counter_overflow);
    generate_counter_overflow(&continue_after_compile);
  }

  return entry_point;
}

// 该函数在JVM启动时就会被调用，执行完成之后，会向JVM中的代码缓存区中写入对应的本地机器指令，在JVM调用一个特定的Java方法时，
// 会根据Java方法所对应的entry_point找到对应的函数入口，并执行z这段预先生成好的指令...

// Java方法执行即解释器解释方法的字节码流的实现
// Generic interpreted method entry to (asm) interpreter
// generate_normal_entry()函数会为执行的方法生成堆栈，而堆栈由局部变量表（用来存储传入的参数和被调用方法的局部变量）、Java方法栈帧数据和操作数栈这三大部分组成，
// 所以entry_point例程（其实就是一段机器指令片段，英文名为stub）会创建这3部分来辅助Java方法的执行。

// 该函数最终会返回生成机器码的入口执行地址，然后通过变量_entry_table数组来保存，这样就可以使用方法类型做为数组下标获取对应的方法入口了。

// 这是调用Java方法的最重要的部分
// 这个函数的实现看起来比较多，但其实逻辑实现比较简单，就是根据被调用方法的实际情况创建出对应的局部变量表，然后就是2个非常重要的函数generate_fixed_frame()和dispatch_next()函数了

// 该函数中调用了两个重要的函数 :
// 1.用于增加方法调用计数的InterpreterGenerator::generate_counter_incr方法
// 2. 在方法调用计数达到阈值后自动触发方法编译的InterpreterGenerator::generate_counter_overflow方法，通过这两个方法的实现为线索研究热点代码编译的实现。
// 热点代码编译实际是有两个场景的，一种是正常的方法调用，调用次数超过阈值形成热点，一种是通过for循环执行一段代码，for循环的次数超过阈值形成热点，这两种情形都会触发方法的编译，
// 不同的是前者是提交一个编译任务给后台编译线程，编译完成后通过适配器将对原来字节码的调用转换成对本地代码的调用，后者是尽可能的立即完成编译，
// 并且在完成必要的栈帧迁移转换后立即执行编译后的本地代码，即完成栈上替换。
address InterpreterGenerator::generate_normal_entry(bool synchronized) {
    slog_trace("进入hotspot/src/cpu/x86/vm/templateInterpreter_x86_64.cpp中的InterpreterGenerator::generate_normal_entry函数...");
  // determine code generation flags
    //UseCompiler表示使用JIT编译器，默认为true
    //CountCompiledCalls表示统计编译方法的执行次数
    //inc_counter表示是否增加方法调用计数
  bool inc_counter  = UseCompiler || CountCompiledCalls;

  // ebx: Method*
  // r13: sender sp
  //  entry_point函数的代码入口地址
    //获取BufferBlob写入地址
  address entry_point = __ pc();

    //执行此段指令前，在call_stub中已经把待执行方法的Method*放入rbx中
    //获取Method中constMethod属性的内存地址
    // 当前rbx中存储的是指向Method的指针，通过Method*找到ConstMethod*
  const Address constMethod(rbx, Method::const_offset());
    // 通过Method*找到AccessFlags
    //获取Method中access_flags属性的内存地址
  const Address access_flags(rbx, Method::access_flags_offset());
    // 通过ConstMethod*得到parameter的大小
    //rdx后面会被设置成constMethod的内存地址，这里是获取constMethod中的size_of_parameters属性的内存地址
  const Address size_of_parameters(rdx,
                                   ConstMethod::size_of_parameters_offset());
    // 通过ConstMethod*得到local变量的大小
  const Address size_of_locals(rdx, ConstMethod::size_of_locals_offset());


  // get parameter size (always needed)
    // 上面已经说明了获取各种方法元数据的计算方式，
    // 但并没有执行计算，下面会生成对应的汇编来执行计算
    // 计算ConstMethod*，保存在rdx里面
    //将constMethod的内存地址放入rdx中
    //Java入参数量
  __ movptr(rdx, constMethod);
    // 计算parameter大小，保存在rcx里面
    //将size_of_parameters地址处的方法参数个数读取到rcx中
  __ load_unsigned_short(rcx, size_of_parameters);

    //此时寄存器中的值
    // rbx: Method*
    // rcx: size of parameters
    // r13: sender_sp，即执行此段指令前的rsp地址，在rsp下面就是方法调用的具体入参

    //将size_of_locals地址处的此方法的本地变量个数读取到rdx中
  // rbx: Method*
  // rcx: size of parameters
  // r13: sender_sp (could differ from sp+wordSize if we were called via c2i )

    // rbx：保存基址；rcx：保存循环变量；rdx：保存目标地址；rax：保存返回地址（下面用到）
    // 此时的各个寄存器中的值如下：
    //   rbx: Method*
    //   rcx: size of parameters
    //   r13: sender_sp (could differ from sp+wordSize
    //        if we were called via c2i ) 即调用者的栈顶地址
    // 计算local变量的大小，保存到rdx
    // 局部变量表最大槽数
  __ load_unsigned_short(rdx, size_of_locals); // get size of locals in words
    // 由于局部变量表用来存储传入的参数和被调用方法的局部变量，
    // 所以rdx减去rcx后就是被调用方法的局部变量可使用的大小
    //将rdx中的本地变量个数减去方法参数个数
    // 局部变量数 = 局部变量表最大槽数 - Java入参数量
  __ subl(rdx, rcx); // rdx = no. of additional locals

  // YYY
//   __ incrementl(rdx);
//   __ andl(rdx, -2);

  // see if we've got enough room on the stack for locals plus overhead.
    // 确保有足够的内存空间开始一个新的栈帧
    // ==============
    // 注意：
    // JVM规范中讲到的栈帧，操作数栈，本地变量表在Hotspot中是没有对应的C++类的，只有负责保存符号引用及其解析结果的运行时常量池有对应的C++类，
    // 即ConstantPool和ConstantPoolCache类。JVM规范中讲到的栈帧，操作数栈，本地变量表在Hotspot中其实都是汇编指令中用到的由CPU直接管理维护的栈帧，
    // 操作数栈是当前Java方法对应的一个栈帧而已，本地变量表示是当前Java方法对应的栈帧的底部的一段连续的内存空间而已，
    // 本地变量表在开启一个新的Java栈帧式就会初始化完成并且在整个方法调用过程中所对应的的内存区域不变，其大小是固定的，根据方法编译后的本地变量大小确定。
    // 操作数栈的变化跟栈帧的演变是一样的，栈顶不断向低地址方向演变。
  generate_stack_overflow_check();

  // get return address
    //为了将方法入参和方法局部变量连在一起，先弹出返回地址
    // 返回地址是在CallStub中保存的，如果不弹出堆栈到rax，中间会有个return address使得局部变量表不是连续的，
    // 这会导致其中的局部变量计算方式不一致，所以暂时将返回地址存储到rax中
    // 将栈顶的值放入rax中，栈顶的值就是此时rsp中的地址，即Java方法执行完成后的地址
  __ pop(rax);

  // compute beginning of parameters (r14)
    // 计算第1个参数的地址：当前栈顶地址 + 变量大小 * 8 - 一个字大小
    // 注意，因为地址保存在低地址上，而堆栈是向低地址扩展的，所以只需加n-1个变量大小就可以得到第1个参数的地址
    //执行此段指令时因为还未移动rsp，rbp，所以rsp的地址不变依然是执行此段指令前的rsp地址
    //计算起始方法入参的地址，将其保存到r14中
  __ lea(r14, Address(rsp, rcx, Address::times_8, -wordSize));

  // rdx - # of additional locals
  // allocate space for locals
  // explicitly initialize locals
    // 把函数的局部变量设置为0,也就是做初始化，防止之前遗留下的值影响
    // rdx：被调用方法的局部变量可使用的大小
  {
    Label exit, loop;
      //test指令执行逻辑与操作，结果保存到标志寄存器中，如果rdx小于0，则逻辑与的结果也是小于0
    __ testl(rdx, rdx);
      // 如果rdx<=0，不做任何操作
      //如果rdx小于或者等于0，即本地变量个数小于方法参数个数，则不用做什么，跳转到exit
    __ jcc(Assembler::lessEqual, exit); // do nothing if rdx <= 0
      //如果rdx大于0
    __ bind(loop);
      // 初始化局部变量
      //将0放入当前栈帧中，即完成本地变量的初始化
    __ push((int) NULL_WORD); // initialize local variables
      //让rdx减1
    __ decrementl(rdx); // until everything initialized
      //判断rdx是否大于0，如果大于则跳转到loop开始执行，即不断push rdx个0到栈帧中，将额外的本地变量都初始化掉
    __ jcc(Assembler::greater, loop);
    __ bind(exit);
  }

  // initialize fixed part of activation frame
    //初始化一个新的栈帧，获取并保存方法的字节码，ConstantPoolCache等的地址
  // 函数的位置：hotspot/src/cpu/x86/vm/templateInterpreter_x86_64.cpp
    // 生成固定帧，调用完generate_fixed_frame()函数后一些寄存器中保存的值如下：
    //
    //rbx：Method*
    //ecx：invocation counter
    //r13：bcp(byte code pointer)
    //rdx：ConstantPool* 常量池的地址
    //r14：本地变量表第1个参数的地址
    slog_trace("即将调用generate_fixed_frame函数来生成固定帧...");
  generate_fixed_frame(false);
    // 执行完generate_fixed_frame()函数后会继续返回执行InterpreterGenerator::generate_normal_entry()函数，如果是为同步方法生成机器码，那么还需要调用lock_method()函数，这个函数会改变当前栈帧的状态，添加同步所需要的一些信息

  // make sure method is not native & not abstract
#ifdef ASSERT
  __ movl(rax, access_flags);
  {
    Label L;
    __ testl(rax, JVM_ACC_NATIVE);
    __ jcc(Assembler::zero, L);
    __ stop("tried to execute native method as non-native");
    __ bind(L);
  }
  {
    Label L;
    __ testl(rax, JVM_ACC_ABSTRACT);
    __ jcc(Assembler::zero, L);
    __ stop("tried to execute abstract method in interpreter");
    __ bind(L);
  }
#endif

  // Since at this point in the method invocation the exception
  // handler would try to exit the monitor of synchronized methods
  // which hasn't been entered yet, we set the thread local variable
  // _do_not_unlock_if_synchronized to true. The remove_activation
  // will check this flag.
  // 解释执行普通Java方法synchronized解锁 :
    //   普通Java方法执行完成后需要通过return系列指令返回到方法的调用栈帧，synchronized解锁就是在return系列指令中完成的。
    //   return系列指令一共有7个，其底层实现都是同一个方法，参考TemplateTable::initialize方法的定义
    // ireturn表示返回一个int值，lreturn表示返回一个long值，freturn表示返回一个float值， dreturn表示返回一个double值，areturn表示返回一个对象引用，return表示返回void，
    // 除上述6个外OpenJDK还增加了一个_return_register_finalizer，跟return一样都是返回void，不同的是如果目标类实现了finalize方法则会注册对应的Finalizer。

    //r15_thread保存了当前线程Thread*的引用
    //获取Thread的do_not_unlock_if_synchronized属性的地址
  const Address do_not_unlock_if_synchronized(r15_thread,
        in_bytes(JavaThread::do_not_unlock_if_synchronized_offset()));
    //将do_not_unlock_if_synchronized属性置为true
  __ movbool(do_not_unlock_if_synchronized, true);

    //执行profile统计
  __ profile_parameters_type(rax, rcx, rdx);
  // increment invocation count & check for overflow
  Label invocation_counter_overflow;
  Label profile_method;
  Label profile_method_continue;
  if (inc_counter) {
      //增加方法调用计数
    generate_counter_incr(&invocation_counter_overflow,
                          &profile_method,
                          &profile_method_continue);
    if (ProfileInterpreter) {
      __ bind(profile_method_continue);
    }
  }

  Label continue_after_compile;
  __ bind(continue_after_compile);

  // check for synchronized interpreted methods
  bang_stack_shadow_pages(false);

  // reset the _do_not_unlock_if_synchronized flag
    //将当前线程的do_not_unlock_if_synchronized属性置为false
  __ movbool(do_not_unlock_if_synchronized, false);

  // check for synchronized methods
  // Must happen AFTER invocation_counter check and stack overflow check,
  // so method is not locked if overflows.
    // 如果是同步方法时，还需要执行lock_method()函数，所以会影响到栈帧布局
    //如果需要上锁则分配monitor然后锁定此方法
  if (synchronized) {
    // Allocate monitor and lock method
    lock_method();
  } else {
    // no synchronization necessary
#ifdef ASSERT
    {
      Label L;
      __ movl(rax, access_flags);
      __ testl(rax, JVM_ACC_SYNCHRONIZED);
      __ jcc(Assembler::zero, L);
      __ stop("method needs synchronization");
      __ bind(L);
    }
#endif
  }

  // start execution
#ifdef ASSERT
  {
    Label L;
     const Address monitor_block_top (rbp,
                 frame::interpreter_frame_monitor_block_top_offset * wordSize);
    __ movptr(rax, monitor_block_top);
    __ cmpptr(rax, rsp);
    __ jcc(Assembler::equal, L);
    __ stop("broken stack frame setup in interpreter");
    __ bind(L);
  }
#endif

  // jvmti support
    // 发布JVMTI事件
  __ notify_method_entry();

    //开始字节码执行
    // 跳转到目标Java方法的第一条字节码指令，并执行其对应的机器指令
    // 调用dispatch_next()函数执行Java方法的字节码，其实就是根据字节码找到对应的机器指令片段的入口地址来执行，这段机器码就是根据对应的字节码语义翻译过来的
    slog_trace("即将调用dispatch_next函数来执行Java方法的第一条字节码...");
  __ dispatch_next(vtos);

  // invocation counter overflow
    //方法执行完成
  if (inc_counter) {
    if (ProfileInterpreter) {
      // We have decided to profile this method in the interpreter
      __ bind(profile_method);
      __ call_VM(noreg, CAST_FROM_FN_PTR(address, InterpreterRuntime::profile_method));
      __ set_method_data_pointer_for_bcp();
      __ get_method(rbx);
      __ jmp(profile_method_continue);
    }
    // Handle overflow of counter and compile method
    // InterpreterGenerator::generate_counter_incr中判断overflow后就会跳转到invocation_counter_overflow标签处，即执行generate_counter_overflow方法。
    __ bind(invocation_counter_overflow);
      //最终执行InterpreterRuntime::frequency_counter_overflow方法，这里会完成方法的编译
      //如果编译完成，则跳转到continue_after_compile
    generate_counter_overflow(&continue_after_compile);
  }

  return entry_point;
}

// Entry points
//
// Here we generate the various kind of entries into the interpreter.
// The two main entry type are generic bytecode methods and native
// call method.  These both come in synchronized and non-synchronized
// versions but the frame layout they create is very similar. The
// other method entry types are really just special purpose entries
// that are really entry and interpretation all in one. These are for
// trivial methods like accessor, empty, or special math methods.
//
// When control flow reaches any of the entry types for the interpreter
// the following holds ->
//
// Arguments:
//
// rbx: Method*
//
// Stack layout immediately at entry
//
// [ return address     ] <--- rsp
// [ parameter n        ]
//   ...
// [ parameter 1        ]
// [ expression stack   ] (caller's java expression stack)

// Assuming that we don't go to one of the trivial specialized entries
// the stack will look like below when we are ready to execute the
// first bytecode (or call the native routine). The register usage
// will be as the template based interpreter expects (see
// interpreter_amd64.hpp).
//
// local variables follow incoming parameters immediately; i.e.
// the return address is moved to the end of the locals).
//
// [ monitor entry      ] <--- rsp
//   ...
// [ monitor entry      ]
// [ expr. stack bottom ]
// [ saved r13          ]
// [ current r14        ]
// [ Method*            ]
// [ saved ebp          ] <--- rbp
// [ return address     ]
// [ local variable m   ]
//   ...
// [ local variable 1   ]
// [ parameter n        ]
//   ...
// [ parameter 1        ] <--- r14
// 调用generate_method_entry()函数为各种类型的方法生成对应的方法入口
// 重点关注生成执行普通Java方法和本地方法入口地址的generate_method_entry方法的实现
// 生成本地方法执行入口地址的是generate_native_entry方法，生成普通Java方法执行入口地址的是generate_normal_entry方法，这两个都要区分方法是否是同步synchronized的。
address AbstractInterpreterGenerator::generate_method_entry(
                                        AbstractInterpreter::MethodKind kind) {
    slog_trace("进入hotspot/src/cpu/x86/vm/templateInterpreter_x86_64.cpp中的AbstractInterpreterGenerator::generate_method_entry函数...");
  // determine code generation flags
  bool synchronized = false;
  address entry_point = NULL;
  InterpreterGenerator* ig_this = (InterpreterGenerator*)this;

    // 根据方法类型kind生成不同的入口
  switch (kind) {
        // 表示普通方法类型，zerolocals表示正常的Java方法调用，包括Java程序的main()方法，对于zerolocals来说，会调用ig_this->generate_normal_entry()函数生成调用stub（入口）
  case Interpreter::zerolocals             :                                                      break;
        // 表示普通的、同步方法类型，即被 synchronized关键字修饰的实例方法
  case Interpreter::zerolocals_synchronized: synchronized = true;                                 break;
        // 表示普通的本地方法，本地方法则通过generate_native_entry生成调用stub，这两个方法都有一个bool参数表示是否需要加锁，如果有锁，都会调用到lock_method()方法
  case Interpreter::native                 : entry_point = ig_this->generate_native_entry(false); break;
        // 表示被 synchronized关键字修饰的本地方法
  case Interpreter::native_synchronized    : entry_point = ig_this->generate_native_entry(true);  break;
  case Interpreter::empty                  : entry_point = ig_this->generate_empty_entry();       break;
  case Interpreter::accessor               : entry_point = ig_this->generate_accessor_entry();    break;
  case Interpreter::abstract               : entry_point = ig_this->generate_abstract_entry();    break;

        // java_lang_math相关方法的入口地址都是通过generate_math_entry方法生成
  case Interpreter::java_lang_math_sin     : // fall thru
  case Interpreter::java_lang_math_cos     : // fall thru
  case Interpreter::java_lang_math_tan     : // fall thru
  case Interpreter::java_lang_math_abs     : // fall thru
  case Interpreter::java_lang_math_log     : // fall thru
  case Interpreter::java_lang_math_log10   : // fall thru
  case Interpreter::java_lang_math_sqrt    : // fall thru
  case Interpreter::java_lang_math_pow     : // fall thru
  case Interpreter::java_lang_math_exp     : entry_point = ig_this->generate_math_entry(kind);      break;
  case Interpreter::java_lang_ref_reference_get
                                           : entry_point = ig_this->generate_Reference_get_entry(); break;
  case Interpreter::java_util_zip_CRC32_update
                                           : entry_point = ig_this->generate_CRC32_update_entry();  break;
  case Interpreter::java_util_zip_CRC32_updateBytes
                                           : // fall thru
  case Interpreter::java_util_zip_CRC32_updateByteBuffer
                                           : entry_point = ig_this->generate_CRC32_updateBytes_entry(kind); break;
  default:
    fatal(err_msg("unexpected method kind: %d", kind));
    break;
  }
    //如果生成的entry_point非空
  if (entry_point) {
    return entry_point;
  }
    //只有普通方法会走到此处，即普通方法通过generate_normal_entry生成调用入口地址
  // generate_normal_entry()函数会为执行的方法生成堆栈，而堆栈由局部变量表（用来存储传入的参数和被调用方法的局部变量）、Java方法栈帧数据和操作数栈这三大部分组成，
  // 所以entry_point例程（其实就是一段机器指令片段，英文名为stub）会创建这3部分来辅助Java方法的执行。
  return ig_this->generate_normal_entry(synchronized);
}

// These should never be compiled since the interpreter will prefer
// the compiled version to the intrinsic version.
bool AbstractInterpreter::can_be_compiled(methodHandle m) {
  switch (method_kind(m)) {
    case Interpreter::java_lang_math_sin     : // fall thru
    case Interpreter::java_lang_math_cos     : // fall thru
    case Interpreter::java_lang_math_tan     : // fall thru
    case Interpreter::java_lang_math_abs     : // fall thru
    case Interpreter::java_lang_math_log     : // fall thru
    case Interpreter::java_lang_math_log10   : // fall thru
    case Interpreter::java_lang_math_sqrt    : // fall thru
    case Interpreter::java_lang_math_pow     : // fall thru
    case Interpreter::java_lang_math_exp     :
      return false;
    default:
      return true;
  }
}

// How much stack a method activation needs in words.
int AbstractInterpreter::size_top_interpreter_activation(Method* method) {
  const int entry_size = frame::interpreter_frame_monitor_size();

  // total overhead size: entry_size + (saved rbp thru expr stack
  // bottom).  be sure to change this if you add/subtract anything
  // to/from the overhead area
  const int overhead_size =
    -(frame::interpreter_frame_initial_sp_offset) + entry_size;

  const int stub_code = frame::entry_frame_after_call_words;
  const int method_stack = (method->max_locals() + method->max_stack()) *
                           Interpreter::stackElementWords;
  return (overhead_size + method_stack + stub_code);
}

//-----------------------------------------------------------------------------
// Exceptions

void TemplateInterpreterGenerator::generate_throw_exception() {
  // Entry point in previous activation (i.e., if the caller was
  // interpreted)
  Interpreter::_rethrow_exception_entry = __ pc();
  // Restore sp to interpreter_frame_last_sp even though we are going
  // to empty the expression stack for the exception processing.
  __ movptr(Address(rbp, frame::interpreter_frame_last_sp_offset * wordSize), (int32_t)NULL_WORD);
  // rax: exception
  // rdx: return address/pc that threw exception
  __ restore_bcp();    // r13 points to call/send
  __ restore_locals();
  __ reinit_heapbase();  // restore r12 as heapbase.
  // Entry point for exceptions thrown within interpreter code
  Interpreter::_throw_exception_entry = __ pc();
  // expression stack is undefined here
  // rax: exception
  // r13: exception bcp
  __ verify_oop(rax);
  __ mov(c_rarg1, rax);

  // expression stack must be empty before entering the VM in case of
  // an exception
  __ empty_expression_stack();
  // find exception handler address and preserve exception oop
  __ call_VM(rdx,
             CAST_FROM_FN_PTR(address,
                          InterpreterRuntime::exception_handler_for_exception),
             c_rarg1);
  // rax: exception handler entry point
  // rdx: preserved exception oop
  // r13: bcp for exception handler
  __ push_ptr(rdx); // push exception which is now the only value on the stack
  __ jmp(rax); // jump to exception handler (may be _remove_activation_entry!)

  // If the exception is not handled in the current frame the frame is
  // removed and the exception is rethrown (i.e. exception
  // continuation is _rethrow_exception).
  //
  // Note: At this point the bci is still the bxi for the instruction
  // which caused the exception and the expression stack is
  // empty. Thus, for any VM calls at this point, GC will find a legal
  // oop map (with empty expression stack).

  // In current activation
  // tos: exception
  // esi: exception bcp

  //
  // JVMTI PopFrame support
  //

  Interpreter::_remove_activation_preserving_args_entry = __ pc();
  __ empty_expression_stack();
  // Set the popframe_processing bit in pending_popframe_condition
  // indicating that we are currently handling popframe, so that
  // call_VMs that may happen later do not trigger new popframe
  // handling cycles.
  __ movl(rdx, Address(r15_thread, JavaThread::popframe_condition_offset()));
  __ orl(rdx, JavaThread::popframe_processing_bit);
  __ movl(Address(r15_thread, JavaThread::popframe_condition_offset()), rdx);

  {
    // Check to see whether we are returning to a deoptimized frame.
    // (The PopFrame call ensures that the caller of the popped frame is
    // either interpreted or compiled and deoptimizes it if compiled.)
    // In this case, we can't call dispatch_next() after the frame is
    // popped, but instead must save the incoming arguments and restore
    // them after deoptimization has occurred.
    //
    // Note that we don't compare the return PC against the
    // deoptimization blob's unpack entry because of the presence of
    // adapter frames in C2.
    Label caller_not_deoptimized;
    __ movptr(c_rarg1, Address(rbp, frame::return_addr_offset * wordSize));
    __ super_call_VM_leaf(CAST_FROM_FN_PTR(address,
                               InterpreterRuntime::interpreter_contains), c_rarg1);
    __ testl(rax, rax);
    __ jcc(Assembler::notZero, caller_not_deoptimized);

    // Compute size of arguments for saving when returning to
    // deoptimized caller
    __ get_method(rax);
    __ movptr(rax, Address(rax, Method::const_offset()));
    __ load_unsigned_short(rax, Address(rax, in_bytes(ConstMethod::
                                                size_of_parameters_offset())));
    __ shll(rax, Interpreter::logStackElementSize);
    __ restore_locals(); // XXX do we need this?
    __ subptr(r14, rax);
    __ addptr(r14, wordSize);
    // Save these arguments
    __ super_call_VM_leaf(CAST_FROM_FN_PTR(address,
                                           Deoptimization::
                                           popframe_preserve_args),
                          r15_thread, rax, r14);

    __ remove_activation(vtos, rdx,
                         /* throw_monitor_exception */ false,
                         /* install_monitor_exception */ false,
                         /* notify_jvmdi */ false);

    // Inform deoptimization that it is responsible for restoring
    // these arguments
    __ movl(Address(r15_thread, JavaThread::popframe_condition_offset()),
            JavaThread::popframe_force_deopt_reexecution_bit);

    // Continue in deoptimization handler
    __ jmp(rdx);

    __ bind(caller_not_deoptimized);
  }

  __ remove_activation(vtos, rdx, /* rdx result (retaddr) is not used */
                       /* throw_monitor_exception */ false,
                       /* install_monitor_exception */ false,
                       /* notify_jvmdi */ false);

  // Finish with popframe handling
  // A previous I2C followed by a deoptimization might have moved the
  // outgoing arguments further up the stack. PopFrame expects the
  // mutations to those outgoing arguments to be preserved and other
  // constraints basically require this frame to look exactly as
  // though it had previously invoked an interpreted activation with
  // no space between the top of the expression stack (current
  // last_sp) and the top of stack. Rather than force deopt to
  // maintain this kind of invariant all the time we call a small
  // fixup routine to move the mutated arguments onto the top of our
  // expression stack if necessary.
  __ mov(c_rarg1, rsp);
  __ movptr(c_rarg2, Address(rbp, frame::interpreter_frame_last_sp_offset * wordSize));
  // PC must point into interpreter here
  __ set_last_Java_frame(noreg, rbp, __ pc());
  __ super_call_VM_leaf(CAST_FROM_FN_PTR(address, InterpreterRuntime::popframe_move_outgoing_args), r15_thread, c_rarg1, c_rarg2);
  __ reset_last_Java_frame(r15_thread, true);
  // Restore the last_sp and null it out
  __ movptr(rsp, Address(rbp, frame::interpreter_frame_last_sp_offset * wordSize));
  __ movptr(Address(rbp, frame::interpreter_frame_last_sp_offset * wordSize), (int32_t)NULL_WORD);

  __ restore_bcp();  // XXX do we need this?
  __ restore_locals(); // XXX do we need this?
  // The method data pointer was incremented already during
  // call profiling. We have to restore the mdp for the current bcp.
  if (ProfileInterpreter) {
    __ set_method_data_pointer_for_bcp();
  }

  // Clear the popframe condition flag
  __ movl(Address(r15_thread, JavaThread::popframe_condition_offset()),
          JavaThread::popframe_inactive);

#if INCLUDE_JVMTI
  if (EnableInvokeDynamic) {
    Label L_done;
    const Register local0 = r14;

    __ cmpb(Address(r13, 0), Bytecodes::_invokestatic);
    __ jcc(Assembler::notEqual, L_done);

    // The member name argument must be restored if _invokestatic is re-executed after a PopFrame call.
    // Detect such a case in the InterpreterRuntime function and return the member name argument, or NULL.

    __ get_method(rdx);
    __ movptr(rax, Address(local0, 0));
    __ call_VM(rax, CAST_FROM_FN_PTR(address, InterpreterRuntime::member_name_arg_or_null), rax, rdx, r13);

    __ testptr(rax, rax);
    __ jcc(Assembler::zero, L_done);

    __ movptr(Address(rbx, 0), rax);
    __ bind(L_done);
  }
#endif // INCLUDE_JVMTI

  __ dispatch_next(vtos);
  // end of PopFrame support

  Interpreter::_remove_activation_entry = __ pc();

  // preserve exception over this code sequence
  __ pop_ptr(rax);
  __ movptr(Address(r15_thread, JavaThread::vm_result_offset()), rax);
  // remove the activation (without doing throws on illegalMonitorExceptions)
  __ remove_activation(vtos, rdx, false, true, false);
  // restore exception
  __ get_vm_result(rax, r15_thread);

  // In between activations - previous activation type unknown yet
  // compute continuation point - the continuation point expects the
  // following registers set up:
  //
  // rax: exception
  // rdx: return address/pc that threw exception
  // rsp: expression stack of caller
  // rbp: ebp of caller
  __ push(rax);                                  // save exception
  __ push(rdx);                                  // save return address
  __ super_call_VM_leaf(CAST_FROM_FN_PTR(address,
                          SharedRuntime::exception_handler_for_return_address),
                        r15_thread, rdx);
  __ mov(rbx, rax);                              // save exception handler
  __ pop(rdx);                                   // restore return address
  __ pop(rax);                                   // restore exception
  // Note that an "issuing PC" is actually the next PC after the call
  __ jmp(rbx);                                   // jump to exception
                                                 // handler of caller
}


//
// JVMTI ForceEarlyReturn support
//
address TemplateInterpreterGenerator::generate_earlyret_entry_for(TosState state) {
  address entry = __ pc();

  __ restore_bcp();
  __ restore_locals();
  __ empty_expression_stack();
  __ load_earlyret_value(state);

  __ movptr(rdx, Address(r15_thread, JavaThread::jvmti_thread_state_offset()));
  Address cond_addr(rdx, JvmtiThreadState::earlyret_state_offset());

  // Clear the earlyret state
  __ movl(cond_addr, JvmtiThreadState::earlyret_inactive);

  __ remove_activation(state, rsi,
                       false, /* throw_monitor_exception */
                       false, /* install_monitor_exception */
                       true); /* notify_jvmdi */
  __ jmp(rsi);

  return entry;
} // end of ForceEarlyReturn support


//-----------------------------------------------------------------------------
// Helper for vtos entry point generation

void TemplateInterpreterGenerator::set_vtos_entry_points(Template* t,
                                                         address& bep,
                                                         address& cep,
                                                         address& sep,
                                                         address& aep,
                                                         address& iep,
                                                         address& lep,
                                                         address& fep,
                                                         address& dep,
                                                         address& vep) {
  assert(t->is_valid() && t->tos_in() == vtos, "illegal template");
  Label L;
    //如果此时栈顶缓存的值类型不是vtos，则将对应的值从栈顶缓存即rax中push到栈帧中
  aep = __ pc();  __ push_ptr();  __ jmp(L);
  fep = __ pc();  __ push_f();    __ jmp(L);
  dep = __ pc();  __ push_d();    __ jmp(L);
  lep = __ pc();  __ push_l();    __ jmp(L);
  bep = cep = sep =
  iep = __ pc();  __ push_i();
  vep = __ pc();
  __ bind(L);
  generate_and_dispatch(t);
}


//-----------------------------------------------------------------------------
// Generation of individual instructions

// helpers for generate_and_dispatch


InterpreterGenerator::InterpreterGenerator(StubQueue* code)
  : TemplateInterpreterGenerator(code) {
    slog_trace("进入hotspot/src/cpu/x86/vm/templateInterpreter_x86_64.cpp中的InterpreterGenerator::InterpreterGenerator构造函数...");
    slog_trace("即将调用generate_all函数...");
    // 其核心就是调用父类的generate_all方法。
   generate_all(); // down here so it can be "virtual"
}

//-----------------------------------------------------------------------------

// Non-product code
#ifndef PRODUCT
address TemplateInterpreterGenerator::generate_trace_code(TosState state) {
  address entry = __ pc();

  __ push(state);
  __ push(c_rarg0);
  __ push(c_rarg1);
  __ push(c_rarg2);
  __ push(c_rarg3);
  __ mov(c_rarg2, rax);  // Pass itos
#ifdef _WIN64
  __ movflt(xmm3, xmm0); // Pass ftos
#endif
  __ call_VM(noreg,
             CAST_FROM_FN_PTR(address, SharedRuntime::trace_bytecode),
             c_rarg1, c_rarg2, c_rarg3);
  __ pop(c_rarg3);
  __ pop(c_rarg2);
  __ pop(c_rarg1);
  __ pop(c_rarg0);
  __ pop(state);
  __ ret(0);                                   // return from result handler

  return entry;
}

void TemplateInterpreterGenerator::count_bytecode() {
  __ incrementl(ExternalAddress((address) &BytecodeCounter::_counter_value));
}

void TemplateInterpreterGenerator::histogram_bytecode(Template* t) {
  __ incrementl(ExternalAddress((address) &BytecodeHistogram::_counters[t->bytecode()]));
}

void TemplateInterpreterGenerator::histogram_bytecode_pair(Template* t) {
  __ mov32(rbx, ExternalAddress((address) &BytecodePairHistogram::_index));
  __ shrl(rbx, BytecodePairHistogram::log2_number_of_codes);
  __ orl(rbx,
         ((int) t->bytecode()) <<
         BytecodePairHistogram::log2_number_of_codes);
  __ mov32(ExternalAddress((address) &BytecodePairHistogram::_index), rbx);
  __ lea(rscratch1, ExternalAddress((address) BytecodePairHistogram::_counters));
  __ incrementl(Address(rscratch1, rbx, Address::times_4));
}


void TemplateInterpreterGenerator::trace_bytecode(Template* t) {
  // Call a little run-time stub to avoid blow-up for each bytecode.
  // The run-time runtime saves the right registers, depending on
  // the tosca in-state for the given template.

  assert(Interpreter::trace_code(t->tos_in()) != NULL,
         "entry must have been generated");
  __ mov(r12, rsp); // remember sp (can only use r12 if not using call_VM)
  __ andptr(rsp, -16); // align stack as required by ABI
  __ call(RuntimeAddress(Interpreter::trace_code(t->tos_in())));
  __ mov(rsp, r12); // restore sp
  __ reinit_heapbase();
}


void TemplateInterpreterGenerator::stop_interpreter_at() {
  Label L;
  __ cmp32(ExternalAddress((address) &BytecodeCounter::_counter_value),
           StopInterpreterAt);
  __ jcc(Assembler::notEqual, L);
  __ int3();
  __ bind(L);
}
#endif // !PRODUCT
#endif // ! CC_INTERP
