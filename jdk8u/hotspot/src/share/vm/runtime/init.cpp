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
#include "classfile/symbolTable.hpp"
#include "code/icBuffer.hpp"
#include "gc_interface/collectedHeap.hpp"
#include "interpreter/bytecodes.hpp"
#include "memory/universe.hpp"
#include "prims/methodHandles.hpp"
#include "runtime/handles.inline.hpp"
#include "runtime/icache.hpp"
#include "runtime/init.hpp"
#include "runtime/safepoint.hpp"
#include "runtime/sharedRuntime.hpp"
#include "services/memTracker.hpp"
#include "utilities/macros.hpp"
#include "utilities/slog.hpp"


// Initialization done by VM thread in vm_init_globals()
void check_ThreadShadow();
void eventlog_init();
void mutex_init();
void chunkpool_init();
void perfMemory_init();

// Initialization done by Java thread in init_globals()
void management_init();
void bytecodes_init();
void classLoader_init();
void codeCache_init();
void VM_Version_init();
void os_init_globals();        // depends on VM_Version_init, before universe_init
void stubRoutines_init1();
jint universe_init();          // depends on codeCache_init and stubRoutines_init
void interpreter_init();       // before any methods loaded
void invocationCounter_init(); // before any methods loaded
void marksweep_init();
void accessFlags_init();
void templateTable_init();
void InterfaceSupport_init();
void universe2_init();  // dependent on codeCache_init and stubRoutines_init, loads primordial classes
void referenceProcessor_init();
void jni_handles_init();
void vmStructs_init();

void vtableStubs_init();
void InlineCacheBuffer_init();
void compilerOracle_init();
void compilationPolicy_init();
void compileBroker_init();

// Initialization after compiler initialization
bool universe_post_init();  // must happen after compiler_init
void javaClasses_init();  // must happen after vtable initialization
void stubRoutines_init2(); // note: StubRoutines need 2-phase init

// Do not disable thread-local-storage, as it is important for some
// JNI/JVM/JVMTI functions and signal handlers to work properly
// during VM shutdown
void perfMemory_exit();
void ostream_exit();

/**
 * 1.初始化java基本类型系统
 * 2.初始化事件队列
 * 3.初始化全局锁
 * 4.初始化chunkpool
 * 这是hotspot实现的内存池，包括_large_pool，_medium_pool，_small_pool和_tiny_pool，这样系统就不必执行malloc/free
 * 5.初始化JVM性能统计数据区(PerfData)，由选项UsePerfData设置。
 */
void vm_init_globals() {
    // threadshadow初始化,该类是处理线程的exception,是所有线程类的父类.线程的exception是从外部挂上去的,这个挂载就是threadshadow类,所以就像是影子一样如影随从.
  check_ThreadShadow();
    // basic_types初始化,基本类型的初始化,判断类型和大小是否是正确
  basic_types_init();
    // eventlog的初始化,全部事件有消息,违例,多重定义,类未加载和破环优化的消息.
  eventlog_init();
    // 互斥锁初始化,使用宏定义来预先定义了所有的互斥锁类型
  mutex_init();
    // 小块内存初始化
  chunkpool_init();
    // 永久区内存初始化
  perfMemory_init();
}


jint init_globals() {
  slog_debug("进入hotspot/src/share/vm/runtime/init.cpp中的init_globals函数...");
  HandleMark hm;
  management_init();
    // 在init_globals()函数中调用bytecodes_init()函数初始化好字节码指令后
  bytecodes_init();
  classLoader_init();
  codeCache_init();
  VM_Version_init();
  os_init_globals();
  stubRoutines_init1();
    // 全局初始化,这个又涉及了诸多初始化方面
  jint status = universe_init();  // dependent on codeCache_init and
                                  // stubRoutines_init1 and metaspace_init.
  if (status != JNI_OK)
    return status;

    // 在bytecodes_init之后会调用interpreter_init()函数初始化解释器。函数最终会调用到TemplateInterpreter::initialize()函数
    // 解释器的引用初始化.
  interpreter_init();  // before any methods loaded
    // 方法调用计数器初始化.
  invocationCounter_init();  // before any methods loaded
  marksweep_init();
    // 获取权限初始化,比如public,private等.
  accessFlags_init();
  templateTable_init();
    // 接口的额外支持的初始化,主要针对ASSERT来说.
  InterfaceSupport_init();
  SharedRuntime::generate_stubs();
    // 类似universe_init的初始化,只是这里涉及的内容比较少.
  universe2_init();  // dependent on codeCache_init and stubRoutines_init1
    // 引用处理的初始化,主要是和软引用,弱引用的处理有关.
  referenceProcessor_init();
    // jni的调用初始化.
  jni_handles_init();
#if INCLUDE_VM_STRUCTS
    // VM结构的初始化,主要是校验一些基本设置和数据就够是否正确.
  vmStructs_init();
#endif // INCLUDE_VM_STRUCTS

    // 虚表中有关调用的初始化
  vtableStubs_init();
    // 内联缓存初始化
  InlineCacheBuffer_init();
    // 编译器初始化,主要针对命令行和文件的编译.
  compilerOracle_init();
    // 编译策略初始化,这里主要和JIT和AOT有关.
  compilationPolicy_init();
  compileBroker_init();
  VMRegImpl::set_regName();

  if (!universe_post_init()) {
    return JNI_ERR;
  }
    // 有关java的class文件初始化
  javaClasses_init();   // must happen after vtable initialization
    // 调用的第二次初始化,和第一次一样,只是由于之前有了解释器和编译器等需要调用的组件的加入,这里就再次生成一遍.
  stubRoutines_init2(); // note: StubRoutines need 2-phase init

#if INCLUDE_NMT
  // Solaris stack is walkable only after stubRoutines are set up.
  // On Other platforms, the stack is always walkable.
  NMT_stack_walkable = true;
#endif // INCLUDE_NMT

  // All the flags that get adjusted by VM_Version_init and os::init_2
  // have been set so dump the flags now.
  if (PrintFlagsFinal) {
    CommandLineFlags::printFlags(tty, false);
  }

  return JNI_OK;
}


void exit_globals() {
  static bool destructorsCalled = false;
  if (!destructorsCalled) {
    destructorsCalled = true;
    perfMemory_exit();
    if (PrintSafepointStatistics) {
      // Print the collected safepoint statistics.
      SafepointSynchronize::print_stat_on_exit();
    }
    if (PrintStringTableStatistics) {
      SymbolTable::dump(tty);
      StringTable::dump(tty);
    }
    ostream_exit();
  }
}


static bool _init_completed = false;

bool is_init_completed() {
  return _init_completed;
}


void set_init_completed() {
  assert(Universe::is_fully_initialized(), "Should have completed initialization");
  _init_completed = true;
}
