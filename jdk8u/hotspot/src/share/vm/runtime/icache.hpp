/*
 * Copyright (c) 1997, 2011, Oracle and/or its affiliates. All rights reserved.
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

#ifndef SHARE_VM_RUNTIME_ICACHE_HPP
#define SHARE_VM_RUNTIME_ICACHE_HPP

#include "memory/allocation.hpp"
#include "runtime/stubCodeGenerator.hpp"

// Interface for updating the instruction cache.  Whenever the VM modifies
// code, part of the processor instruction cache potentially has to be flushed.

// Default implementation is in icache.cpp, and can be hidden per-platform.
// Most platforms must provide only ICacheStubGenerator::generate_icache_flush().
// Platforms that don't require icache flushing can just nullify the public
// members of AbstractICache in their ICache class.  AbstractICache should never
// be referenced other than by deriving the ICache class from it.
//
// The code for the ICache class and for generate_icache_flush() must be in
// architecture-specific files, i.e., icache_<arch>.hpp/.cpp

class AbstractICache : AllStatic {
 public:
  // The flush stub signature
  // _flush_icache_stub的第一个参数addr表示指令缓存的起始地址，第二个参数lines表示刷新的缓存行的行数，
  // 第三个参数magic，用来将其作为调用结果，来表示stub正确的执行了。因为_flush_icache_stub的特殊性，
  // 所以他必须在其他的stub生成前先生成，而且第一次调用_flush_icache_stub也是刷新_flush_icache_stub自身，因为stub还不存在所以第一次调用时不会真实的刷新。
  typedef int (*flush_icache_stub_t)(address addr, int lines, int magic);

 protected:
  // The flush stub function address
  // _flush_icache_stub表示执行指令缓存刷新的stub
  static flush_icache_stub_t _flush_icache_stub;

  // Call the flush stub
  static void call_flush_stub(address start, int lines);

 public:
  enum {
    stub_size      = 0, // Size of the icache flush stub in bytes
    line_size      = 0, // Icache line size in bytes
    log2_line_size = 0  // log2(line_size)
  };

    // AbstractICache定义的方法只有三个，其中initialize用来初始化属性_flush_icache_stub，
    // 另外两个方法是不同场景下用来刷新指令缓存的，invalidate_word和invalidate_range。
  static void initialize();
  static void invalidate_word(address addr);
  static void invalidate_range(address start, int nbytes);
};


// Must be included before the definition of ICacheStubGenerator
// because ICacheStubGenerator uses ICache definitions.

#ifdef TARGET_ARCH_x86
# include "icache_x86.hpp"
#endif
#ifdef TARGET_ARCH_aarch64
# include "icache_aarch64.hpp"
#endif
#ifdef TARGET_ARCH_sparc
# include "icache_sparc.hpp"
#endif
#ifdef TARGET_ARCH_zero
# include "icache_zero.hpp"
#endif
#ifdef TARGET_ARCH_arm
# include "icache_arm.hpp"
#endif
#ifdef TARGET_ARCH_ppc
# include "icache_ppc.hpp"
#endif



// ICacheStubGenerator继承自StubCodeGenerator，用来生成ICache中刷新指令缓存的stub，其定义在icache.hpp中，其定义的方法只有一个generate_icache_flush。
// ICacheStubGenerator的实现跟特定CPU架构有关，通过configurations.xml指定
class ICacheStubGenerator : public StubCodeGenerator {
 public:
  ICacheStubGenerator(CodeBuffer *c) : StubCodeGenerator(c) {}

  // Generate the icache flush stub.
  //
  // Since we cannot flush the cache when this stub is generated,
  // it must be generated first, and just to be sure, we do extra
  // work to allow a check that these instructions got executed.
  //
  // The flush stub has three parameters (see flush_icache_stub_t).
  //
  //   addr  - Start address, must be aligned at log2_line_size
  //   lines - Number of line_size icache lines to flush
  //   magic - Magic number copied to result register to make sure
  //           the stub executed properly
  //
  // A template for generate_icache_flush is
  //
  //    #define __ _masm->
  //
  //    void ICacheStubGenerator::generate_icache_flush(
  //      ICache::flush_icache_stub_t* flush_icache_stub
  //    ) {
  //      StubCodeMark mark(this, "ICache", "flush_icache_stub");
  //
  //      address start = __ pc();
  //
  //      // emit flush stub asm code
  //
  //      // Must be set here so StubCodeMark destructor can call the flush stub.
  //      *flush_icache_stub = (ICache::flush_icache_stub_t)start;
  //    };
  //
  //    #undef __
  //
  // The first use of flush_icache_stub must apply it to itself.  The
  // StubCodeMark destructor in generate_icache_flush will call Assembler::flush,
  // which in turn will call invalidate_range (see asm/assembler.cpp), which
  // in turn will call the flush stub *before* generate_icache_flush returns.
  // The usual method of having generate_icache_flush return the address of the
  // stub to its caller, which would then, e.g., store that address in
  // flush_icache_stub, won't work.  generate_icache_flush must itself set
  // flush_icache_stub to the address of the stub it generates before
  // the StubCodeMark destructor is invoked.

  void generate_icache_flush(ICache::flush_icache_stub_t* flush_icache_stub);
};

#endif // SHARE_VM_RUNTIME_ICACHE_HPP
