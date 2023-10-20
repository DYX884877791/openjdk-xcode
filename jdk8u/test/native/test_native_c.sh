#!/bin/bash
set -e
# 在一个shell脚本(如test_call_other_shell.sh)中调用另外一个shell脚本(如parameter_usage.sh)，这里总结几种可行的方法，这些方法在linux上和windows上(通过Git Bash)均适用：
# 1.通过source: 运行在相同的进程，在test_call_other_shell.sh中调用parameter_usage.sh后，parameter_usage.sh中的变量和函数在test_call_other_shell.sh中可直接使用
# 2.通过/bin/bash: 运行在不同的进程
# 3.通过sh: 运行在不同的进程
# 4.通过.: 运行在相同的进程，在test_call_other_shell.sh中调用parameter_usage.sh后，parameter_usage.sh中的变量和函数在test_call_other_shell.sh中可直接使用

. ./set_native_env.sh

$base_dir/common/bin/logger.sh $build_dir/build.log make jvm
	# -isysroot "/Applications/Xcode.app/Contents/Developer/Platforms/MacOSX.platform/Developer/SDKs/MacOSX13.1.sdk" \
	# -framework CoreFoundation
	# -I/Users/dengyouxu/Developer/Software/openjdk8u/jdk8u-xcode10/jdk8u-dev/jdk/src/share/bin \
	# -I/Users/dengyouxu/Developer/Software/openjdk8u/jdk8u-xcode10/jdk8u-dev/jdk/src/solaris/bin \
	# -I/Users/dengyouxu/Developer/Software/openjdk8u/jdk8u-xcode10/jdk8u-dev/jdk/src/macosx/bin   \
	# -I/Users/dengyouxu/Developer/Software/openjdk8u/jdk8u-xcode10/jdk8u-dev/jdk/src/share/javavm/export \
	# -I/Users/dengyouxu/Developer/Software/openjdk8u/jdk8u-xcode10/jdk8u-dev/jdk/src/macosx/javavm/export \
	# -I/Users/dengyouxu/Developer/Software/openjdk8u/jdk8u-xcode10/jdk8u-dev/jdk/src/share/native/common \
	# -I/Users/dengyouxu/Developer/Software/openjdk8u/jdk8u-xcode10/jdk8u-dev/jdk/src/solaris/native/common \
	# -iframework "/Applications/Xcode.app/Contents/Developer/Platforms/MacOSX.platform/Developer/SDKs/MacOSX13.1.sdk/System/Library/Frameworks" \

echo "编译测试用例代码成功,即将执行该测试用例"

cd $build_dir
./jvm
