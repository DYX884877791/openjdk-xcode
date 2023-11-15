/*
 * Copyright (c) 2011, 2013, Oracle and/or its affiliates. All rights reserved.
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

#include "classfile/classLoaderData.hpp"
#include "classfile/javaClasses.hpp"

inline ClassLoaderData* ClassLoaderData::class_loader_data_or_null(oop loader) {
  if (loader == NULL) {
    return ClassLoaderData::the_null_class_loader_data();
  }
  return java_lang_ClassLoader::loader_data(loader);
}

inline ClassLoaderData* ClassLoaderData::class_loader_data(oop loader) {
  ClassLoaderData* loader_data = class_loader_data_or_null(loader);
  assert(loader_data != NULL, "Must be");
  return loader_data;
}


//  find_or_create用于查找某个java/lang/ClassLoader实例对应的ClassLoaderData，如果不存在则为该实例创建一个新的ClassLoaderData实例并添加到ClassLoaderDataGraph管理的ClassLoaderData链表中。
//  注意ClassLoaderData指针的保存位置比较特殊，不是在ClassLoader实例的内存中，而是内存外，内存上方的8字节处，为什么这8字节在没有保存ClassLoaderData指针时是NULL了？因为Java对象创建的时候会保证对象间有8字节的空隙。
inline ClassLoaderData *ClassLoaderDataGraph::find_or_create(Handle loader, TRAPS) {
    //校验loader必须是一个oop
  guarantee(loader() != NULL && loader()->is_oop(), "Loader must be oop");
  // Gets the class loader data out of the java/lang/ClassLoader object, if non-null
  // it's already in the loader_data, so no need to add
    //根据java/lang/ClassLoader对象的地址获取ClassLoaderData的指针，如果不为空说明这个ClassLoader对象
    //已经添加到ClassLoaderDataGraph中了，否则需要添加
  ClassLoaderData* loader_data= java_lang_ClassLoader::loader_data(loader());
  if (loader_data) {
     return loader_data;
  }
  return ClassLoaderDataGraph::add(loader, false, THREAD);
}
