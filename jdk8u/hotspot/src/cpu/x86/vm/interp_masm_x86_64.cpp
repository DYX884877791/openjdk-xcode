/*
 * Copyright (c) 2003, 2018, Oracle and/or its affiliates. All rights reserved.
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
#include "interp_masm_x86.hpp"
#include "interpreter/interpreter.hpp"
#include "interpreter/interpreterRuntime.hpp"
#include "oops/arrayOop.hpp"
#include "oops/markOop.hpp"
#include "oops/methodData.hpp"
#include "oops/method.hpp"
#include "prims/jvmtiExport.hpp"
#include "prims/jvmtiRedefineClassesTrace.hpp"
#include "prims/jvmtiThreadState.hpp"
#include "runtime/basicLock.hpp"
#include "runtime/biasedLocking.hpp"
#include "runtime/sharedRuntime.hpp"
#include "runtime/thread.inline.hpp"
#include "utilities/slog.hpp"


// Implementation of InterpreterMacroAssembler

#ifdef CC_INTERP
void InterpreterMacroAssembler::get_method(Register reg) {
  movptr(reg, Address(rbp, -((int)sizeof(BytecodeInterpreter) + 2 * wordSize)));
  movptr(reg, Address(reg, byte_offset_of(BytecodeInterpreter, _method)));
}
#endif // CC_INTERP

#ifndef CC_INTERP

void InterpreterMacroAssembler::call_VM_leaf_base(address entry_point,
                                                  int number_of_arguments) {
  // interpreter specific
  //
  // Note: No need to save/restore bcp & locals (r13 & r14) pointer
  //       since these are callee saved registers and no blocking/
  //       GC can happen in leaf calls.
  // Further Note: DO NOT save/restore bcp/locals. If a caller has
  // already saved them so that it can use esi/edi as temporaries
  // then a save/restore here will DESTROY the copy the caller
  // saved! There used to be a save_bcp() that only happened in
  // the ASSERT path (no restore_bcp). Which caused bizarre failures
  // when jvm built with ASSERTs.
#ifdef ASSERT
  {
    Label L;
    cmpptr(Address(rbp, frame::interpreter_frame_last_sp_offset * wordSize), (int32_t)NULL_WORD);
    jcc(Assembler::equal, L);
    stop("InterpreterMacroAssembler::call_VM_leaf_base:"
         " last_sp != NULL");
    bind(L);
  }
#endif
  // super call
  MacroAssembler::call_VM_leaf_base(entry_point, number_of_arguments);
  // interpreter specific
  // Used to ASSERT that r13/r14 were equal to frame's bcp/locals
  // but since they may not have been saved (and we don't want to
  // save thme here (see note above) the assert is invalid.
}

void InterpreterMacroAssembler::call_VM_base(Register oop_result,
                                             Register java_thread,
                                             Register last_java_sp,
                                             address  entry_point,
                                             int      number_of_arguments,
                                             bool     check_exceptions) {
  // interpreter specific
  //
  // Note: Could avoid restoring locals ptr (callee saved) - however doesn't
  //       really make a difference for these runtime calls, since they are
  //       slow anyway. Btw., bcp must be saved/restored since it may change
  //       due to GC.
  // assert(java_thread == noreg , "not expecting a precomputed java thread");
  save_bcp();
#ifdef ASSERT
  {
    Label L;
    cmpptr(Address(rbp, frame::interpreter_frame_last_sp_offset * wordSize), (int32_t)NULL_WORD);
    jcc(Assembler::equal, L);
    stop("InterpreterMacroAssembler::call_VM_leaf_base:"
         " last_sp != NULL");
    bind(L);
  }
#endif /* ASSERT */
  // super call
  MacroAssembler::call_VM_base(oop_result, noreg, last_java_sp,
                               entry_point, number_of_arguments,
                               check_exceptions);
  // interpreter specific
  restore_bcp();
  restore_locals();
}


void InterpreterMacroAssembler::check_and_handle_popframe(Register java_thread) {
  if (JvmtiExport::can_pop_frame()) {
    Label L;
    // Initiate popframe handling only if it is not already being
    // processed.  If the flag has the popframe_processing bit set, it
    // means that this code is called *during* popframe handling - we
    // don't want to reenter.
    // This method is only called just after the call into the vm in
    // call_VM_base, so the arg registers are available.
    movl(c_rarg0, Address(r15_thread, JavaThread::popframe_condition_offset()));
    testl(c_rarg0, JavaThread::popframe_pending_bit);
    jcc(Assembler::zero, L);
    testl(c_rarg0, JavaThread::popframe_processing_bit);
    jcc(Assembler::notZero, L);
    // Call Interpreter::remove_activation_preserving_args_entry() to get the
    // address of the same-named entrypoint in the generated interpreter code.
    call_VM_leaf(CAST_FROM_FN_PTR(address, Interpreter::remove_activation_preserving_args_entry));
    jmp(rax);
    bind(L);
  }
}


void InterpreterMacroAssembler::load_earlyret_value(TosState state) {
  movptr(rcx, Address(r15_thread, JavaThread::jvmti_thread_state_offset()));
  const Address tos_addr(rcx, JvmtiThreadState::earlyret_tos_offset());
  const Address oop_addr(rcx, JvmtiThreadState::earlyret_oop_offset());
  const Address val_addr(rcx, JvmtiThreadState::earlyret_value_offset());
  switch (state) {
    case atos: movptr(rax, oop_addr);
               movptr(oop_addr, (int32_t)NULL_WORD);
               verify_oop(rax, state);              break;
    case ltos: movptr(rax, val_addr);                 break;
    case btos:                                   // fall through
    case ztos:                                   // fall through
    case ctos:                                   // fall through
    case stos:                                   // fall through
    case itos: movl(rax, val_addr);                 break;
    case ftos: movflt(xmm0, val_addr);              break;
    case dtos: movdbl(xmm0, val_addr);              break;
    case vtos: /* nothing to do */                  break;
    default  : ShouldNotReachHere();
  }
  // Clean up tos value in the thread object
  movl(tos_addr,  (int) ilgl);
  movl(val_addr,  (int32_t) NULL_WORD);
}


void InterpreterMacroAssembler::check_and_handle_earlyret(Register java_thread) {
  if (JvmtiExport::can_force_early_return()) {
    Label L;
    movptr(c_rarg0, Address(r15_thread, JavaThread::jvmti_thread_state_offset()));
    testptr(c_rarg0, c_rarg0);
    jcc(Assembler::zero, L); // if (thread->jvmti_thread_state() == NULL) exit;

    // Initiate earlyret handling only if it is not already being processed.
    // If the flag has the earlyret_processing bit set, it means that this code
    // is called *during* earlyret handling - we don't want to reenter.
    movl(c_rarg0, Address(c_rarg0, JvmtiThreadState::earlyret_state_offset()));
    cmpl(c_rarg0, JvmtiThreadState::earlyret_pending);
    jcc(Assembler::notEqual, L);

    // Call Interpreter::remove_activation_early_entry() to get the address of the
    // same-named entrypoint in the generated interpreter code.
    movptr(c_rarg0, Address(r15_thread, JavaThread::jvmti_thread_state_offset()));
    movl(c_rarg0, Address(c_rarg0, JvmtiThreadState::earlyret_tos_offset()));
    call_VM_leaf(CAST_FROM_FN_PTR(address, Interpreter::remove_activation_early_entry), c_rarg0);
    jmp(rax);
    bind(L);
  }
}


void InterpreterMacroAssembler::get_unsigned_2_byte_index_at_bcp(
  Register reg,
  int bcp_offset) {
  assert(bcp_offset >= 0, "bcp is still pointing to start of bytecode");
  load_unsigned_short(reg, Address(r13, bcp_offset));
  bswapl(reg);
  shrl(reg, 16);
}


void InterpreterMacroAssembler::get_cache_index_at_bcp(Register index,
                                                       int bcp_offset,
                                                       size_t index_size) {
  assert(bcp_offset > 0, "bcp is still pointing to start of bytecode");
  if (index_size == sizeof(u2)) {
    load_unsigned_short(index, Address(r13, bcp_offset));
  } else if (index_size == sizeof(u4)) {
    assert(EnableInvokeDynamic, "giant index used only for JSR 292");
    movl(index, Address(r13, bcp_offset));
    // Check if the secondary index definition is still ~x, otherwise
    // we have to change the following assembler code to calculate the
    // plain index.
    assert(ConstantPool::decode_invokedynamic_index(~123) == 123, "else change next line");
    notl(index);  // convert to plain index
  } else if (index_size == sizeof(u1)) {
    load_unsigned_byte(index, Address(r13, bcp_offset));
  } else {
    ShouldNotReachHere();
  }
}


void InterpreterMacroAssembler::get_cache_and_index_at_bcp(Register cache,
                                                           Register index,
                                                           int bcp_offset,
                                                           size_t index_size) {
  assert_different_registers(cache, index);
  get_cache_index_at_bcp(index, bcp_offset, index_size);
  movptr(cache, Address(rbp, frame::interpreter_frame_cache_offset * wordSize));
  assert(sizeof(ConstantPoolCacheEntry) == 4 * wordSize, "adjust code below");
  // convert from field index to ConstantPoolCacheEntry index
  assert(exact_log2(in_words(ConstantPoolCacheEntry::size())) == 2, "else change next line");
  shll(index, 2);
}


void InterpreterMacroAssembler::get_cache_and_index_and_bytecode_at_bcp(Register cache,
                                                                        Register index,
                                                                        Register bytecode,
                                                                        int byte_no,
                                                                        int bcp_offset,
                                                                        size_t index_size) {
  get_cache_and_index_at_bcp(cache, index, bcp_offset, index_size);
  // We use a 32-bit load here since the layout of 64-bit words on
  // little-endian machines allow us that.
  movl(bytecode, Address(cache, index, Address::times_ptr, ConstantPoolCache::base_offset() + ConstantPoolCacheEntry::indices_offset()));
  const int shift_count = (1 + byte_no) * BitsPerByte;
  assert((byte_no == TemplateTable::f1_byte && shift_count == ConstantPoolCacheEntry::bytecode_1_shift) ||
         (byte_no == TemplateTable::f2_byte && shift_count == ConstantPoolCacheEntry::bytecode_2_shift),
         "correct shift count");
  shrl(bytecode, shift_count);
  assert(ConstantPoolCacheEntry::bytecode_1_mask == ConstantPoolCacheEntry::bytecode_2_mask, "common mask");
  andl(bytecode, ConstantPoolCacheEntry::bytecode_1_mask);
}


void InterpreterMacroAssembler::get_cache_entry_pointer_at_bcp(Register cache,
                                                               Register tmp,
                                                               int bcp_offset,
                                                               size_t index_size) {
  assert(cache != tmp, "must use different register");
  get_cache_index_at_bcp(tmp, bcp_offset, index_size);
  assert(sizeof(ConstantPoolCacheEntry) == 4 * wordSize, "adjust code below");
  // convert from field index to ConstantPoolCacheEntry index
  // and from word offset to byte offset
  assert(exact_log2(in_bytes(ConstantPoolCacheEntry::size_in_bytes())) == 2 + LogBytesPerWord, "else change next line");
  shll(tmp, 2 + LogBytesPerWord);
  movptr(cache, Address(rbp, frame::interpreter_frame_cache_offset * wordSize));
  // skip past the header
  addptr(cache, in_bytes(ConstantPoolCache::base_offset()));
  addptr(cache, tmp);  // construct pointer to cache entry
}

void InterpreterMacroAssembler::get_method_counters(Register method,
                                                    Register mcs, Label& skip) {
  Label has_counters;
    // 获取当前Method的_method_counters属性
  movptr(mcs, Address(method, Method::method_counters_offset()));
    // 校验_method_counters属性是否非空，如果不为空则跳转到has_counters
  testptr(mcs, mcs);
  jcc(Assembler::notZero, has_counters);
    // 如果为空，则调用build_method_counters方法创建一个新的MethodCounters
  call_VM(noreg, CAST_FROM_FN_PTR(address,
          InterpreterRuntime::build_method_counters), method);
    // 将新的MethodCounters的地址放入mcs中，校验其是否为空，如果为空则跳转到skip
  movptr(mcs, Address(method,Method::method_counters_offset()));
  testptr(mcs, mcs);
  jcc(Assembler::zero, skip); // No MethodCounters allocated, OutOfMemory
  bind(has_counters);
}

// Load object from cpool->resolved_references(index)
void InterpreterMacroAssembler::load_resolved_reference_at_index(
                                           Register result, Register index) {
  assert_different_registers(result, index);
  // convert from field index to resolved_references() index and from
  // word index to byte offset. Since this is a java object, it can be compressed
  Register tmp = index;  // reuse
  shll(tmp, LogBytesPerHeapOop);

  get_constant_pool(result);
  // load pointer for resolved_references[] objArray
  movptr(result, Address(result, ConstantPool::resolved_references_offset_in_bytes()));
  // JNIHandles::resolve(obj);
  movptr(result, Address(result, 0));
  // Add in the index
  addptr(result, tmp);
  load_heap_oop(result, Address(result, arrayOopDesc::base_offset_in_bytes(T_OBJECT)));
}

// Generate a subtype check: branch to ok_is_subtype if sub_klass is a
// subtype of super_klass.
//
// Args:
//      rax: superklass
//      Rsub_klass: subklass
//
// Kills:
//      rcx, rdi
void InterpreterMacroAssembler::gen_subtype_check(Register Rsub_klass,
                                                  Label& ok_is_subtype) {
  assert(Rsub_klass != rax, "rax holds superklass");
  assert(Rsub_klass != r14, "r14 holds locals");
  assert(Rsub_klass != r13, "r13 holds bcp");
  assert(Rsub_klass != rcx, "rcx holds 2ndary super array length");
  assert(Rsub_klass != rdi, "rdi holds 2ndary super array scan ptr");

  // Profile the not-null value's klass.
  profile_typecheck(rcx, Rsub_klass, rdi); // blows rcx, reloads rdi

  // Do the check.
  check_klass_subtype(Rsub_klass, rax, rcx, ok_is_subtype); // blows rcx

  // Profile the failure of the check.
  profile_typecheck_failed(rcx); // blows rcx
}



// Java Expression Stack

void InterpreterMacroAssembler::pop_ptr(Register r) {
  pop(r);
}

void InterpreterMacroAssembler::pop_i(Register r) {
  // XXX can't use pop currently, upper half non clean
  movl(r, Address(rsp, 0));
  addptr(rsp, wordSize);
}

void InterpreterMacroAssembler::pop_l(Register r) {
  movq(r, Address(rsp, 0));
  addptr(rsp, 2 * Interpreter::stackElementSize);
}

void InterpreterMacroAssembler::pop_f(XMMRegister r) {
  movflt(r, Address(rsp, 0));
  addptr(rsp, wordSize);
}

void InterpreterMacroAssembler::pop_d(XMMRegister r) {
  movdbl(r, Address(rsp, 0));
  addptr(rsp, 2 * Interpreter::stackElementSize);
}

void InterpreterMacroAssembler::push_ptr(Register r) {
  push(r);
}

void InterpreterMacroAssembler::push_i(Register r) {
  push(r);
}

void InterpreterMacroAssembler::push_l(Register r) {
  subptr(rsp, 2 * wordSize);
  movq(Address(rsp, 0), r);
}

void InterpreterMacroAssembler::push_f(XMMRegister r) {
  subptr(rsp, wordSize);
  movflt(Address(rsp, 0), r);
}

void InterpreterMacroAssembler::push_d(XMMRegister r) {
  subptr(rsp, 2 * wordSize);
  movdbl(Address(rsp, 0), r);
}

void InterpreterMacroAssembler::pop(TosState state) {
    // 根据不同的栈顶值类型从栈帧中pop一个对应类型的变量放到rax中
  switch (state) {
  case atos: pop_ptr();                 break;
  case btos:
  case ztos:
  case ctos:
  case stos:
  case itos: pop_i();                   break;
  case ltos: pop_l();                   break;
  case ftos: pop_f();                   break;
  case dtos: pop_d();                   break;
  case vtos: /* nothing to do */        break;
  default:   ShouldNotReachHere();
  }
  verify_oop(rax, state);
}

void InterpreterMacroAssembler::push(TosState state) {
  verify_oop(rax, state);
  switch (state) {
  case atos: push_ptr();                break;
  case btos:
  case ztos:
  case ctos:
  case stos:
  case itos: push_i();                  break;
  case ltos: push_l();                  break;
  case ftos: push_f();                  break;
  case dtos: push_d();                  break;
  case vtos: /* nothing to do */        break;
  default  : ShouldNotReachHere();
  }
}


// Helpers for swap and dup
void InterpreterMacroAssembler::load_ptr(int n, Register val) {
  movptr(val, Address(rsp, Interpreter::expr_offset_in_bytes(n)));
}

void InterpreterMacroAssembler::store_ptr(int n, Register val) {
  movptr(Address(rsp, Interpreter::expr_offset_in_bytes(n)), val);
}


void InterpreterMacroAssembler::prepare_to_jump_from_interpreted() {
  // set sender sp
  lea(r13, Address(rsp, wordSize));
  // record last_sp
  movptr(Address(rbp, frame::interpreter_frame_last_sp_offset * wordSize), r13);
}


// Jump to from_interpreted entry of a call unless single stepping is possible
// in this thread in which case we must call the i2i entry
void InterpreterMacroAssembler::jump_from_interpreted(Register method, Register temp) {
  prepare_to_jump_from_interpreted();

  if (JvmtiExport::can_post_interpreter_events()) {
    Label run_compiled_code;
    // JVMTI events, such as single-stepping, are implemented partly by avoiding running
    // compiled code in threads for which the event is enabled.  Check here for
    // interp_only_mode if these events CAN be enabled.
    // interp_only is an int, on little endian it is sufficient to test the byte only
    // Is a cmpl faster?
    cmpb(Address(r15_thread, JavaThread::interp_only_mode_offset()), 0);
    jccb(Assembler::zero, run_compiled_code);
    jmp(Address(method, Method::interpreter_entry_offset()));
    bind(run_compiled_code);
  }

  jmp(Address(method, Method::from_interpreted_offset()));

}


// The following two routines provide a hook so that an implementation
// can schedule the dispatch in two parts.  amd64 does not do this.
void InterpreterMacroAssembler::dispatch_prolog(TosState state, int step) {
  // Nothing amd64 specific to be done here
}

void InterpreterMacroAssembler::dispatch_epilog(TosState state, int step) {
  dispatch_next(state, step);
}

/**
 * 比如取一个字节大小的指令（如iconst_0、aload_0等都是一个字节大小的指令），那么InterpreterMacroAssembler::dispatch_next()函数生成的汇编代码如下 ：
 * // 在generate_fixed_frame()函数中
 * // 已经让%r13存储了bcp
 * // %ebx中存储的是字节码的Opcode，也就是操作码
 * movzbl 0x0(%r13),%ebx
 *
 * // $0x7ffff73ba4a0这个地址指向的
 * // 是对应state状态下的一维数组，长度为256
 * movabs $0x7ffff73ba4a0,%r10
 *
 * // 注意%r10中存储的是常量，根据计算公式
 * // %r10+%rbx*8来获取指向存储入口地址的地址，
 * // 通过*(%r10+%rbx*8)获取到入口地址，
 * // 然后跳转到入口地址执行
 * jmpq *(%r10,%rbx,8)
 *
 * %r10指向的是对应栈顶缓存状态state下的一维数组，长度为256，其中存储的值为opcode
 *
 */
void InterpreterMacroAssembler::dispatch_base(TosState state, // 表示栈顶缓存状态
                                              address* table,
                                              bool verifyoop) {
  slog_trace("进入hotspot/src/cpu/x86/vm/interp_masm_x86_64.cpp中的InterpreterMacroAssembler::dispatch_base函数...");
    // 64位下是空实现
  verify_FPU(1, state);
    // 校验栈帧size
  if (VerifyActivationFrameSize) {
    Label L;
    mov(rcx, rbp);
    subptr(rcx, rsp);
    int32_t min_frame_size =
      (frame::link_offset - frame::interpreter_frame_initial_sp_offset) *
      wordSize;
    cmpptr(rcx, (int32_t)min_frame_size);
    jcc(Assembler::greaterEqual, L);
    stop("broken stack frame");
    bind(L);
  }
    // 如果栈顶缓存是对象，则校验对象
  if (verifyoop) {
    verify_oop(rax, state);
  }
    // 获取当前栈顶状态字节码转发表的地址，保存到rscratch1
    // 将table的地址拷贝到rscratch1寄存器中，这时的table地址实际上是DispatchTable的二维数组_table属性的第一维数组的地址
  lea(rscratch1, ExternalAddress((address)table));
  Address addr = Address(rscratch1, rbx, Address::times_8);
    // 跳转到字节码对应的入口执行机器码指令
    // address = rscratch1 + rbx * 8
    // 因为DispatchTable中索引直接为字节码指令，从0开始，而rbx现在存的就是下一条指令，所以可以通过（DispatchTable首地址 + rbx * 每个地址所占字节）来索引。然后直接利用jmp指令跳往字节码对应机器指令的地址。
    // 此时rbx是待执行的字节码，即跳转到对应字节码的汇编指令上，DispatchTable的二维数组_table属性的第二维就是字节码，即在rscratch1地址上偏移rbx*8个字节就可以找到对应的字节码指令实现了
  jmp(addr);
}

void InterpreterMacroAssembler::dispatch_only(TosState state) {
  dispatch_base(state, Interpreter::dispatch_table(state));
}

void InterpreterMacroAssembler::dispatch_only_normal(TosState state) {
  dispatch_base(state, Interpreter::normal_table(state));
}

void InterpreterMacroAssembler::dispatch_only_noverify(TosState state) {
  dispatch_base(state, Interpreter::normal_table(state), false);
}

// 执行字节码流的核心方法
// 调用dispatch_next()函数执行Java方法的字节码，其实就是根据字节码找到对应的机器指令片段的入口地址来执行，这段机器码就是根据对应的字节码语义翻译过来的，

// 从generate_fixed_frame()函数生成Java方法调用栈帧的时候，
// 如果当前是第一次调用，那么r13指向的是字节码的首地址，
// 即第一个字节码，此时的step参数为0

// 比如取一个字节大小的指令（如iconst_0、aload_0等都是一个字节大小的指令），那么InterpreterMacroAssembler::dispatch_next()函数生成的汇编代码如下 ：
// 在generate_fixed_frame()函数中
// 已经让%r13存储了bcp
// %ebx中存储的是字节码的Opcode，也就是操作码
// movzbl 0x0(%r13),%ebx

// $0x7ffff73ba4a0这个地址指向的
// 是对应state状态下的一维数组，长度为256
// movabs $0x7ffff73ba4a0,%r10

// 注意%r10中存储的是常量，根据计算公式
// %r10+%rbx*8来获取指向存储入口地址的地址，
// 通过*(%r10+%rbx*8)获取到入口地址，
// 然后跳转到入口地址执行
// jmpq *(%r10,%rbx,8)

// InterpreterMacroAssembler::dispatch_next开始执行第一个字节码的汇编指令，那么是如何跳转到下一个字节码指令了？
// 答案在set_entry_points_for_all_bytes()方法调用的TemplateInterpreterGenerator::set_short_entry_points方法实现里，
// 该方法用来实际生成_normal_table中各个字节码的调用入口地址
void InterpreterMacroAssembler::dispatch_next(TosState state, int step) {
    slog_trace("进入hotspot/src/cpu/x86/vm/interp_masm_x86_64.cpp中的InterpreterMacroAssembler::dispatch_next函数...");
  // load next bytecode (load before advancing r13 to prevent AGI)
    // 会根据当前指令地址偏移，获取下条指令地址，并通过地址获得指令,放入rbx寄存器。
    // 加载下一条字节码
    // r13保存的就是方法的字节码的内存位置，step表示跳过的字节数，第一次调用时没有传step，step默认为0
  load_unsigned_byte(rbx, Address(r13, step));
  // advance r13
    // r13指向字节码的首地址，当第1次调用时，参数step的值为0，那么load_unsigned_byte()函数从r13指向的内存中取一个字节的值，取出来的是字节码指令的操作码。
    // 增加r13的步长，这样下次执行时就会取出来下一个字节码指令的操作码。

    // 在当前字节码的位置，指针向前移动step宽度，
    // 获取地址上的值，这个值是Opcode（范围1~202），存储到rbx
    // step的值由字节码指令和它的操作数共同决定
    // 自增r13供下一次字节码分派使用
    // 将r13中的地址自增step
  increment(r13, step);
    // 根据栈顶缓存状态查询派发表，跳转执行
    // 返回当前栈顶状态的所有字节码入口点
  dispatch_base(state, Interpreter::dispatch_table(state));
}

void InterpreterMacroAssembler::dispatch_via(TosState state, address* table) {
  // load current bytecode
  load_unsigned_byte(rbx, Address(r13, 0));
  dispatch_base(state, table);
}

// remove activation
//
// Unlock the receiver if this is a synchronized method.
// Unlock any Java monitors from syncronized blocks.
// Remove the activation from the stack.
//
// If there are locked Java monitors
//    If throw_monitor_exception
//       throws IllegalMonitorStateException
//    Else if install_monitor_exception
//       installs IllegalMonitorStateException
//    Else
//       no error processing
void InterpreterMacroAssembler::remove_activation(
        TosState state,
        Register ret_addr,
        bool throw_monitor_exception,
        bool install_monitor_exception,
        bool notify_jvmdi) {
  // Note: Registers rdx xmm0 may be in use for the
  // result check if synchronized method
  Label unlocked, unlock, no_unlock;

  // get the value of _do_not_unlock_if_synchronized into rdx
  const Address do_not_unlock_if_synchronized(r15_thread,
    in_bytes(JavaThread::do_not_unlock_if_synchronized_offset()));
    // 将当前线程的do_not_unlock_if_synchronized属性拷贝到rdx中
  movbool(rdx, do_not_unlock_if_synchronized);
    // 将当前线程的do_not_unlock_if_synchronized属性置为false
  movbool(do_not_unlock_if_synchronized, false); // reset the flag

 // get method access flags
    // 获取方法的access_flags，判断是否同步synchronized方法
  movptr(rbx, Address(rbp, frame::interpreter_frame_method_offset * wordSize));
  movl(rcx, Address(rbx, Method::access_flags_offset()));
  testl(rcx, JVM_ACC_SYNCHRONIZED);
    // 如果不是跳转到unlocked
  jcc(Assembler::zero, unlocked);

  // Don't unlock anything if the _do_not_unlock_if_synchronized flag
  // is set.
  testbool(rdx);
    // 如果是synchronized方法
    // 如果_do_not_unlock_if_synchronized属性为true，则跳转到no_unlock
  jcc(Assembler::notZero, no_unlock);

  // unlock monitor
    //如果_do_not_unlock_if_synchronized属性为false
    //将保存在rax即栈顶缓存中的方法调用结果放入栈帧中
  push(state); // save result

  // BasicObjectLock will be first in list, since this is a
  // synchronized method. However, need to check that the object has
  // not been unlocked by an explicit monitorexit bytecode.
    //获取BasicObjectLock的地址
  const Address monitor(rbp, frame::interpreter_frame_initial_sp_offset *
                        wordSize - (int) sizeof(BasicObjectLock));
  // We use c_rarg1 so that if we go slow path it will be the correct
  // register for unlock_object to pass to VM directly
    //将关联的BasicObjectLock地址放入c_rarg1
  lea(c_rarg1, monitor); // address of first monitor

    //将obj属性放入rax中
  movptr(rax, Address(c_rarg1, BasicObjectLock::obj_offset_in_bytes()));
  testptr(rax, rax);
    //如果rax非空，即未解锁，则跳转到unlock
  jcc(Assembler::notZero, unlock);

    //如果rax为空，即没有经过加锁
    //将方法返回值重新放回rax中
  pop(state);
  if (throw_monitor_exception) {
    // Entry already unlocked, need to throw exception
      //抛出异常，终止处理
    call_VM(noreg, CAST_FROM_FN_PTR(address,
                   InterpreterRuntime::throw_illegal_monitor_state_exception));
    should_not_reach_here();
  } else {
    // Monitor already unlocked during a stack unroll. If requested,
    // install an illegal_monitor_state_exception.  Continue with
    // stack unrolling.
    if (install_monitor_exception) {
      call_VM(noreg, CAST_FROM_FN_PTR(address,
                     InterpreterRuntime::new_illegal_monitor_state_exception));
    }
    jmp(unlocked);
  }

  bind(unlock);
    //执行解锁
  unlock_object(c_rarg1);
    //将方法返回值重新放回rax中
  pop(state);

  // Check that for block-structured locking (i.e., that all locked
  // objects has been unlocked)
  bind(unlocked);

  // rax: Might contain return value

  // Check that all monitors are unlocked
  {
    Label loop, exception, entry, restart;
    const int entry_size = frame::interpreter_frame_monitor_size() * wordSize;
    const Address monitor_block_top(
        rbp, frame::interpreter_frame_monitor_block_top_offset * wordSize);
    const Address monitor_block_bot(
        rbp, frame::interpreter_frame_initial_sp_offset * wordSize);

    bind(restart);
    // We use c_rarg1 so that if we go slow path it will be the correct
    // register for unlock_object to pass to VM directly
      //将monitor_block_top地址放入c_rarg1
    movptr(c_rarg1, monitor_block_top); // points to current entry, starting
                                  // with top-most entry
      //monitor_block_bot的地址放入rbx
    lea(rbx, monitor_block_bot);  // points to word before bottom of
                                  // monitor block
    jmp(entry);

    // Entry already locked, need to throw exception
    bind(exception);

    if (throw_monitor_exception) {
      // Throw exception
        //抛出异常
      MacroAssembler::call_VM(noreg,
                              CAST_FROM_FN_PTR(address, InterpreterRuntime::
                                   throw_illegal_monitor_state_exception));
      should_not_reach_here();
    } else {
      // Stack unrolling. Unlock object and install illegal_monitor_exception.
      // Unlock does not block, so don't have to worry about the frame.
      // We don't have to preserve c_rarg1 since we are going to throw an exception.

        //将rax中的方法调用结果入栈
      push(state);
        //解锁
      unlock_object(c_rarg1);
        //将rax中的方法调用结果出栈
      pop(state);

      if (install_monitor_exception) {
          //install_monitor_exception为true，抛出异常
        call_VM(noreg, CAST_FROM_FN_PTR(address,
                                        InterpreterRuntime::
                                        new_illegal_monitor_state_exception));
      }

        //install_monitor_exception为false，跳转到restart开始下一次循环
      jmp(restart);
    }

    bind(loop);
    // check if current entry is used
      //判断BasicObjectLock的obj属性是否为NULL
    cmpptr(Address(c_rarg1, BasicObjectLock::obj_offset_in_bytes()), (int32_t) NULL);
      //如果不是，说明还有未解锁的BasicObjectLock，即synchronized方法中用synchronized关键字修饰的代码块，跳转到exception
    jcc(Assembler::notEqual, exception);

    addptr(c_rarg1, entry_size); // otherwise advance to next entry
    bind(entry);
      //比较两个地址是否一致，如果一致则表示到达了底部
    cmpptr(c_rarg1, rbx); // check if bottom reached
    jcc(Assembler::notEqual, loop); // if not at bottom then check this entry
  }

  bind(no_unlock);

  // jvmti support
  if (notify_jvmdi) {
    notify_method_exit(state, NotifyJVMTI);    // preserve TOSCA
  } else {
    notify_method_exit(state, SkipNotifyJVMTI); // preserve TOSCA
  }

  // remove activation
  // get sender sp
    //移除当前栈帧，准备跳转到调用此方法的栈帧
  movptr(rbx,
         Address(rbp, frame::interpreter_frame_sender_sp_offset * wordSize));
  leave();                           // remove frame anchor
  pop(ret_addr);                     // get return address
  mov(rsp, rbx);                     // set sp to sender sp
}

#endif // C_INTERP

// Lock object
//
// Args:
//      c_rarg1: BasicObjectLock to be used for locking
//
// Kills:
//      rax
//      c_rarg0, c_rarg1, c_rarg2, c_rarg3, .. (param regs)
//      rscratch1, rscratch2 (scratch regs)
void InterpreterMacroAssembler::lock_object(Register lock_reg) {
  assert(lock_reg == c_rarg1, "The argument is only for looks. It must be c_rarg1");

    //UseHeavyMonitors表示是否只使用重量级锁，默认为false，如果为true则调用InterpreterRuntime::monitorenter方法获取重量级锁
  if (UseHeavyMonitors) {
    call_VM(noreg,
            CAST_FROM_FN_PTR(address, InterpreterRuntime::monitorenter),
            lock_reg);
  } else {
    Label done;

    const Register swap_reg = rax; // Must use rax for cmpxchg instruction，cmpxchg其实就是CAS操作，必须使用rax寄存器作为老数据的存储。
    const Register obj_reg = c_rarg3; // Will contain the oop

      // 其中BasicObjectLock是一个简单的数据结构
    const int obj_offset = BasicObjectLock::obj_offset_in_bytes();
    const int lock_offset = BasicObjectLock::lock_offset_in_bytes ();
    const int mark_offset = lock_offset +
                            BasicLock::displaced_header_offset_in_bytes();

    Label slow_case;

    // Load object pointer into obj_reg %c_rarg3
      //进入此方法目标obj要么是无锁状态，要么是对同一个对象的synchronized嵌套情形下的有锁状态
      //将用于获取锁的实例oop拷贝到obj_reg中，即将加锁对象放入obj_reg寄存器
    movptr(obj_reg, Address(lock_reg, obj_offset));

      //UseBiasedLocking默认为true
      //如果虚拟机参数允许使用偏向锁，那么进入biased_locking_enter（）中
    if (UseBiasedLocking) {
        //首先尝试获取偏向锁，获取成功会跳转到done，否则走到slow_case
        // lock_reg :存储的是分配的BasicObjectLock的指针
        // obj_reg :存储的是锁对象的指针
        // slow_case :即InterpreterRuntime::monitorenter（）；
        // done :标志着获取锁成功。
        // slow_case 和 done 也被传入，这样在biased_locking_enter（）中，就可以根据情况跳到这两处了。
      biased_locking_enter(lock_reg, obj_reg, swap_reg, rscratch1, false, done, &slow_case);
    }

    // Load immediate 1 into swap_reg %rax
      //如果UseBiasedLocking为false或者目标对象的锁不是偏向锁了会走此逻辑
      // 加载1到swap_reg
    movl(swap_reg, 1);

    // Load (object->mark() | 1) into swap_reg %rax
      //计算 object->mark() | 1，结果保存到swap_reg，跟1做或运算将其标记为无锁状态
      // 获取加锁对象的对象头，与1做位或运算，结果放入swap_reg
    orptr(swap_reg, Address(obj_reg, 0));

    // Save (object->mark() | 1) into BasicLock's displaced header
      //将(object->mark() | 1)的结果保存到BasicLock的displaced_header中，保存原来的对象头
    movptr(Address(lock_reg, mark_offset), swap_reg);

    assert(lock_offset == 0,
           "displached header must be first word in BasicObjectLock");

    if (os::is_MP()) lock();
      //lock_reg即是里面的lock属性的地址
      //如果是多核系统，通过lock指令保证cmpxchgp的操作是原子的，即只可能有一个线程操作obj对象头
      //将obj的对象头同rax即swap_reg比较，如果相等将lock_reg写入obj对象头，即lock属性写入obj对象头，如果不等于则将obj对象头放入rax中
    cmpxchgptr(lock_reg, Address(obj_reg, 0));
    if (PrintBiasedLockingStatistics) {
        //增加计数器
      cond_inc32(Assembler::zero,
                 ExternalAddress((address) BiasedLocking::fast_path_entry_count_addr()));
    }
      //如果等于，说明obj的对象头是无锁状态的，此时跟1做或运算，结果不变
    jcc(Assembler::zero, done);

    // Test if the oopMark is an obvious stack pointer, i.e.,
    //  1) (mark & 7) == 0, and
    //  2) rsp <= mark < mark + os::pagesize()
    //
    // These 3 tests can be done by evaluating the following
    // expression: ((mark - rsp) & (7 - os::vm_page_size())),
    // assuming both stack pointer and pagesize have their
    // least significant 3 bits clear.
    // NOTE: the oopMark is in swap_reg %rax as the result of cmpxchg
      //如果不等于，说明obj的对象头要么是偏向锁，要么是重量级锁，多线程下可能其他线程已经获取了该对象的轻量级锁
      //下面的汇编指令相当于执行如下判断，判断目标对应的对象头是否属于当前调用栈帧，如果是说明还是当前线程占有该轻量级锁，如果不是则说明其他线程占用了轻量级锁或者已经膨胀成重量级锁
    subptr(swap_reg, rsp);
    andptr(swap_reg, 7 - os::vm_page_size());

    // Save the test result, for recursive case, the result is zero
      //在递归即synchronized嵌套使用的情形下，上述指令计算的结果就是0
      //当BasicLock的displaced_header置为NULL
    movptr(Address(lock_reg, mark_offset), swap_reg);

    if (PrintBiasedLockingStatistics) {
        //增加计数器
      cond_inc32(Assembler::zero,
                 ExternalAddress((address) BiasedLocking::fast_path_entry_count_addr()));
    }
      //如果andptr的结果为0，说明当前线程已经获取了轻量级锁则跳转到done
    jcc(Assembler::zero, done);

      // 直接跳到这，需要进入InterpreterRuntime::monitorenter（）中去获取锁。
      //否则执行InterpreterRuntime::monitorenter将轻量级锁膨胀成重量级锁或者获取重量级锁
    bind(slow_case);

    // Call the runtime routine for slow case
    call_VM(noreg,
            CAST_FROM_FN_PTR(address, InterpreterRuntime::monitorenter),
            lock_reg);

      // 直接跳到这表明获取锁成功，接下来就会返回到entry_point例程进行字节码的执行了。
    bind(done);
  }
}


// Unlocks an object. Used in monitorexit bytecode and
// remove_activation.  Throws an IllegalMonitorException if object is
// not locked by current thread.
//
// Args:
//      c_rarg1: BasicObjectLock for lock
//
// Kills:
//      rax
//      c_rarg0, c_rarg1, c_rarg2, c_rarg3, ... (param regs)
//      rscratch1, rscratch2 (scratch regs)
void InterpreterMacroAssembler::unlock_object(Register lock_reg) {
  assert(lock_reg == c_rarg1, "The argument is only for looks. It must be rarg1");

  if (UseHeavyMonitors) {
      //如果只使用重量级锁，UseHeavyMonitors默认为false
    call_VM(noreg,
            CAST_FROM_FN_PTR(address, InterpreterRuntime::monitorexit),
            lock_reg);
  } else {
    Label done;

    const Register swap_reg   = rax;  // Must use rax for cmpxchg instruction
    const Register header_reg = c_rarg2;  // Will contain the old oopMark
    const Register obj_reg    = c_rarg3;  // Will contain the oop

      //保存bcp，方便解锁异常时回滚
    save_bcp(); // Save in case of exception

    // Convert from BasicObjectLock structure to object and BasicLock
    // structure Store the BasicLock address into %rax
      //将lock属性的地址复制到swap_reg
    lea(swap_reg, Address(lock_reg, BasicObjectLock::lock_offset_in_bytes()));

    // Load oop into obj_reg(%c_rarg3)
      //将obj属性复制到obj_reg
    movptr(obj_reg, Address(lock_reg, BasicObjectLock::obj_offset_in_bytes()));

    // Free entry
      //将obj属性置为NULL
    movptr(Address(lock_reg, BasicObjectLock::obj_offset_in_bytes()), (int32_t)NULL_WORD);

    if (UseBiasedLocking) {
        //如果持有偏向锁，则解锁完成后跳转到done
        // 这里的代码见 hotspot/src/cpu/x86/vm/macroAssembler_x86.cpp的biased_locking_exit方法
      biased_locking_exit(obj_reg, header_reg, done);
    }

    // Load the old header from BasicLock structure
      //将BasicLock的displaced_header属性复制到header_reg中，即该对象原来的对象头
    movptr(header_reg, Address(swap_reg,
                               BasicLock::displaced_header_offset_in_bytes()));

    // Test for recursion
      //判断这个是否为空
    testptr(header_reg, header_reg);

    // zero for recursive case
      //如果为空说明这是对同一目标对象的synchronized嵌套情形，则跳转到done，等到外层的synchronized解锁恢复目标对象的对象头
    jcc(Assembler::zero, done);

    // Atomic swap back the old header
    if (os::is_MP()) lock();
      //如果是多核系统则通过lock指令前缀将cmpxchg变成一个原子操作，即只能有一个线程同时操作obj的对象头
      //将obj的对象头同rax即swap_reg，即lock属性地址比较，如果相等把header_reg写入到obj的对象头中即恢复对象头，如果不等把obj对象头写入rax中
    cmpxchgptr(header_reg, Address(obj_reg, 0));

    // zero for recursive case
      //如果相等，说明还是轻量级锁，解锁完成
    jcc(Assembler::zero, done);

    // Call the runtime routine for slow case.
      //如果不等于，说明轻量级锁被膨胀成重量级锁，恢复obj属性，因为上面将该属性置为NULL
    movptr(Address(lock_reg, BasicObjectLock::obj_offset_in_bytes()),
         obj_reg); // restore obj
      //调用InterpreterRuntime::monitorexit，完成重量级锁退出
    call_VM(noreg,
            CAST_FROM_FN_PTR(address, InterpreterRuntime::monitorexit),
            lock_reg);

    bind(done);

      //恢复bcp
    restore_bcp();
  }
}

#ifndef CC_INTERP

void InterpreterMacroAssembler::test_method_data_pointer(Register mdp,
                                                         Label& zero_continue) {
  assert(ProfileInterpreter, "must be profiling interpreter");
  movptr(mdp, Address(rbp, frame::interpreter_frame_mdx_offset * wordSize));
  testptr(mdp, mdp);
  jcc(Assembler::zero, zero_continue);
}


// Set the method data pointer for the current bcp.
void InterpreterMacroAssembler::set_method_data_pointer_for_bcp() {
  assert(ProfileInterpreter, "must be profiling interpreter");
  Label set_mdp;
  push(rax);
  push(rbx);

  get_method(rbx);
  // Test MDO to avoid the call if it is NULL.
  movptr(rax, Address(rbx, in_bytes(Method::method_data_offset())));
  testptr(rax, rax);
  jcc(Assembler::zero, set_mdp);
  // rbx: method
  // r13: bcp
  call_VM_leaf(CAST_FROM_FN_PTR(address, InterpreterRuntime::bcp_to_di), rbx, r13);
  // rax: mdi
  // mdo is guaranteed to be non-zero here, we checked for it before the call.
  movptr(rbx, Address(rbx, in_bytes(Method::method_data_offset())));
  addptr(rbx, in_bytes(MethodData::data_offset()));
  addptr(rax, rbx);
  bind(set_mdp);
  movptr(Address(rbp, frame::interpreter_frame_mdx_offset * wordSize), rax);
  pop(rbx);
  pop(rax);
}

void InterpreterMacroAssembler::verify_method_data_pointer() {
  assert(ProfileInterpreter, "must be profiling interpreter");
#ifdef ASSERT
  Label verify_continue;
  push(rax);
  push(rbx);
  push(c_rarg3);
  push(c_rarg2);
  test_method_data_pointer(c_rarg3, verify_continue); // If mdp is zero, continue
  get_method(rbx);

  // If the mdp is valid, it will point to a DataLayout header which is
  // consistent with the bcp.  The converse is highly probable also.
  load_unsigned_short(c_rarg2,
                      Address(c_rarg3, in_bytes(DataLayout::bci_offset())));
  addptr(c_rarg2, Address(rbx, Method::const_offset()));
  lea(c_rarg2, Address(c_rarg2, ConstMethod::codes_offset()));
  cmpptr(c_rarg2, r13);
  jcc(Assembler::equal, verify_continue);
  // rbx: method
  // r13: bcp
  // c_rarg3: mdp
  call_VM_leaf(CAST_FROM_FN_PTR(address, InterpreterRuntime::verify_mdp),
               rbx, r13, c_rarg3);
  bind(verify_continue);
  pop(c_rarg2);
  pop(c_rarg3);
  pop(rbx);
  pop(rax);
#endif // ASSERT
}


void InterpreterMacroAssembler::set_mdp_data_at(Register mdp_in,
                                                int constant,
                                                Register value) {
  assert(ProfileInterpreter, "must be profiling interpreter");
  Address data(mdp_in, constant);
  movptr(data, value);
}


void InterpreterMacroAssembler::increment_mdp_data_at(Register mdp_in,
                                                      int constant,
                                                      bool decrement) {
  // Counter address
  Address data(mdp_in, constant);

  increment_mdp_data_at(data, decrement);
}

void InterpreterMacroAssembler::increment_mdp_data_at(Address data,
                                                      bool decrement) {
  assert(ProfileInterpreter, "must be profiling interpreter");
  // %%% this does 64bit counters at best it is wasting space
  // at worst it is a rare bug when counters overflow

  if (decrement) {
    // Decrement the register.  Set condition codes.
    addptr(data, (int32_t) -DataLayout::counter_increment);
    // If the decrement causes the counter to overflow, stay negative
    Label L;
    jcc(Assembler::negative, L);
    addptr(data, (int32_t) DataLayout::counter_increment);
    bind(L);
  } else {
    assert(DataLayout::counter_increment == 1,
           "flow-free idiom only works with 1");
    // Increment the register.  Set carry flag.
    addptr(data, DataLayout::counter_increment);
    // If the increment causes the counter to overflow, pull back by 1.
    sbbptr(data, (int32_t)0);
  }
}


void InterpreterMacroAssembler::increment_mdp_data_at(Register mdp_in,
                                                      Register reg,
                                                      int constant,
                                                      bool decrement) {
  Address data(mdp_in, reg, Address::times_1, constant);

  increment_mdp_data_at(data, decrement);
}

void InterpreterMacroAssembler::set_mdp_flag_at(Register mdp_in,
                                                int flag_byte_constant) {
  assert(ProfileInterpreter, "must be profiling interpreter");
  int header_offset = in_bytes(DataLayout::header_offset());
  int header_bits = DataLayout::flag_mask_to_header_mask(flag_byte_constant);
  // Set the flag
  orl(Address(mdp_in, header_offset), header_bits);
}



void InterpreterMacroAssembler::test_mdp_data_at(Register mdp_in,
                                                 int offset,
                                                 Register value,
                                                 Register test_value_out,
                                                 Label& not_equal_continue) {
  assert(ProfileInterpreter, "must be profiling interpreter");
  if (test_value_out == noreg) {
    cmpptr(value, Address(mdp_in, offset));
  } else {
    // Put the test value into a register, so caller can use it:
    movptr(test_value_out, Address(mdp_in, offset));
    cmpptr(test_value_out, value);
  }
  jcc(Assembler::notEqual, not_equal_continue);
}


void InterpreterMacroAssembler::update_mdp_by_offset(Register mdp_in,
                                                     int offset_of_disp) {
  assert(ProfileInterpreter, "must be profiling interpreter");
  Address disp_address(mdp_in, offset_of_disp);
  addptr(mdp_in, disp_address);
  movptr(Address(rbp, frame::interpreter_frame_mdx_offset * wordSize), mdp_in);
}


void InterpreterMacroAssembler::update_mdp_by_offset(Register mdp_in,
                                                     Register reg,
                                                     int offset_of_disp) {
  assert(ProfileInterpreter, "must be profiling interpreter");
  Address disp_address(mdp_in, reg, Address::times_1, offset_of_disp);
  addptr(mdp_in, disp_address);
  movptr(Address(rbp, frame::interpreter_frame_mdx_offset * wordSize), mdp_in);
}


void InterpreterMacroAssembler::update_mdp_by_constant(Register mdp_in,
                                                       int constant) {
  assert(ProfileInterpreter, "must be profiling interpreter");
  addptr(mdp_in, constant);
  movptr(Address(rbp, frame::interpreter_frame_mdx_offset * wordSize), mdp_in);
}


void InterpreterMacroAssembler::update_mdp_for_ret(Register return_bci) {
  assert(ProfileInterpreter, "must be profiling interpreter");
  push(return_bci); // save/restore across call_VM
  call_VM(noreg,
          CAST_FROM_FN_PTR(address, InterpreterRuntime::update_mdp_for_ret),
          return_bci);
  pop(return_bci);
}


void InterpreterMacroAssembler::profile_taken_branch(Register mdp,
                                                     Register bumped_count) {
  if (ProfileInterpreter) {
    Label profile_continue;

    // If no method data exists, go to profile_continue.
    // Otherwise, assign to mdp
    test_method_data_pointer(mdp, profile_continue);

    // We are taking a branch.  Increment the taken count.
    // We inline increment_mdp_data_at to return bumped_count in a register
    //increment_mdp_data_at(mdp, in_bytes(JumpData::taken_offset()));
    Address data(mdp, in_bytes(JumpData::taken_offset()));
    movptr(bumped_count, data);
    assert(DataLayout::counter_increment == 1,
            "flow-free idiom only works with 1");
    addptr(bumped_count, DataLayout::counter_increment);
    sbbptr(bumped_count, 0);
    movptr(data, bumped_count); // Store back out

    // The method data pointer needs to be updated to reflect the new target.
    update_mdp_by_offset(mdp, in_bytes(JumpData::displacement_offset()));
    bind(profile_continue);
  }
}


void InterpreterMacroAssembler::profile_not_taken_branch(Register mdp) {
  if (ProfileInterpreter) {
    Label profile_continue;

    // If no method data exists, go to profile_continue.
    test_method_data_pointer(mdp, profile_continue);

    // We are taking a branch.  Increment the not taken count.
    increment_mdp_data_at(mdp, in_bytes(BranchData::not_taken_offset()));

    // The method data pointer needs to be updated to correspond to
    // the next bytecode
    update_mdp_by_constant(mdp, in_bytes(BranchData::branch_data_size()));
    bind(profile_continue);
  }
}

void InterpreterMacroAssembler::profile_call(Register mdp) {
  if (ProfileInterpreter) {
    Label profile_continue;

    // If no method data exists, go to profile_continue.
    test_method_data_pointer(mdp, profile_continue);

    // We are making a call.  Increment the count.
    increment_mdp_data_at(mdp, in_bytes(CounterData::count_offset()));

    // The method data pointer needs to be updated to reflect the new target.
    update_mdp_by_constant(mdp, in_bytes(CounterData::counter_data_size()));
    bind(profile_continue);
  }
}


void InterpreterMacroAssembler::profile_final_call(Register mdp) {
  if (ProfileInterpreter) {
    Label profile_continue;

    // If no method data exists, go to profile_continue.
    test_method_data_pointer(mdp, profile_continue);

    // We are making a call.  Increment the count.
    increment_mdp_data_at(mdp, in_bytes(CounterData::count_offset()));

    // The method data pointer needs to be updated to reflect the new target.
    update_mdp_by_constant(mdp,
                           in_bytes(VirtualCallData::
                                    virtual_call_data_size()));
    bind(profile_continue);
  }
}


void InterpreterMacroAssembler::profile_virtual_call(Register receiver,
                                                     Register mdp,
                                                     Register reg2,
                                                     bool receiver_can_be_null) {
  if (ProfileInterpreter) {
    Label profile_continue;

    // If no method data exists, go to profile_continue.
    test_method_data_pointer(mdp, profile_continue);

    Label skip_receiver_profile;
    if (receiver_can_be_null) {
      Label not_null;
      testptr(receiver, receiver);
      jccb(Assembler::notZero, not_null);
      // We are making a call.  Increment the count for null receiver.
      increment_mdp_data_at(mdp, in_bytes(CounterData::count_offset()));
      jmp(skip_receiver_profile);
      bind(not_null);
    }

    // Record the receiver type.
    record_klass_in_profile(receiver, mdp, reg2, true);
    bind(skip_receiver_profile);

    // The method data pointer needs to be updated to reflect the new target.
    update_mdp_by_constant(mdp,
                           in_bytes(VirtualCallData::
                                    virtual_call_data_size()));
    bind(profile_continue);
  }
}

// This routine creates a state machine for updating the multi-row
// type profile at a virtual call site (or other type-sensitive bytecode).
// The machine visits each row (of receiver/count) until the receiver type
// is found, or until it runs out of rows.  At the same time, it remembers
// the location of the first empty row.  (An empty row records null for its
// receiver, and can be allocated for a newly-observed receiver type.)
// Because there are two degrees of freedom in the state, a simple linear
// search will not work; it must be a decision tree.  Hence this helper
// function is recursive, to generate the required tree structured code.
// It's the interpreter, so we are trading off code space for speed.
// See below for example code.
void InterpreterMacroAssembler::record_klass_in_profile_helper(
                                        Register receiver, Register mdp,
                                        Register reg2, int start_row,
                                        Label& done, bool is_virtual_call) {
  if (TypeProfileWidth == 0) {
    if (is_virtual_call) {
      increment_mdp_data_at(mdp, in_bytes(CounterData::count_offset()));
    }
    return;
  }

  int last_row = VirtualCallData::row_limit() - 1;
  assert(start_row <= last_row, "must be work left to do");
  // Test this row for both the receiver and for null.
  // Take any of three different outcomes:
  //   1. found receiver => increment count and goto done
  //   2. found null => keep looking for case 1, maybe allocate this cell
  //   3. found something else => keep looking for cases 1 and 2
  // Case 3 is handled by a recursive call.
  for (int row = start_row; row <= last_row; row++) {
    Label next_test;
    bool test_for_null_also = (row == start_row);

    // See if the receiver is receiver[n].
    int recvr_offset = in_bytes(VirtualCallData::receiver_offset(row));
    test_mdp_data_at(mdp, recvr_offset, receiver,
                     (test_for_null_also ? reg2 : noreg),
                     next_test);
    // (Reg2 now contains the receiver from the CallData.)

    // The receiver is receiver[n].  Increment count[n].
    int count_offset = in_bytes(VirtualCallData::receiver_count_offset(row));
    increment_mdp_data_at(mdp, count_offset);
    jmp(done);
    bind(next_test);

    if (test_for_null_also) {
      Label found_null;
      // Failed the equality check on receiver[n]...  Test for null.
      testptr(reg2, reg2);
      if (start_row == last_row) {
        // The only thing left to do is handle the null case.
        if (is_virtual_call) {
          jccb(Assembler::zero, found_null);
          // Receiver did not match any saved receiver and there is no empty row for it.
          // Increment total counter to indicate polymorphic case.
          increment_mdp_data_at(mdp, in_bytes(CounterData::count_offset()));
          jmp(done);
          bind(found_null);
        } else {
          jcc(Assembler::notZero, done);
        }
        break;
      }
      // Since null is rare, make it be the branch-taken case.
      jcc(Assembler::zero, found_null);

      // Put all the "Case 3" tests here.
      record_klass_in_profile_helper(receiver, mdp, reg2, start_row + 1, done, is_virtual_call);

      // Found a null.  Keep searching for a matching receiver,
      // but remember that this is an empty (unused) slot.
      bind(found_null);
    }
  }

  // In the fall-through case, we found no matching receiver, but we
  // observed the receiver[start_row] is NULL.

  // Fill in the receiver field and increment the count.
  int recvr_offset = in_bytes(VirtualCallData::receiver_offset(start_row));
  set_mdp_data_at(mdp, recvr_offset, receiver);
  int count_offset = in_bytes(VirtualCallData::receiver_count_offset(start_row));
  movl(reg2, DataLayout::counter_increment);
  set_mdp_data_at(mdp, count_offset, reg2);
  if (start_row > 0) {
    jmp(done);
  }
}

// Example state machine code for three profile rows:
//   // main copy of decision tree, rooted at row[1]
//   if (row[0].rec == rec) { row[0].incr(); goto done; }
//   if (row[0].rec != NULL) {
//     // inner copy of decision tree, rooted at row[1]
//     if (row[1].rec == rec) { row[1].incr(); goto done; }
//     if (row[1].rec != NULL) {
//       // degenerate decision tree, rooted at row[2]
//       if (row[2].rec == rec) { row[2].incr(); goto done; }
//       if (row[2].rec != NULL) { count.incr(); goto done; } // overflow
//       row[2].init(rec); goto done;
//     } else {
//       // remember row[1] is empty
//       if (row[2].rec == rec) { row[2].incr(); goto done; }
//       row[1].init(rec); goto done;
//     }
//   } else {
//     // remember row[0] is empty
//     if (row[1].rec == rec) { row[1].incr(); goto done; }
//     if (row[2].rec == rec) { row[2].incr(); goto done; }
//     row[0].init(rec); goto done;
//   }
//   done:

void InterpreterMacroAssembler::record_klass_in_profile(Register receiver,
                                                        Register mdp, Register reg2,
                                                        bool is_virtual_call) {
  assert(ProfileInterpreter, "must be profiling");
  Label done;

  record_klass_in_profile_helper(receiver, mdp, reg2, 0, done, is_virtual_call);

  bind (done);
}

void InterpreterMacroAssembler::profile_ret(Register return_bci,
                                            Register mdp) {
  if (ProfileInterpreter) {
    Label profile_continue;
    uint row;

    // If no method data exists, go to profile_continue.
    test_method_data_pointer(mdp, profile_continue);

    // Update the total ret count.
    increment_mdp_data_at(mdp, in_bytes(CounterData::count_offset()));

    for (row = 0; row < RetData::row_limit(); row++) {
      Label next_test;

      // See if return_bci is equal to bci[n]:
      test_mdp_data_at(mdp,
                       in_bytes(RetData::bci_offset(row)),
                       return_bci, noreg,
                       next_test);

      // return_bci is equal to bci[n].  Increment the count.
      increment_mdp_data_at(mdp, in_bytes(RetData::bci_count_offset(row)));

      // The method data pointer needs to be updated to reflect the new target.
      update_mdp_by_offset(mdp,
                           in_bytes(RetData::bci_displacement_offset(row)));
      jmp(profile_continue);
      bind(next_test);
    }

    update_mdp_for_ret(return_bci);

    bind(profile_continue);
  }
}


void InterpreterMacroAssembler::profile_null_seen(Register mdp) {
  if (ProfileInterpreter) {
    Label profile_continue;

    // If no method data exists, go to profile_continue.
    test_method_data_pointer(mdp, profile_continue);

    set_mdp_flag_at(mdp, BitData::null_seen_byte_constant());

    // The method data pointer needs to be updated.
    int mdp_delta = in_bytes(BitData::bit_data_size());
    if (TypeProfileCasts) {
      mdp_delta = in_bytes(VirtualCallData::virtual_call_data_size());
    }
    update_mdp_by_constant(mdp, mdp_delta);

    bind(profile_continue);
  }
}


void InterpreterMacroAssembler::profile_typecheck_failed(Register mdp) {
  if (ProfileInterpreter && TypeProfileCasts) {
    Label profile_continue;

    // If no method data exists, go to profile_continue.
    test_method_data_pointer(mdp, profile_continue);

    int count_offset = in_bytes(CounterData::count_offset());
    // Back up the address, since we have already bumped the mdp.
    count_offset -= in_bytes(VirtualCallData::virtual_call_data_size());

    // *Decrement* the counter.  We expect to see zero or small negatives.
    increment_mdp_data_at(mdp, count_offset, true);

    bind (profile_continue);
  }
}


void InterpreterMacroAssembler::profile_typecheck(Register mdp, Register klass, Register reg2) {
  if (ProfileInterpreter) {
    Label profile_continue;

    // If no method data exists, go to profile_continue.
    test_method_data_pointer(mdp, profile_continue);

    // The method data pointer needs to be updated.
    int mdp_delta = in_bytes(BitData::bit_data_size());
    if (TypeProfileCasts) {
      mdp_delta = in_bytes(VirtualCallData::virtual_call_data_size());

      // Record the object type.
      record_klass_in_profile(klass, mdp, reg2, false);
    }
    update_mdp_by_constant(mdp, mdp_delta);

    bind(profile_continue);
  }
}


void InterpreterMacroAssembler::profile_switch_default(Register mdp) {
  if (ProfileInterpreter) {
    Label profile_continue;

    // If no method data exists, go to profile_continue.
    test_method_data_pointer(mdp, profile_continue);

    // Update the default case count
    increment_mdp_data_at(mdp,
                          in_bytes(MultiBranchData::default_count_offset()));

    // The method data pointer needs to be updated.
    update_mdp_by_offset(mdp,
                         in_bytes(MultiBranchData::
                                  default_displacement_offset()));

    bind(profile_continue);
  }
}


void InterpreterMacroAssembler::profile_switch_case(Register index,
                                                    Register mdp,
                                                    Register reg2) {
  if (ProfileInterpreter) {
    Label profile_continue;

    // If no method data exists, go to profile_continue.
    test_method_data_pointer(mdp, profile_continue);

    // Build the base (index * per_case_size_in_bytes()) +
    // case_array_offset_in_bytes()
    movl(reg2, in_bytes(MultiBranchData::per_case_size()));
    imulptr(index, reg2); // XXX l ?
    addptr(index, in_bytes(MultiBranchData::case_array_offset())); // XXX l ?

    // Update the case count
    increment_mdp_data_at(mdp,
                          index,
                          in_bytes(MultiBranchData::relative_count_offset()));

    // The method data pointer needs to be updated.
    update_mdp_by_offset(mdp,
                         index,
                         in_bytes(MultiBranchData::
                                  relative_displacement_offset()));

    bind(profile_continue);
  }
}



void InterpreterMacroAssembler::verify_oop(Register reg, TosState state) {
  if (state == atos) {
    MacroAssembler::verify_oop(reg);
  }
}

void InterpreterMacroAssembler::verify_FPU(int stack_depth, TosState state) {
}
#endif // !CC_INTERP


void InterpreterMacroAssembler::notify_method_entry() {
  // Whenever JVMTI is interp_only_mode, method entry/exit events are sent to
  // track stack depth.  If it is possible to enter interp_only_mode we add
  // the code to check if the event should be sent.
  if (JvmtiExport::can_post_interpreter_events()) {
    Label L;
    movl(rdx, Address(r15_thread, JavaThread::interp_only_mode_offset()));
    testl(rdx, rdx);
    jcc(Assembler::zero, L);
    call_VM(noreg, CAST_FROM_FN_PTR(address,
                                    InterpreterRuntime::post_method_entry));
    bind(L);
  }

  {
    SkipIfEqual skip(this, &DTraceMethodProbes, false);
    get_method(c_rarg1);
    call_VM_leaf(CAST_FROM_FN_PTR(address, SharedRuntime::dtrace_method_entry),
                 r15_thread, c_rarg1);
  }

  // RedefineClasses() tracing support for obsolete method entry
  if (RC_TRACE_IN_RANGE(0x00001000, 0x00002000)) {
    get_method(c_rarg1);
    call_VM_leaf(
      CAST_FROM_FN_PTR(address, SharedRuntime::rc_trace_method_entry),
      r15_thread, c_rarg1);
  }
}


void InterpreterMacroAssembler::notify_method_exit(
    TosState state, NotifyMethodExitMode mode) {
  // Whenever JVMTI is interp_only_mode, method entry/exit events are sent to
  // track stack depth.  If it is possible to enter interp_only_mode we add
  // the code to check if the event should be sent.
  if (mode == NotifyJVMTI && JvmtiExport::can_post_interpreter_events()) {
    Label L;
    // Note: frame::interpreter_frame_result has a dependency on how the
    // method result is saved across the call to post_method_exit. If this
    // is changed then the interpreter_frame_result implementation will
    // need to be updated too.

    // For c++ interpreter the result is always stored at a known location in the frame
    // template interpreter will leave it on the top of the stack.
    NOT_CC_INTERP(push(state);)
    movl(rdx, Address(r15_thread, JavaThread::interp_only_mode_offset()));
    testl(rdx, rdx);
    jcc(Assembler::zero, L);
    call_VM(noreg,
            CAST_FROM_FN_PTR(address, InterpreterRuntime::post_method_exit));
    bind(L);
    NOT_CC_INTERP(pop(state));
  }

  {
    SkipIfEqual skip(this, &DTraceMethodProbes, false);
    NOT_CC_INTERP(push(state));
    get_method(c_rarg1);
    call_VM_leaf(CAST_FROM_FN_PTR(address, SharedRuntime::dtrace_method_exit),
                 r15_thread, c_rarg1);
    NOT_CC_INTERP(pop(state));
  }
}

// Jump if ((*counter_addr += increment) & mask) satisfies the condition.
void InterpreterMacroAssembler::increment_mask_and_jump(Address counter_addr,
                                                        int increment, int mask,
                                                        Register scratch, bool preloaded,
                                                        Condition cond, Label* where) {
    //preloaded一般传false
  if (!preloaded) {
      //将_counter属性的值复制到scratch，即rcx中
    movl(scratch, counter_addr);
  }
    //将_counter属性增加increment
  incrementl(scratch, increment);
    //将scratch寄存器中的值写入到_counter属性
  movl(counter_addr, scratch);
    //将mask与scratch中的值做且运算
  andl(scratch, mask);
  if (where != NULL) {
      //如果且运算的结果是0，即达到阈值的时候，则跳转到where，即overflow处
    jcc(cond, *where);
  }
}
