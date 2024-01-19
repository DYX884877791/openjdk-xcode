#!/usr/bin/env sh
set -e

CUR_DIR=`cd $(dirname $0) && pwd`
. ${CUR_DIR}/set_java_env.sh

# 先编译SetCpu.java文件
${openjdk_home}/bin/javac ${java_dir}/${java_main_class}.java -d ${java_target_dir} -encoding utf-8 && echo "编译Java文件成功,即将执行该测试用例"

# 再跟小
${openjdk_home}/bin/javah -jni -v -o SetCpu.h SetCpu