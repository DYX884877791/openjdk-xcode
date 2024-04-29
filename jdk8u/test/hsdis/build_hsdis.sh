#!/bin/sh
. ../set_test_env.sh
hsdis_dir=${base_dir}/hotspot/src/share/tools/hsdis
if test ! -e build/${platform_os}-amd64/hsdis-amd64.dylib; then
  make BINUTILS=${hsdis_dir}/../binutils/binutils-2.30 ARCH=amd64
fi
cp build/macosx-amd64/hsdis-amd64.dylib ${openjdk_home}/lib/server/

SingleThreadExecution
Immutable
GuardedSupension
Balking
