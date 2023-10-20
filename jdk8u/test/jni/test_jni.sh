#!/usr/bin/env sh

# 先编译

#  javah命令就是用来给JNI接口生成符合JNI调用规范的头文件的
# 为了避免通过选项指定查找路径和输出路径，可进入到包含目标class文件的目录下执行javah命令，因为javah默认在当前目录下查找目标class，
# 并将结果头文件默认放到当前目录。执行javah -jni -v -o HelloWorld.h jni.HelloWorld生成头文件
