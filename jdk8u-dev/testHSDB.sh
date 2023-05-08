#!/usr/bin/env bash
export _JAVA_LAUNCHER_DEBUG=1
cd `pwd`/build/macosx-x86_64-normal-server-slowdebug/jdk/bin || echo "error"
./java -version

