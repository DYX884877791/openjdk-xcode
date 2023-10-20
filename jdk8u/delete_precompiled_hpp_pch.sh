#!/usr/bin/env sh
CUR_DIR=`cd $(dirname $0) && pwd`
. ${CUR_DIR}/test/set_base_env.sh
rm -f ${openjdk_build_dir}/hotspot/bsd_amd64_compiler2/generated/dependencies/precompiled.hpp.pch.d
