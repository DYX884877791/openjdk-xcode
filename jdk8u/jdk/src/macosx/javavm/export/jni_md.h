/*
 * Copyright (c) 1996, 2013, Oracle and/or its affiliates. All rights reserved.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 * This code is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 only, as
 * published by the Free Software Foundation.  Oracle designates this
 * particular file as subject to the "Classpath" exception as provided
 * by Oracle in the LICENSE file that accompanied this code.
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
 */

#ifndef _JAVASOFT_JNI_MD_H_
#define _JAVASOFT_JNI_MD_H_

/**
 * 因为在hotspot/make/bsd/makefiles/gcc.make中设置了CFLAGS += -fvisibility=hidden，
 *
 * 意味着，动态库中的函数默认是隐藏的，除非代码中显示声明为__attribute__((visibility("default"))).
 *
 * 所以这里通过宏定义了符号的可见属性，
 * __attribute__((visibility("default")))
 *
 * https://blog.csdn.net/starstarstone/article/details/7493144?utm_source=tuicool
 */
#define JNIEXPORT     __attribute__((visibility("default")))
#define JNIIMPORT     __attribute__((visibility("default")))
#define JNICALL

typedef int jint;
#ifdef _LP64 /* 64-bit */
typedef long jlong;
#else
typedef long long jlong;
#endif

typedef signed char jbyte;

#endif /* !_JAVASOFT_JNI_MD_H_ */
