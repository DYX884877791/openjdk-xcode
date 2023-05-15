#!/bin/bash
set -e
# 在一个shell脚本(如test_call_other_shell.sh)中调用另外一个shell脚本(如parameter_usage.sh)，这里总结几种可行的方法，这些方法在linux上和windows上(通过Git Bash)均适用：
# 1.通过source: 运行在相同的进程，在test_call_other_shell.sh中调用parameter_usage.sh后，parameter_usage.sh中的变量和函数在test_call_other_shell.sh中可直接使用
# 2.通过/bin/bash: 运行在不同的进程
# 3.通过sh: 运行在不同的进程
# 4.通过.: 运行在相同的进程，在test_call_other_shell.sh中调用parameter_usage.sh后，parameter_usage.sh中的变量和函数在test_call_other_shell.sh中可直接使用

. ./setenv.sh

cd $cur_dir
make compileJava

echo "编译Java文件成功,即将执行该测试用例"

cd $java_target_dir

#gdb --args
$openjdk_home/bin/java $java_main_class


