/*
 * Copyright (c) 2001, 2012, Oracle and/or its affiliates. All rights reserved.
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

#ifndef SHARE_VM_MEMORY_GENOOPCLOSURES_INLINE_HPP
#define SHARE_VM_MEMORY_GENOOPCLOSURES_INLINE_HPP

#include "memory/cardTableRS.hpp"
#include "memory/defNewGeneration.hpp"
#include "memory/genCollectedHeap.hpp"
#include "memory/genOopClosures.hpp"
#include "memory/genRemSet.hpp"
#include "memory/generation.hpp"
#include "memory/sharedHeap.hpp"
#include "memory/space.hpp"

inline OopsInGenClosure::OopsInGenClosure(Generation* gen) :
  ExtendedOopClosure(gen->ref_processor()), _orig_gen(gen), _rs(NULL) {
  set_generation(gen);
}

inline void OopsInGenClosure::set_generation(Generation* gen) {
  _gen = gen;
  _gen_boundary = _gen->reserved().start();
  // Barrier set for the heap, must be set after heap is initialized
  if (_rs == NULL) {
    GenRemSet* rs = SharedHeap::heap()->rem_set();
    assert(rs->rs_kind() == GenRemSet::CardTable, "Wrong rem set kind");
    _rs = (CardTableRS*)rs;
  }
}

template <class T> inline void OopsInGenClosure::do_barrier(T* p) {
  assert(generation()->is_in_reserved(p), "expected ref in generation");
    //获取p指向的对象地址
  T heap_oop = oopDesc::load_heap_oop(p);
  assert(!oopDesc::is_null(heap_oop), "expected non-null oop");
    //获取heap_oop的完整地址
  oop obj = oopDesc::decode_heap_oop_not_null(heap_oop);
  // If p points to a younger generation, mark the card.
    //_gen_boundary是当前generation的起始地址，如果obj小于gen_boundary，说明obj是更年轻的分代中的
    //如果generation本身就是年轻代，则if不成立，如果是老年代，则要求obj是年轻代的
  if ((HeapWord*)obj < _gen_boundary) {
      //将对应的卡表项标记为youngergen_card
    _rs->inline_write_ref_field_gc(p, obj);
  }
}

template <class T> inline void OopsInGenClosure::par_do_barrier(T* p) {
  assert(generation()->is_in_reserved(p), "expected ref in generation");
  T heap_oop = oopDesc::load_heap_oop(p);
  assert(!oopDesc::is_null(heap_oop), "expected non-null oop");
  oop obj = oopDesc::decode_heap_oop_not_null(heap_oop);
  // If p points to a younger generation, mark the card.
  if ((HeapWord*)obj < gen_boundary()) {
    rs()->write_ref_field_gc_par(p, obj);
  }
}

inline void OopsInKlassOrGenClosure::do_klass_barrier() {
  assert(_scanned_klass != NULL, "Must be");
  _scanned_klass->record_modified_oops();
}

// NOTE! Any changes made here should also be made
// in FastScanClosure::do_oop_work()
template <class T> inline void ScanClosure::do_oop_work(T* p) {
  T heap_oop = oopDesc::load_heap_oop(p);
  // Should we copy the obj?
  if (!oopDesc::is_null(heap_oop)) {
    oop obj = oopDesc::decode_heap_oop_not_null(heap_oop);
    if ((HeapWord*)obj < _boundary) {
      assert(!_g->to()->is_in_reserved(obj), "Scanning field twice?");
      oop new_obj = obj->is_forwarded() ? obj->forwardee()
                                        : _g->copy_to_survivor_space(obj);
      oopDesc::encode_store_heap_oop_not_null(p, new_obj);
    }

    if (is_scanning_a_klass()) {
      do_klass_barrier();
    } else if (_gc_barrier) {
      // Now call parent closure
      do_barrier(p);
    }
  }
}

inline void ScanClosure::do_oop_nv(oop* p)       { ScanClosure::do_oop_work(p); }
inline void ScanClosure::do_oop_nv(narrowOop* p) { ScanClosure::do_oop_work(p); }

// NOTE! Any changes made here should also be made
// in ScanClosure::do_oop_work()
template <class T> inline void FastScanClosure::do_oop_work(T* p) {
    // 从地址p处获取对象
  T heap_oop = oopDesc::load_heap_oop(p);
  // Should we copy the obj?
  if (!oopDesc::is_null(heap_oop)) {
      //加载目标oop
    oop obj = oopDesc::decode_heap_oop_not_null(heap_oop);
    if ((HeapWord*)obj < _boundary) {
        //如果obj是年轻代的对象
      assert(!_g->to()->is_in_reserved(obj), "Scanning field twice?");
        //is_forwarded为true说明该对象已经执行被promote了
        //如果为false，则执行promote，即拷贝到to区或者老年代
        //new_obj就是执行promote的对象新地址
      oop new_obj = obj->is_forwarded() ? obj->forwardee()
                                        : _g->copy_to_survivor_space(obj);
        //让p指向对象的新地址
      oopDesc::encode_store_heap_oop_not_null(p, new_obj);
      if (is_scanning_a_klass()) {
        do_klass_barrier();
      } else if (_gc_barrier) {
        // Now call parent closure
          //初始化时_gen_boundary指向年轻代的起始地址，执行GenCollectedHeap::gen_process_roots时会将其置为老年代的起始地址，此时do_barrier会将p对应的卡表项置为youngergen_card
        do_barrier(p);
      }
    }
  }
}

inline void FastScanClosure::do_oop_nv(oop* p)       { FastScanClosure::do_oop_work(p); }
inline void FastScanClosure::do_oop_nv(narrowOop* p) { FastScanClosure::do_oop_work(p); }

// Note similarity to ScanClosure; the difference is that
// the barrier set is taken care of outside this closure.
template <class T> inline void ScanWeakRefClosure::do_oop_work(T* p) {
    //校验oop非空
  assert(!oopDesc::is_null(*p), "null weak reference?");
    //获取oop
  oop obj = oopDesc::load_decode_heap_oop_not_null(p);
  // weak references are sometimes scanned twice; must check
  // that to-space doesn't already contain this object
    //校验obj在年轻代中且不在to区中
  if ((HeapWord*)obj < _boundary && !_g->to()->is_in_reserved(obj)) {
      //获取obj的拷贝地址，如果该对象因为Space压缩已经有拷贝地址则使用该地址，否则将该对象拷贝到to区或者老年代
      //copy_to_survivor_space方法会完成拷贝并返回拷贝到to区或者老年代的新地址，如果拷贝失败返回NULL
    oop new_obj = obj->is_forwarded() ? obj->forwardee()
                                      : _g->copy_to_survivor_space(obj);
      //将p指向新地址
    oopDesc::encode_store_heap_oop_not_null(p, new_obj);
  }
}

inline void ScanWeakRefClosure::do_oop_nv(oop* p)       { ScanWeakRefClosure::do_oop_work(p); }
inline void ScanWeakRefClosure::do_oop_nv(narrowOop* p) { ScanWeakRefClosure::do_oop_work(p); }

#endif // SHARE_VM_MEMORY_GENOOPCLOSURES_INLINE_HPP
