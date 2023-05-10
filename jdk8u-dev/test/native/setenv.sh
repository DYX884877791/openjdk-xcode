#!/bin/bash
set -e

cur_dir=`pwd`
build_dir=$cur_dir/build
if test ! -e $build_dir ; then
	mkdir $build_dir
fi
java_dir=$cur_dir/java
java_target_dir=$java_dir/target
if test ! -e $java_target_dir ; then
	mkdir $java_target_dir
fi

path=$(dirname "$cur_dir")
cd $path/..
base_dir=`pwd`


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

openjdk_build_dir=build/$platform_os-x86_64-normal-server-$debug_level/jdk

openjdk_home="$base_dir/$openjdk_build_dir"
if test ! -e $openjdk_home ; then
	echo "$openjdk_home not exist!"
	exit 1
fi


java_main_class=Main
if test -z $java_main_class ; then
	echo "java_main_class is empty!"
	exit 1
fi


export cur_dir
export build_dir
export base_dir
export java_dir
export java_target_dir
export openjdk_home
export java_main_class