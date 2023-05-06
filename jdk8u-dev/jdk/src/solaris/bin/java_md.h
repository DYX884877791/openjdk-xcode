/*
 * Copyright (c) 1998, 2012, Oracle and/or its affiliates. All rights reserved.
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

#ifndef JAVA_MD_H
#define JAVA_MD_H

/*
 * This file contains common defines and includes for Solaris, Linux and MacOSX.
 */
#include <limits.h>
#include <unistd.h>
#include <sys/param.h>
#include "manifest_info.h"
#include "jli_util.h"

#define PATH_SEPARATOR          ':'
#define FILESEP                 "/"
#define FILE_SEPARATOR          '/'
#define IS_FILE_SEPARATOR(c) ((c) == '/')
#ifndef MAXNAMELEN
#define MAXNAMELEN              PATH_MAX
#endif

/*
 * Common function prototypes and sundries.
 */
char *LocateJRE(manifest_info *info);
void ExecJRE(char *jre, char **argv);
int UnsetEnv(char *name);
char *FindExecName(char *program);
const char *SetExecname(char **argv);
const char *GetExecName();
static jboolean GetJVMPath(const char *jrepath, const char *jvmtype,
                           char *jvmpath, jint jvmpathsize, const char * arch,
                           int bitsWanted);
static jboolean GetJREPath(char *path, jint pathsize, const char * arch,
                           jboolean speculative);

/*
 * 当定义了宏MACOSX以后，就包含java_md_macosx.h，否则包含java_md_solinux.h，这两个头文件包含了特定于该操作系统的特殊头文件和变量定义。
 * 类似这种代码在hotspot目录下特别常见，JVM通常将公共的代码放在share目录下，特定于操作系统的代码放在该操作系统的目录下，
 * 然后通过上述两种方式在编译打包时根据构建脚本配置或者宏定义编译打包成特定于操作系统的JDK
 */
#ifdef MACOSX
#include "java_md_macosx.h"
#else  /* !MACOSX */
#include "java_md_solinux.h"
#endif /* MACOSX */

#endif /* JAVA_MD_H */
