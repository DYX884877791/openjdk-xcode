#!/bin/bash
#
# Copyright (c) 2012, Oracle and/or its affiliates. All rights reserved.
# DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
#
# This code is free software; you can redistribute it and/or modify it
# under the terms of the GNU General Public License version 2 only, as
# published by the Free Software Foundation.
#
# This code is distributed in the hope that it will be useful, but WITHOUT
# ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
# FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
# version 2 for more details (a copy is included in the LICENSE file that
# accompanied this code).
#
# You should have received a copy of the GNU General Public License version
# 2 along with this work; if not, write to the Free Software Foundation,
# Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA.
#
# Please contact Oracle, 500 Oracle Parkway, Redwood Shores, CA 94065 USA
# or visit www.oracle.com if you need additional information or have any
# questions.
#

# Usage: ./logger.sh theloggfile acommand arg1 arg2 
#
# Execute acommand with args, in such a way that
# both stdout and stderr from acommand are appended to 
# theloggfile.
#
# Preserve stdout and stderr, so that the stdout
# from logger.sh is the same from acommand and equally
# for stderr.
#
# Propagate the result code from acommand so that
# ./logger.sh exits with the same result code.

# Create a temporary directory to store the result code from
# the wrapped command.
# mktemp [OPTION] [TEMPLATE]
# mktemp命令用于安全地创建一个临时文件或目录，并输出其名称，TEMPLATE在最后一个组件中必须至少包含3个连续的X，如果未指定TEMPLATE，则使用tmp.XXXXXXXXXX作为名称在当前目录下创建相应的临时文件，X为生成的随机数，尾部的X将替换为当前进程号和随机字母的组合
# mktemp命令的底层实现：使用了一个叫做mkstemp的C函数。mkstemp函数是一个系统调用，用于创建一个唯一的临时文件。
RCDIR=`mktemp -dt jdk-build-logger.tmp.XXXXXX` || exit $?
# trap：shell中的信号捕获，https://blog.csdn.net/MyySophia/article/details/122194021，https://www.cnblogs.com/cheyunhua/p/14637004.html
trap "rm -rf \"$RCDIR\"" EXIT
LOGFILE=$1
# tee：从标准输入读入并写往标准输出和文件，后接-a/--append表示追加到给出的文件，而不是覆盖
shift
(exec 3>&1 ; ("$@" 2>&1 1>&3; echo $? > "$RCDIR/rc") | tee -a $LOGFILE 1>&2 ; exec 3>&-) | tee -a $LOGFILE
exit `cat "$RCDIR/rc"`
