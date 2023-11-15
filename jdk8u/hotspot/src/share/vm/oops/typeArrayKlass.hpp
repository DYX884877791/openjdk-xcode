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

#ifndef SHARE_VM_OOPS_TYPEARRAYKLASS_HPP
#define SHARE_VM_OOPS_TYPEARRAYKLASS_HPP

#include "classfile/classLoaderData.hpp"
#include "oops/arrayKlass.hpp"

// A TypeArrayKlass is the klass of a typeArray
// It contains the type and size of the elements

// TypeArrayKlass是ArrayKlass的子类，用于表示数组元素是基本类型的数组，表示基本数组类的元信息
/**
 * 数组类和普通类不同，数组类没有对应的Class文件，所以数组类是直接被虚拟机创建的。HotSpot在初始化时就会创建好8个基本类型的一维数组对象TypeArrayKlass。
 * 之前在HotSpot启动时，调用initializeJVM()方法初始化HotSpot，这个方法会最终调用到Universe::genesis()方法，在这个方法中初始化基本类型的一维数组对象TypeArrayKlass。
 * 例如初始化boolean类型的一维数组，调用语句如下：
 *   _boolArrayKlassObj      = TypeArrayKlass::create_klass(T_BOOLEAN, sizeof(jboolean), CHECK);
 *   其中_boolArrayKlassObj是声明在universe.cpp文件中的全局变量，如下：
 *   Klass* Universe::_boolArrayKlassObj                 = NULL;
 */
class TypeArrayKlass : public ArrayKlass {
  friend class VMStructs;
 private:
    // _max_length表示该数组允许的最大长度。
  jint _max_length;            // maximum number of elements allowed in an array

  // Constructor
  TypeArrayKlass(BasicType type, Symbol* name);
  static TypeArrayKlass* allocate(ClassLoaderData* loader_data, BasicType type, Symbol* name, TRAPS);
 public:
  TypeArrayKlass() {} // For dummy objects.

  // instance variables
  jint max_length()                     { return _max_length; }
  void set_max_length(jint m)           { _max_length = m;    }

  // testers
  bool oop_is_typeArray_slow() const    { return true; }

  // klass allocation
  // 调用TypeArrayKlass::create_klass()方法创建TypeArrayKlass对象
  static TypeArrayKlass* create_klass(BasicType type, const char* name_str,
                               TRAPS);
  static inline Klass* create_klass(BasicType type, int scale, TRAPS) {
    TypeArrayKlass* tak = create_klass(type, external_name(type), CHECK_NULL);
    assert(scale == (1 << tak->log2_element_size()), "scale must check out");
    return tak;
  }

  int oop_size(oop obj) const;

  bool compute_is_subtype_of(Klass* k);

  // Allocation
  typeArrayOop allocate_common(int length, bool do_zero, TRAPS);
  typeArrayOop allocate(int length, TRAPS) { return allocate_common(length, true, THREAD); }
  oop multi_allocate(int rank, jint* sizes, TRAPS);

  oop protection_domain() const { return NULL; }

  // Copying
  void  copy_array(arrayOop s, int src_pos, arrayOop d, int dst_pos, int length, TRAPS);

  // Iteration
  int oop_oop_iterate(oop obj, ExtendedOopClosure* blk);
  int oop_oop_iterate_m(oop obj, ExtendedOopClosure* blk, MemRegion mr);

  // Garbage collection
  void oop_follow_contents(oop obj);
  int  oop_adjust_pointers(oop obj);

  // Parallel Scavenge and Parallel Old
  PARALLEL_GC_DECLS

 protected:
  // Find n'th dimensional array
  virtual Klass* array_klass_impl(bool or_null, int n, TRAPS);

  // Returns the array class with this class as element type
  virtual Klass* array_klass_impl(bool or_null, TRAPS);

 public:
  // Casting from Klass*
  static TypeArrayKlass* cast(Klass* k) {
    assert(k->oop_is_typeArray(), "cast to TypeArrayKlass");
    return (TypeArrayKlass*) k;
  }

  // Naming
  static const char* external_name(BasicType type);

  // Sizing
  static int header_size()  { return sizeof(TypeArrayKlass)/HeapWordSize; }
  int size() const          { return ArrayKlass::static_size(header_size()); }

  // Initialization (virtual from Klass)
  void initialize(TRAPS);

 public:
  // Printing
#ifndef PRODUCT
  void oop_print_on(oop obj, outputStream* st);
#endif

  void print_on(outputStream* st) const;
  void print_value_on(outputStream* st) const;

 public:
  const char* internal_name() const;
};

#endif // SHARE_VM_OOPS_TYPEARRAYKLASS_HPP
