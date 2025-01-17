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
#include "classfile/symbolTable.hpp"
#include "classfile/systemDictionaryShared.hpp"
#include "classfile/verificationType.hpp"
#include "classfile/verifier.hpp"

VerificationType VerificationType::from_tag(u1 tag) {
  switch (tag) {
    case ITEM_Top:     return bogus_type();
    case ITEM_Integer: return integer_type();
    case ITEM_Float:   return float_type();
    case ITEM_Double:  return double_type();
    case ITEM_Long:    return long_type();
    case ITEM_Null:    return null_type();
    default:
      ShouldNotReachHere();
      return bogus_type();
  }
}

// 就是检查一个类型 to （当前类型）是否可以被 from 类型赋值
bool VerificationType::is_reference_assignable_from(
    const VerificationType& from, ClassVerifier* context,
    bool from_field_is_protected, TRAPS) const {
  instanceKlassHandle klass = context->current_class();
    // 如果 from 为 null，则合法（null 可以复制给任意对象和数组类型）
  if (from.is_null()) {
    // null is assignable to any reference
    return true;
  } else if (is_null()) {
    return false;
  } else if (name() == from.name()) {
      // 如果 from 和 to 是同一类型，则也合法
    return true;
  } else if (is_object()) {
      // 对象引用类型
    // We need check the class hierarchy to check assignability
      // 如果 to 是一个对象类型，则需要检查 from 和 to 类型是否有继承关系
    if (name() == vmSymbols::java_lang_Object()) {
      // any object or array is assignable to java.lang.Object
        // 如果 to 为 java.lang.Object 类型，则也是合法
      return true;
    }
      // 加载 to 类型的 class 文件以便获取更多的信息
    Klass* obj = SystemDictionary::resolve_or_fail(
        name(), Handle(THREAD, klass->class_loader()),
        Handle(THREAD, klass->protection_domain()), true, CHECK_false);
    KlassHandle this_class(THREAD, obj);

      // 如果是接口类型，则认为合法（至于为什么，英文注释里有）
    if (this_class->is_interface() && (!from_field_is_protected ||
        from.name() != vmSymbols::java_lang_Object())) {
      // If we are not trying to access a protected field or method in
      // java.lang.Object then we treat interfaces as java.lang.Object,
      // including java.lang.Cloneable and java.io.Serializable.
      return true;
    } else if (from.is_object()) {
        // 如果 from 是对象引用类型，则需要加载 from 类型的类文件
      Klass* from_class = SystemDictionary::resolve_or_fail(
          from.name(), Handle(THREAD, klass->class_loader()),
          Handle(THREAD, klass->protection_domain()), true, CHECK_false);
        // 判断 from 是否是 to 的子类型
      bool result = InstanceKlass::cast(from_class)->is_subclass_of(this_class());
      if (result && DumpSharedSpaces) {
        if (klass()->is_subclass_of(from_class) && klass()->is_subclass_of(this_class())) {
          // No need to save verification dependency. At run time, <klass> will be
          // loaded from the archived only if <from_class> and <this_class> are
          // also loaded from the archive. I.e., all 3 classes are exactly the same
          // as we saw at archive creation time.
        } else {
          // Save the dependency. At run time, we need to check that the condition
          // from_class->is_subclass_of(this_class() is still true.
          Symbol* accessor_clsname = from.name();
          Symbol* target_clsname = this_class()->name();
          SystemDictionaryShared::add_verification_dependency(klass(),
                       accessor_clsname, target_clsname);
        }
      }
      return result;
    }
  } else if (is_array() && from.is_array()) {
      // 判断数组的情况
    VerificationType comp_this = get_component(context, CHECK_false);
    VerificationType comp_from = from.get_component(context, CHECK_false);
    if (!comp_this.is_bogus() && !comp_from.is_bogus()) {
      return comp_this.is_assignable_from(comp_from, context,
                                          from_field_is_protected, THREAD);
    }
  }
  return false;
}

VerificationType VerificationType::get_component(ClassVerifier *context, TRAPS) const {
  assert(is_array() && name()->utf8_length() >= 2, "Must be a valid array");
  Symbol* component;
  switch (name()->byte_at(1)) {
    case 'Z': return VerificationType(Boolean);
    case 'B': return VerificationType(Byte);
    case 'C': return VerificationType(Char);
    case 'S': return VerificationType(Short);
    case 'I': return VerificationType(Integer);
    case 'J': return VerificationType(Long);
    case 'F': return VerificationType(Float);
    case 'D': return VerificationType(Double);
    case '[':
      component = context->create_temporary_symbol(
        name(), 1, name()->utf8_length(),
        CHECK_(VerificationType::bogus_type()));
      return VerificationType::reference_type(component);
    case 'L':
      component = context->create_temporary_symbol(
        name(), 2, name()->utf8_length() - 1,
        CHECK_(VerificationType::bogus_type()));
      return VerificationType::reference_type(component);
    default:
      // Met an invalid type signature, e.g. [X
      return VerificationType::bogus_type();
  }
}

void VerificationType::print_on(outputStream* st) const {
  switch (_u._data) {
    case Bogus:            st->print("top"); break;
    case Category1:        st->print("category1"); break;
    case Category2:        st->print("category2"); break;
    case Category2_2nd:    st->print("category2_2nd"); break;
    case Boolean:          st->print("boolean"); break;
    case Byte:             st->print("byte"); break;
    case Short:            st->print("short"); break;
    case Char:             st->print("char"); break;
    case Integer:          st->print("integer"); break;
    case Float:            st->print("float"); break;
    case Long:             st->print("long"); break;
    case Double:           st->print("double"); break;
    case Long_2nd:         st->print("long_2nd"); break;
    case Double_2nd:       st->print("double_2nd"); break;
    case Null:             st->print("null"); break;
    case ReferenceQuery:   st->print("reference type"); break;
    case Category1Query:   st->print("category1 type"); break;
    case Category2Query:   st->print("category2 type"); break;
    case Category2_2ndQuery: st->print("category2_2nd type"); break;
    default:
      if (is_uninitialized_this()) {
        st->print("uninitializedThis");
      } else if (is_uninitialized()) {
        st->print("uninitialized %d", bci());
      } else {
        name()->print_value_on(st);
      }
  }
}
