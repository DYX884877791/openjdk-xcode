#!/bin/bash
set -ex

. ../set_base_env.sh
# 得到shell脚本文件所在目录的完整路径（绝对路径） $( dirname “$0“ ) 不推荐使用--> 是有缺陷的，例如：脚本A source 了另一个⽬目录下的脚本B, 然后脚本B尝试使⽤用此法获取路路径时得到的是A的路路径
# ${BASH_SOURCE[0]} 推荐使用 --> BASH_SOURCE[0] - 等价于 BASH_SOURCE ,取得当前执行的 shell 文件所在的路径及文件名
# $0 获取的是脚本文件名，而 ${BASH_SOURCE[0]} 获取的是被引用的文件名。
# 如果 shell 脚本是被其他脚本包含的，那么 $0 会是包含它的脚本的名字，而 "${BASH_SOURCE[0]}" 将是被包含的脚本的名字。
# 获取当前脚本的文件名字
cur_file=$(basename ${BASH_SOURCE[0]})
# 获取当前脚本所在的相对路径
cur_file_dir=$(dirname ${BASH_SOURCE[0]})
# 获取当前脚本所在的绝对路径
cur_dir=$(cd ${cur_file_dir} && pwd)

build_dir=$cur_dir/build
if test ! -e $build_dir ; then
	mkdir $build_dir
fi

# 这里使用上级sample目录来测试
java_dir=$cur_dir/../sample/java
java_target_dir=$java_dir/target
if test ! -e $java_target_dir ; then
	mkdir $java_target_dir
fi

java_main_class=Main
if test -z $java_main_class ; then
	echo "java_main_class is empty!"
	exit 1
fi


export cur_dir
export build_dir
export java_dir
export java_target_dir
export java_main_class

