#!/bin/bash
set -ex
# 在一个shell脚本(如test_call_other_shell.sh)中调用另外一个shell脚本(如parameter_usage.sh)，这里总结几种可行的方法，这些方法在linux上和windows上(通过Git Bash)均适用：
# 1.通过source: 运行在相同的进程，在test_call_other_shell.sh中调用parameter_usage.sh后，parameter_usage.sh中的变量和函数在test_call_other_shell.sh中可直接使用
# 2.通过/bin/bash: 运行在不同的进程
# 3.通过sh: 运行在不同的进程
# 4.通过.: 运行在相同的进程，在test_call_other_shell.sh中调用parameter_usage.sh后，parameter_usage.sh中的变量和函数在test_call_other_shell.sh中可直接使用

. ./set_java_env.sh

find ${java_dir} -name "*.java" > ${cur_dir}/sources.list

${openjdk_home}/bin/javac -encoding utf-8 -cp . -d ${java_target_dir} -g @${cur_dir}/sources.list
echo 'Compile Java File Success!!!'

cd $java_target_dir
#gdb --args
$openjdk_home/bin/java -Dslog.level=ALL ${java_main_class}


