#!/bin/bash

set -e

# define JDK and repo
JDKBASE=jdk8u-dev

DEBUG_LEVEL=release
DEBUG_LEVEL=slowdebug
DEBUG_LEVEL=fastdebug
## release, fastdebug, slowdebug

# define build environment
BUILD_DIR=`pwd`
pushd `dirname $0`
SCRIPT_DIR=`pwd`
PATCH_DIR=$SCRIPT_DIR/jdk8u-patch
popd
JDK_DIR=$BUILD_DIR/$JDKBASE

downloadjdksrc() {
	if [ ! -d "$JDK_DIR" ]; then
		pushd "$BUILD_DIR"
		hg clone http://hg.openjdk.java.net/jdk8u/$JDKBASE "$JDK_DIR"
		cd "$JDK_DIR"
		chmod 755 get_source.sh configure
		./get_source.sh
		popd
	else 
		pushd "$JDK_DIR"
		hg pull -u 
		for a in corba hotspot jaxp jaxws jdk langtools nashorn ; do
			pushd $a
			hg pull -u
			popd
		done
		popd
	fi
}

patchjdk() {
	cd $JDK_DIR
	patch -p1 <$PATCH_DIR/mac-jdk8u.patch
	for a in hotspot jdk ; do 
		cd $JDK_DIR/$a
		for b in $PATCH_DIR/mac-jdk8u-$a*.patch ; do 
			 patch -p1 <$b
		done
	done
}

revertjdk() {
	cd $JDK_DIR
	hg revert .
	for a in hotspot jdk ; do 
		cd $JDK_DIR/$a
		hg revert .
	done
}

configurejdk() {
	if [ $XCODE_VERSION -ge 11 ] ; then
		DISABLE_PCH=--disable-precompiled-headers
	fi
	pushd $JDK_DIR
	chmod 755 ./configure
	unset JAVA_HOME
	BOOT_JDK=$TOOL_DIR/jdk8u/Contents/Home
	./configure --with-toolchain-type=clang \
            --with-xcode-path=$XCODE_APP \
            --includedir=$XCODE_DEVELOPER_PREFIX/Toolchains/XcodeDefault.xctoolchain/usr/include \
            --with-debug-level=$DEBUG_LEVEL \
            --with-boot-jdk=$BOOT_JDK \
            --with-freetype-include=$TOOL_DIR/freetype/include \
            --with-freetype-lib=$TOOL_DIR/freetype/objs/.libs $DISABLE_PCH
	popd
}

buildjdk() {
	pushd $JDK_DIR
	make images COMPILER_WARNINGS_FATAL=false CONF=macosx-x86_64-normal-server-$DEBUG_LEVEL
	popd
}

. $SCRIPT_DIR/tools.sh $BUILD_DIR/tools freetype autoconf mercurial bootstrap_jdk8 webrev
set -x
downloadjdksrc
#revertjdk
patchjdk
configurejdk
buildjdk

