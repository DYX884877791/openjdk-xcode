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

#ifndef SHARE_VM_OOPS_OOPSHIERARCHY_HPP
#define SHARE_VM_OOPS_OOPSHIERARCHY_HPP

#include "runtime/globals.hpp"
#include "utilities/globalDefinitions.hpp"

// OBJECT hierarchy
// This hierarchy is a representation hierarchy, i.e. if A is a superclass
// of B, A's representation is a prefix of B's representation.
//java对象中oop的偏移量
typedef juint narrowOop; // Offset instead of address for an oop within a java object

// If compressed klass pointers then use narrowKlass.
// 如果压缩klass指针，则使用窄klass
typedef juint  narrowKlass;

typedef void* OopOrNarrowOopStar;
//对象头标记
typedef class   markOopDesc*                markOop;

#ifndef CHECK_UNHANDLED_OOPS

/**
 * Hotspot虚拟机在内部使用两组类来表示Java的类和对象
 *
 * 1. oop(ordinary object pointer,普通对象指针),描述对象的实例信息,是在Java程序运行过程中new对象时创建的。
 *      是HotSpot用来表示Java对象的实例信息的一个体系，即在JVM层面，oop用于表示对象（oop本质上是一个指向内存中对象的起始存储位置的指针）
 * 2. klass,描述java类,是虚拟机内部Java类型结构,Klass是什么时候创建的呢？一般jvm在加载class文件时，会在方法区创建instanceKlass，表示其元数据，包括常量池、字段、方法等。
 *
 * 这里的缩进就体现了Oop继承体系：
 * oopDesc
 * --instanceOopDesc
 * --arrayOopDesc
 * ----objArrayOopDesc
 * ----typeArrayOopDesc
 */
//对象头（包含markOop），包含Kclass
typedef class oopDesc*                            oop;
//Java类实例对象（包含oop）
typedef class   instanceOopDesc*            instanceOop;
//Java数组（包含oop）
typedef class   arrayOopDesc*                    arrayOop;
//Java对象数组（包含arrayOop）
typedef class     objArrayOopDesc*            objArrayOop;
//Java基本类型数组（包含arrayOop）
typedef class     typeArrayOopDesc*            typeArrayOop;

#else

// When CHECK_UNHANDLED_OOPS is defined, an "oop" is a class with a
// carefully chosen set of constructors and conversion operators to go
// to and from the underlying oopDesc pointer type.
//
// Because oop and its subclasses <type>Oop are class types, arbitrary
// conversions are not accepted by the compiler.  Applying a cast to
// an oop will cause the best matched conversion operator to be
// invoked returning the underlying oopDesc* type if appropriate.
// No copy constructors, explicit user conversions or operators of
// numerical type should be defined within the oop class. Most C++
// compilers will issue a compile time error concerning the overloading
// ambiguity between operators of numerical and pointer types. If
// a conversion to or from an oop to a numerical type is needed,
// use the inline template methods, cast_*_oop, defined below.
//
// Converting NULL to oop to Handle implicit is no longer accepted by the
// compiler because there are too many steps in the conversion.  Use Handle()
// instead, which generates less code anyway.

class Thread;
class PromotedObject;


class oop {
  oopDesc* _o;

  void register_oop();
  void unregister_oop();

  // friend class markOop;
public:
  void set_obj(const void* p)         {
    raw_set_obj(p);
    if (CheckUnhandledOops) register_oop();
  }
  void raw_set_obj(const void* p)     { _o = (oopDesc*)p; }

  oop()                               { set_obj(NULL); }
  oop(const oop& o)                   { set_obj(o.obj()); }
  oop(const volatile oop& o)          { set_obj(o.obj()); }
  oop(const void* p)                  { set_obj(p); }
  ~oop()                              {
    if (CheckUnhandledOops) unregister_oop();
  }

  oopDesc* obj()  const volatile      { return _o; }

  // General access
  oopDesc*  operator->() const        { return obj(); }
  bool operator==(const oop o) const  { return obj() == o.obj(); }
  bool operator==(void *p) const      { return obj() == p; }
  bool operator!=(const volatile oop o) const  { return obj() != o.obj(); }
  bool operator!=(void *p) const      { return obj() != p; }

  bool operator<(oop o) const         { return obj() < o.obj(); }
  bool operator>(oop o) const         { return obj() > o.obj(); }
  bool operator<=(oop o) const        { return obj() <= o.obj(); }
  bool operator>=(oop o) const        { return obj() >= o.obj(); }
  bool operator!() const              { return !obj(); }

  // Assignment
  oop& operator=(const oop& o)                            { _o = o.obj(); return *this; }
  volatile oop& operator=(const oop& o) volatile          { _o = o.obj(); return *this; }
  volatile oop& operator=(const volatile oop& o) volatile { _o = o.obj(); return *this; }

  // Explict user conversions
  operator void* () const             { return (void *)obj(); }
#ifndef SOLARIS
  operator void* () const volatile    { return (void *)obj(); }
#endif
  operator HeapWord* () const         { return (HeapWord*)obj(); }
  operator oopDesc* () const volatile { return obj(); }
  operator intptr_t* () const         { return (intptr_t*)obj(); }
  operator PromotedObject* () const   { return (PromotedObject*)obj(); }
  operator markOop () const volatile  { return markOop(obj()); }
  operator address   () const         { return (address)obj(); }

  // from javaCalls.cpp
  operator jobject () const           { return (jobject)obj(); }
  // from javaClasses.cpp
  operator JavaThread* () const       { return (JavaThread*)obj(); }

#ifndef _LP64
  // from jvm.cpp
  operator jlong* () const            { return (jlong*)obj(); }
#endif

  // from parNewGeneration and other things that want to get to the end of
  // an oop for stuff (like ObjArrayKlass.cpp)
  operator oop* () const              { return (oop *)obj(); }
};

#define DEF_OOP(type)                                                      \
   class type##OopDesc;                                                    \
   class type##Oop : public oop {                                          \
     public:                                                               \
       type##Oop() : oop() {}                                              \
       type##Oop(const oop& o) : oop(o) {}                                 \
       type##Oop(const volatile oop& o) : oop(o) {}                        \
       type##Oop(const void* p) : oop(p) {}                                \
       operator type##OopDesc* () const { return (type##OopDesc*)obj(); }  \
       type##OopDesc* operator->() const {                                 \
            return (type##OopDesc*)obj();                                  \
       }                                                                   \
       type##Oop& operator=(const type##Oop& o) {                          \
            oop::operator=(o);                                             \
            return *this;                                                  \
       }                                                                   \
       volatile type##Oop& operator=(const type##Oop& o) volatile {        \
            (void)const_cast<oop&>(oop::operator=(o));                     \
            return *this;                                                  \
       }                                                                   \
       volatile type##Oop& operator=(const volatile type##Oop& o) volatile {\
            (void)const_cast<oop&>(oop::operator=(o));                     \
            return *this;                                                  \
       }                                                                   \
   };

DEF_OOP(instance);
DEF_OOP(array);
DEF_OOP(objArray);
DEF_OOP(typeArray);

#endif // CHECK_UNHANDLED_OOPS

// For CHECK_UNHANDLED_OOPS, it is ambiguous C++ behavior to have the oop
// structure contain explicit user defined conversions of both numerical
// and pointer type. Define inline methods to provide the numerical conversions.
template <class T> inline oop cast_to_oop(T value) {
  return (oop)(CHECK_UNHANDLED_OOPS_ONLY((void *))(value));
}
template <class T> inline T cast_from_oop(oop o) {
  return (T)(CHECK_UNHANDLED_OOPS_ONLY((void*))o);
}

// The metadata hierarchy is separate from the oop hierarchy
// 元数据层次结构与oop层次结构是分开的

//      class MetaspaceObj
//继承自MetaspaceObj，方法信息相关，参数，返回值，注解，异常，code指令
class   ConstMethod;
//继承自MetaspaceObj，包含ConstantPool
class   ConstantPoolCache;
//继承自Metadata，与方法统计优化有关
class   MethodData;
//      class Metadata
//继承自Metadata，组织方法有关信息，包含ConstMethod，MethodData，MethodData，CompiledMethod等
class   Method;
//继承自Metadata，包含Java类字节码常量池信息
class   ConstantPool;
//      class CHeapObj
//继承自CHeapObj，包含方法的编译等信息
class   CompiledICHolder;

// klass层次结构与oop层次结构是分开的。
// The klass hierarchy is separate from the oop hierarchy.
/**
 *  * 这里的缩进就体现了klass继承体系：
 * Klass
 * --InstanceKlass
 * ----InstanceMirrorKlass
 * ----InstanceClassLoaderKlass
 * ----InstanceRefKlass
 * --ArrayKlass
 * ----ObjArrayKlass
 * ----TypeArrayKlass
 */
//继承自 Metadata， 维护着类继承结构的类信息，对应一个ClassLoaderData
class Klass;
//继承自Klass，维护着对应的Java类相关信息（注解，字段，接口，虚表，版本，线程等信息），以及类所处的状 态
class   InstanceKlass;
//继承自InstanceKlass，用于java.lang.Class实例，与反射相关，除了类的普通字段之外，它们还包含类的静态字段
class     InstanceMirrorKlass;
//继承自InstanceKlass，它是为了遍历这个类装入器指向的类装入器的依赖关系。
class     InstanceClassLoaderKlass;
//继承自InstanceKlass，与java/lang/ref/Reference相关
class     InstanceRefKlass;
//继承自Klass，数组相关
class   ArrayKlass;
//继承自ArrayKlass，对象数组
class     ObjArrayKlass;
//继承自ArrayKlass，基本类型数组
class     TypeArrayKlass;

#endif // SHARE_VM_OOPS_OOPSHIERARCHY_HPP
