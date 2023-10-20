#!/bin/bash

set -e

BUILD_LOG="LOG=debug"
BUILD_MODE=normal
TEST_JDK=false
BUILD_JAVAFX=false
CUR_DIR=`cd $(dirname $0) && pwd`
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
	BUILD_DIR=$CUR_DIR
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
SCRIPT_DIR="$CUR_DIR"
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

configurejdk() {
	progress "configure jdk"
	#if [ $XCODE_VERSION -ge 11 ] ; then
	#	DISABLE_PCH=--disable-precompiled-headers
	#fi
	pushd "$JDK_DIR"
	chmod 755 ./configure
	unset DARWIN_CONFIG
	if $IS_DARWIN ; then
		#BOOT_JDK="$TOOL_DIR/jdk8u/Contents/Home"
		DARWIN_CONFIG="MAKE=/usr/bin/make \
			--with-toolchain-type=clang \
            --with-xcode-path="$XCODE_APP" \
            --includedir="$XCODE_DEVELOPER_PREFIX/Toolchains/XcodeDefault.xctoolchain/usr/include" \
            --with-boot-jdk="$BOOT_JDK""
	fi
	./configure $DARWIN_CONFIG $BUILD_VERSION_CONFIG \
            --with-debug-level=$DEBUG_LEVEL \
            --with-conf-name=$JDK_CONF \
            --disable-zip-debug-info \
            --with-target-bits=64 \
            --with-jvm-variants=server \
	     	--with-native-debug-symbols=internal \
            --with-jtreg="$BUILD_DIR/tools/jtreg" \
            --with-freetype-include="$TOOL_DIR/freetype/include" \
            --with-freetype-lib=$TOOL_DIR/freetype/objs/.libs $DISABLE_PCH
	popd
}

buildjdk() {
	progress "build jdk"
	pushd "$JDK_DIR"
	# compiledb make images $BUILD_LOG COMPILER_WARNINGS_FATAL=false CONF=$JDK_CONF
	make images $BUILD_LOG COMPILER_WARNINGS_FATAL=false CONF=$JDK_CONF
	if $IS_DARWIN ; then
        # seems the path handling has changed; use rpath instead of hardcoded path
        find  "$JDK_DIR/build/$JDK_CONF/images" -type f -name libfontmanager.dylib -exec install_name_tool -change /usr/local/lib/libfreetype.6.dylib @rpath/libfreetype.dylib.6 {} \; -print
    fi
	popd
}


progress() {
	echo $1
}

#### build the world
progress "building in $JDK_DIR"


JDK_IMAGE_DIR="$JDK_DIR/build/$JDK_CONF/images/j2sdk-image"

# must always download tools to set paths properly

set -x

echo "RECONFIGURE_BUILD--->$RECONFIGURE_BUILD"
echo "CLEAN_BUILD--->$CLEAN_BUILD"

if [ true -o $RECONFIGURE_BUILD -o $CLEAN_BUILD ] ; then
	echo "configure jdk..."
	configurejdk
fi

echo "build jdk..."
buildjdk

progress "create distribution zip"

if $BUILD_JAVAFX ; then
	WITH_JAVAFX_STR=-javafx
else
	WITH_JAVAFX_STR=
fi

ZIP_NAME="$BUILD_DIR/jdk8u$BUILD_MODE$WITH_JAVAFX_STR.zip"

if $BUILD_JAVAFX ; then
	progress "call build_javafx script"
	"$SCRIPT_DIR/build-javafx.sh" "$JDK_IMAGE_DIR" "$ZIP_NAME"
fi

echo "BUILD JDK SUCCESS!!!"


