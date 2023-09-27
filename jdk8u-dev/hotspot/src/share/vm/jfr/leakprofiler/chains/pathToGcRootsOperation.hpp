/*
 * Copyright (c) 2019, Oracle and/or its affiliates. All rights reserved.
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

#ifndef SHARE_JFR_LEAKPROFILER_CHAINS_PATHTOGCROOTSOPERATION_HPP
#define SHARE_JFR_LEAKPROFILER_CHAINS_PATHTOGCROOTSOPERATION_HPP

#include "jfr/leakprofiler/utilities/vmOperation.hpp"

class EdgeStore;
class ObjectSampler;

// Safepoint operation for finding paths to gc roots
// GC Roots基本思路就是通过一系列的称为“GC Roots”的对象作为起始点， 从这些节点开始向下搜索， 搜索所走过的路径称为引用链（ Reference Chain），
// 当一个对象到 GC Roots 没有任何引用链相连（ 用图论的话来 说，就是从GC Roots到这个对象不可达）时，则证明此对象是不可用的。
// 一个对象可以属于多个root，GC Roots有以下几种：
//
//1、在虚拟机栈中引用的对象，例如各个线程被调用的方法堆栈中使用到的参数、局部变量、临时变量等。
//2、在方法区中类静态属性引用的对象，例如java类的引用类型静态变量。
//3、在方法区中常量引用的对象，例如字符串常量池里的引用。
//4、在本地方法栈中JNI引用的对象。
//5、Java虚拟机内部的引用，如基本数据类型对应的class对象，一些常驻的异常对象等，还有类加载器。
//6、所有被同步锁持有的对象。
//7、所有活着的线程 .
//8、反映Java虚拟机内部情况的JMXBean、JVMTI中注册的回调、本地代码缓存等。除了这些固定的GC Roots集合外，根据用户所选用的垃圾收集器以及当前回收的内存区域不同，还可以有其他对象临时性地加入，共同构成完整GC Roots集合。
//
class PathToGcRootsOperation : public OldObjectVMOperation {
 private:
  ObjectSampler* _sampler;
  EdgeStore* const _edge_store;
  const int64_t _cutoff_ticks;
  const bool _emit_all;

 public:
  PathToGcRootsOperation(ObjectSampler* sampler, EdgeStore* edge_store, int64_t cutoff, bool emit_all);
  virtual void doit();
};

#endif // SHARE_JFR_LEAKPROFILER_CHAINS_PATHTOGCROOTSOPERATION_HPP
