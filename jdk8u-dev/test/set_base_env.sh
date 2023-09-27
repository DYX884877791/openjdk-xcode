#!/bin/bash
set -ex

# 获取当前脚本的文件名字
cur_file=$(basename ${BASH_SOURCE[0]})
# 获取当前脚本所在的相对路径
cur_file_dir=$(dirname ${BASH_SOURCE[0]})
# 获取当前脚本所在的绝对路径
cur_dir=$(cd ${cur_file_dir} && pwd)
# 获取项目(jdk8u-dev)的绝对路径
base_dir=$(cd $(dirname ${cur_dir}) && pwd)

platform_os=
set_os() {
    if [ "`uname`" = "Linux" ] ; then
        platform_os=linux
    fi
    if [ "`uname`" = "Darwin" ] ; then
        platform_os=macosx
    fi
}

set_os

debug_level=slowdebug
openjdk_build_dir=${base_dir}/build/${platform_os}-x86_64-normal-server-${debug_level}
openjdk_home=${openjdk_build_dir}/images/j2sdk-image
if test ! -e $openjdk_home ; then
	echo "$openjdk_home not exist!"
	exit 1
fi

echo "$openjdk_home"

export openjdk_build_dir
export openjdk_home
export platform_os
export base_dir