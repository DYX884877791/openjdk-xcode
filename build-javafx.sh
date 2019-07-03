#!/bin/bash

set -x
set -e

# build options
SCRATCH_BUILD_JAVA=true
TARGET_JDK8=true

# define build environment
BUILD_DIR=`pwd`
pushd `dirname $0`
PATCH_DIR=`pwd`
popd
TOOL_DIR=$BUILD_DIR/tools
if ! $TARGET_JDK8 ; then
  TARGET_JDK11=$SCRATCH_BUILD_JAVA
else
  TARGET_JDK11=false
fi

JAVAFX_REPO=https://hg.openjdk.java.net/openjfx/jfx-dev/rt
JAVAFX_BUILD_DIR=`pwd`/javafx

clone_javafx() {
  if [ ! -d $JAVAFX_BUILD_DIR ] ; then
    cd `dirname $JAVAFX_BUILD_DIR`
    hg clone $JAVAFX_REPO "$JAVAFX_BUILD_DIR"
    chmod 755 "$JAVAFX_BUILD_DIR/gradlew"
  fi
}


build_javafx() {
    cd "$JAVAFX_BUILD_DIR"
    ./gradlew
}

clean_javafx() {
    cd "$JAVAFX_BUILD_DIR"
    #./gradlew clean
    rm -fr build
}

build_jdk8() {
   JDK_DIR=$BUILD_DIR/jdk8u-dev/build/macosx-x86_64-normal-server-release/images/j2sdk-image
   if [ ! -f $JDK_DIR/bin/javac ] ; then
       cd $BUILD_DIR
       $PATCH_DIR/build8.sh 
   fi
}
 
build_jdk11() {
   JDK_DIR=$BUILD_DIR/jdk11u-dev/build/macosx-x86_64-normal-server-release/images/jdk
   if [ ! -f $JDK_DIR/bin/javac ] ; then
       cd $BUILD_DIR
       $PATCH_DIR/build11.sh --with-import-modules=$JAVAFX_BUILD_DIR/build/modular-sdk
   fi
}
 
. $PATCH_DIR/tools.sh $TOOL_DIR ant mercurial cmake mvn bootstrap_jdk11

clone_javafx
clean_javafx
build_javafx

if $SCRATCH_BUILD_JAVA ; then
   if $TARGET_JDK8 ; then
      build_jdk8
   else
      build_jdk11
   fi
fi

