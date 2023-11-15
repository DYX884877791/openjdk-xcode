#!/usr/bin/env sh
set -e
CUR_DIR=`cd $(dirname $0) && pwd`
. ${CUR_DIR}/test/set_base_env.sh
rm -f ${openjdk_build_dir}/hotspot/bsd_amd64_compiler2/generated/dependencies/precompiled.hpp.pch.d && echo '删除precompiled.hpp.pch.d成功' || echo '删除precompiled.hpp.pch.d失败'
rm -f ${openjdk_build_dir}/hotspot/bsd_amd64_compiler2/debug/precompiled.hpp.pch && echo '删除precompiled.hpp.pch成功' || echo '删除precompiled.hpp.pch失败'