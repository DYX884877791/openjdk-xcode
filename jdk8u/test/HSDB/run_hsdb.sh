#!/bin/bash
set -e

. ./set_java_env.sh

pid=$(ps -ef | grep java | grep "${java_main_class}" | grep -v grep | awk '{print $2}')
if test -n $pid; then
  echo "${java_main_class} running on pid ${pid}"
fi

sudo ${openjdk_home}/bin/java -cp ${openjdk_home}/lib/sa-jdi.jar sun.jvm.hotspot.HSDB ${pid}