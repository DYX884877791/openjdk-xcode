#!/usr/bin/env bash
CMAKE_OUTPUT_DIR=cmake-build-debug
if [ "$#" -eq 1 -a "$1" == "clean" ];then
    echo "即将清理cmake构建目录..."
    rm -rf ${CMAKE_OUTPUT_DIR}/*
    exit 1;
fi
# 该CMAKE脚本需要依赖已编译好的openjdk image镜像...
cmake . -B ${CMAKE_OUTPUT_DIR}

cd ${CMAKE_OUTPUT_DIR}
make V=1 -j8 gamma