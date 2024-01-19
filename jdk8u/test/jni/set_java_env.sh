#!/bin/bash
set -e
CUR_DIR=`cd $(dirname $0) && pwd`
. ${CUR_DIR}/../set_base_env.sh
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

java_dir=$cur_dir/java
java_target_dir=$cur_dir/target
if test ! -e $java_target_dir ; then
	mkdir -p $java_target_dir
fi


export cur_dir
export java_dir
export java_target_dir


