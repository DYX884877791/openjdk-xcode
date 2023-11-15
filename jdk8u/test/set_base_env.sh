#!/bin/bash
set -e

# 如果使用dirname $0的话，$0保存了被执行脚本的程序名称。注意，它保存的是以二进制方式执行的脚本名而非以source方式加载的脚本名称。
# 例如，执行a.sh时，a.sh中的$0的值是a.sh，如果a.sh执行b.sh，b.sh中的$0的值是b.sh，如果a.sh中source b.sh，则b.sh中的$0的值为a.sh。
#
# 除了$0，bash还提供了一个数组变量BASH_SOURCE，该数组保存了bash的SOURCE调用层次。
# https://www.junmajinlong.com/shell/bash_source/
# 获取当前脚本的文件名字
cur_file=$(basename ${BASH_SOURCE[0]})
# 获取当前脚本所在的相对路径
cur_file_dir=$(dirname ${BASH_SOURCE[0]})
# 获取当前脚本所在的绝对路径
cur_dir=$(cd ${cur_file_dir} && pwd)
# 获取jdk项目的绝对路径
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

echo "openjdk_home is $openjdk_home"

export openjdk_build_dir
export openjdk_home
export platform_os
export base_dir