#!/bin/bash

set -e

BUILD_LOG="LOG=debug"
BUILD_MODE=dev
TEST_JDK=false
BUILD_JAVAFX=false

set_os() {
	IS_LINUX=false
	IS_DARWIN=false
	if [ "`uname`" = "Linux" ] ; then
		IS_LINUX=true
	fi
	IS_DARWIN=false
	if [ "`uname`" = "Darwin" ] ; then
		IS_DARWIN=true
	fi
}

set_os

BOOT_JDK_MACOS="/Library/Java/JavaVirtualMachines/jdk1.8.0_291.jdk/Contents/Home"
BOOT_JDK_LINUX="/home/dengyouxu/Development/Software/jdk-8u351-linux-x64/jdk1.8.0_351"

if $IS_LINUX ; then
    BOOT_JDK=$BOOT_JDK_LINUX
fi

if $IS_DARWIN ; then
    BOOT_JDK=$BOOT_JDK_MACOS
fi

# set to true to alway reconfigure the build (recommended that CLEAN_BUILD also be set true)
RECONFIGURE_BUILD=true

# set to true to always clean the build
CLEAN_BUILD=false

# set to true to always revert patches
REVERT_PATCHES=false

# aarch64 does not work yet - only x86_64
export BUILD_TARGET_ARCH=x86_64

# if we're on a macos m1 machine, we can run in x86_64 or native aarch64/arm64 mode.
# currently the build script only supports building x86_64 binaries and only on x86_64 hosts.
if $IS_DARWIN ; then
	if [ "`uname -m`" = "arm64" ] ; then
		echo "building on aarch64 - restarting in x86_64 mode"
		arch -x86_64 "$0" $@
		exit $?
	fi
fi

# check the boot jdk path is exist?
if [ -z $BOOT_JDK ] ; then
	echo "the boot jdk path is incorrect,please check it"
	exit $?
elif [ ! -d $BOOT_JDK ] ; then
	echo "the boot jdk path not exist,please check it"
	exit $?
else 
	echo "the boot jdk path is $BOOT_JDK"
fi


if [ "X$BUILD_MODE" == "X" ] ; then
	# normal, dev, shenandoah, [jvmci, eventually]
	BUILD_MODE=normal
fi

## release, fastdebug, slowdebug
if [ "X$DEBUG_LEVEL" == "X" ] ; then
	DEBUG_LEVEL=slowdebug
fi

## build directory
if [ "X$BUILD_DIR" == "X" ] ; then
	BUILD_DIR=`pwd`
fi

## add javafx to build at end
if [ "X$BUILD_JAVAFX" == "X" ] ; then
	BUILD_JAVAFX=false
fi
BUILD_SCENEBUILDER=$BUILD_JAVAFX

### no need to change anything below this line unless something went wrong



if [ "$BUILD_MODE" == "normal" ] ; then
	JDK_BASE=jdk8u
	BUILD_MODE=dev
	JDK_REPO=https://github.com/openjdk/$JDK_BASE.git
	JDK_DIR="$BUILD_DIR/$JDK_BASE"
elif [ "$BUILD_MODE" == "dev" ] ; then
	JDK_BASE=jdk8u-dev
	BUILD_MODE=dev
	JDK_REPO=https://github.com/openjdk/$JDK_BASE.git
	JDK_DIR="$BUILD_DIR/$JDK_BASE"
elif [ "$BUILD_MODE" == "shenandoah" ] ; then
	JDK_BASE=jdk8u
	BUILD_MODE=dev
	JDK_REPO=https://github.com/openjdk/shenandoah-jdk8u-dev
	JDK_DIR="$BUILD_DIR/$JDK_BASE-shenandoah"
fi

# define build environment
pushd `dirname $0`
SCRIPT_DIR=`pwd`
PATCH_DIR="$SCRIPT_DIR/jdk8u-patch"
TOOL_DIR="$BUILD_DIR/tools"
TMP_DIR="$TOOL_DIR/tmp"
popd

if $IS_DARWIN ; then
	JDK_CONF=macosx-${BUILD_TARGET_ARCH}-normal-server-$DEBUG_LEVEL
else
	JDK_CONF=linux-${BUILD_TARGET_ARCH}-normal-server-$DEBUG_LEVEL
fi

### JDK


buildjdk() {
	progress "build jdk"
	pushd "$JDK_DIR"
	compiledb make jdk $BUILD_LOG COMPILER_WARNINGS_FATAL=false CONF=$JDK_CONF
	popd
}


progress() {
	echo $1
}

#### build the world
progress "building in $JDK_DIR"

set_os



JDK_IMAGE_DIR="$JDK_DIR/build/$JDK_CONF/images/j2sdk-image"

# must always download tools to set paths properly

set -x

echo "RECONFIGURE_BUILD--->$RECONFIGURE_BUILD"
echo "CLEAN_BUILD--->$CLEAN_BUILD"

buildjdk

echo "BUILD JDK SUCCESS!!!"


