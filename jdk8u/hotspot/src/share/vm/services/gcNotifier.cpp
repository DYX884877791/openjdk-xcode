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

#include "precompiled.hpp"
#include "classfile/systemDictionary.hpp"
#include "classfile/vmSymbols.hpp"
#include "oops/oop.inline.hpp"
#include "runtime/interfaceSupport.hpp"
#include "runtime/java.hpp"
#include "runtime/javaCalls.hpp"
#include "runtime/mutex.hpp"
#include "runtime/mutexLocker.hpp"
#include "services/gcNotifier.hpp"
#include "services/management.hpp"
#include "services/memoryService.hpp"
#include "memoryManager.hpp"
#include "memory/oopFactory.hpp"

GCNotificationRequest *GCNotifier::first_request = NULL;
GCNotificationRequest *GCNotifier::last_request = NULL;

// pushNotification用于创建一个GC通知消息，将其放入队列中
void GCNotifier::pushNotification(GCMemoryManager *mgr, const char *action, const char *cause) {
  // Make a copy of the last GC statistics
  // GC may occur between now and the creation of the notification
  int num_pools = MemoryService::num_memory_pools();
  // stat is deallocated inside GCNotificationRequest
    //创建一个新的GCStatInfo
  GCStatInfo* stat = new(ResourceObj::C_HEAP, mtGC) GCStatInfo(num_pools);
    //将上一次GC的GCStatInfo复制到这个新的实例中
  mgr->get_last_gc_stat(stat);
    //创建一个新的GC通知
  GCNotificationRequest *request = new GCNotificationRequest(os::javaTimeMillis(),mgr,action,cause,stat);
    //添加到缓存队列中
  addRequest(request);
 }

void GCNotifier::addRequest(GCNotificationRequest *request) {
    //获取Service_lock
  MutexLockerEx ml(Service_lock, Mutex::_no_safepoint_check_flag);
  if(first_request == NULL) {
    first_request = request;
  } else {
    last_request->next = request;
  }
  last_request = request;
  Service_lock->notify_all();
}

GCNotificationRequest *GCNotifier::getRequest() {
  MutexLockerEx ml(Service_lock, Mutex::_no_safepoint_check_flag);
  GCNotificationRequest *request = first_request;
    //更新first_request，即按照添加的顺序依次处理
  if(first_request != NULL) {
    first_request = first_request->next;
  }
  return request;
}

bool GCNotifier::has_event() {
  return first_request != NULL;
}

static Handle getGcInfoBuilder(GCMemoryManager *gcManager,TRAPS) {

  Klass* k = Management::sun_management_GarbageCollectorImpl_klass(CHECK_NH);
  instanceKlassHandle gcMBeanKlass (THREAD, k);

  instanceOop i = gcManager->get_memory_manager_instance(THREAD);
  instanceHandle ih(THREAD, i);

  JavaValue result(T_OBJECT);
  JavaCallArguments args(ih);

  JavaCalls::call_virtual(&result,
                          gcMBeanKlass,
                          vmSymbols::getGcInfoBuilder_name(),
                          vmSymbols::getGcInfoBuilder_signature(),
                          &args,
                          CHECK_NH);
  return Handle(THREAD,(oop)result.get_jobject());
}

static Handle createGcInfo(GCMemoryManager *gcManager, GCStatInfo *gcStatInfo,TRAPS) {

  // Fill the arrays of MemoryUsage objects with before and after GC
  // per pool memory usage

  Klass* mu_klass = Management::java_lang_management_MemoryUsage_klass(CHECK_NH);
  instanceKlassHandle mu_kh(THREAD, mu_klass);

  // The array allocations below should use a handle containing mu_klass
  // as the first allocation could trigger a GC, causing the actual
  // klass oop to move, and leaving mu_klass pointing to the old
  // location.
  objArrayOop bu = oopFactory::new_objArray(mu_kh(), MemoryService::num_memory_pools(), CHECK_NH);
  objArrayHandle usage_before_gc_ah(THREAD, bu);
  objArrayOop au = oopFactory::new_objArray(mu_kh(), MemoryService::num_memory_pools(), CHECK_NH);
  objArrayHandle usage_after_gc_ah(THREAD, au);

  for (int i = 0; i < MemoryService::num_memory_pools(); i++) {
    Handle before_usage = MemoryService::create_MemoryUsage_obj(gcStatInfo->before_gc_usage_for_pool(i), CHECK_NH);
    Handle after_usage;

    MemoryUsage u = gcStatInfo->after_gc_usage_for_pool(i);
    if (u.max_size() == 0 && u.used() > 0) {
      // If max size == 0, this pool is a survivor space.
      // Set max size = -1 since the pools will be swapped after GC.
      MemoryUsage usage(u.init_size(), u.used(), u.committed(), (size_t)-1);
      after_usage = MemoryService::create_MemoryUsage_obj(usage, CHECK_NH);
    } else {
        after_usage = MemoryService::create_MemoryUsage_obj(u, CHECK_NH);
    }
    usage_before_gc_ah->obj_at_put(i, before_usage());
    usage_after_gc_ah->obj_at_put(i, after_usage());
  }

  // Current implementation only has 1 attribute (number of GC threads)
  // The type is 'I'
  objArrayOop extra_args_array = oopFactory::new_objArray(SystemDictionary::Integer_klass(), 1, CHECK_NH);
  objArrayHandle extra_array (THREAD, extra_args_array);
  Klass* itKlass = SystemDictionary::Integer_klass();
  instanceKlassHandle intK(THREAD, itKlass);

  instanceHandle extra_arg_val = intK->allocate_instance_handle(CHECK_NH);

  {
    JavaValue res(T_VOID);
    JavaCallArguments argsInt;
    argsInt.push_oop(extra_arg_val);
    argsInt.push_int(gcManager->num_gc_threads());

    JavaCalls::call_special(&res,
                            intK,
                            vmSymbols::object_initializer_name(),
                            vmSymbols::int_void_signature(),
                            &argsInt,
                            CHECK_NH);
  }
  extra_array->obj_at_put(0,extra_arg_val());

  Klass* gcInfoklass = Management::com_sun_management_GcInfo_klass(CHECK_NH);
  instanceKlassHandle ik(THREAD, gcInfoklass);

  Handle gcInfo_instance = ik->allocate_instance_handle(CHECK_NH);

  JavaValue constructor_result(T_VOID);
  JavaCallArguments constructor_args(16);
  constructor_args.push_oop(gcInfo_instance);
  constructor_args.push_oop(getGcInfoBuilder(gcManager,THREAD));
  constructor_args.push_long(gcStatInfo->gc_index());
  constructor_args.push_long(Management::ticks_to_ms(gcStatInfo->start_time()));
  constructor_args.push_long(Management::ticks_to_ms(gcStatInfo->end_time()));
  constructor_args.push_oop(usage_before_gc_ah);
  constructor_args.push_oop(usage_after_gc_ah);
  constructor_args.push_oop(extra_array);

  JavaCalls::call_special(&constructor_result,
                          ik,
                          vmSymbols::object_initializer_name(),
                          vmSymbols::com_sun_management_GcInfo_constructor_signature(),
                          &constructor_args,
                          CHECK_NH);

  return Handle(gcInfo_instance());
}

void GCNotifier::sendNotification(TRAPS) {
  GCNotifier::sendNotificationInternal(THREAD);
  // Clearing pending exception to avoid premature termination of
  // the service thread
  if (HAS_PENDING_EXCEPTION) {
    CLEAR_PENDING_EXCEPTION;
  }
}

class NotificationMark : public StackObj {
  // This class is used in GCNotifier::sendNotificationInternal to ensure that
  // the GCNotificationRequest object is properly cleaned up, whatever path
  // is used to exit the method.
  GCNotificationRequest* _request;
public:
  NotificationMark(GCNotificationRequest* r) {
    _request = r;
  }
  ~NotificationMark() {
    assert(_request != NULL, "Sanity check");
    delete _request;
  }
};

// sendNotificationInternal用于从队列中获取并发送GC通知消息，由内部线程ServiceThread不断执行
void GCNotifier::sendNotificationInternal(TRAPS) {
  ResourceMark rm(THREAD);
  HandleMark hm(THREAD);
    //获取待发送的一个GC通知消息
  GCNotificationRequest *request = getRequest();
  if (request != NULL) {
    NotificationMark nm(request);
      //创建一个com_sun_management_GcInfo类实例
    Handle objGcInfo = createGcInfo(request->gcManager, request->gcStatInfo, CHECK);

      //初始化其他的方法参数
    Handle objName = java_lang_String::create_from_str(request->gcManager->name(), CHECK);
    Handle objAction = java_lang_String::create_from_str(request->gcAction, CHECK);
    Handle objCause = java_lang_String::create_from_str(request->gcCause, CHECK);

    Klass* k = Management::sun_management_GarbageCollectorImpl_klass(CHECK);
    instanceKlassHandle gc_mbean_klass(THREAD, k);

    instanceOop gc_mbean = request->gcManager->get_memory_manager_instance(THREAD);
    instanceHandle gc_mbean_h(THREAD, gc_mbean);
      //校验gc_mbean必须是sun_management_GarbageCollectorImpl实例
    if (!gc_mbean_h->is_a(k)) {
      THROW_MSG(vmSymbols::java_lang_IllegalArgumentException(),
                "This GCMemoryManager doesn't have a GarbageCollectorMXBean");
    }

    JavaValue result(T_VOID);
    JavaCallArguments args(gc_mbean_h);
      //设置方法参数
    args.push_long(request->timestamp);
    args.push_oop(objName);
    args.push_oop(objAction);
    args.push_oop(objCause);
    args.push_oop(objGcInfo);

      //调用sun_management_GarbageCollectorImpl的createGCNotification方法
    JavaCalls::call_virtual(&result,
                            gc_mbean_klass,
                            vmSymbols::createGCNotification_name(),
                            vmSymbols::createGCNotification_signature(),
                            &args,
                            CHECK);
  }
}

