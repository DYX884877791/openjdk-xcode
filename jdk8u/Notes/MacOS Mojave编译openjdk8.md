---
created: 2021-12-05T00:59:26 (UTC +08:00)
tags: []
source: https://www.jianshu.com/p/0e6300eddcf4
author: 
---

# MacOS Mojave编译openjdk8 - 简书
---
[![](https://upload.jianshu.io/users/upload_avatars/7426929/078db31a-f822-4c13-9725-4b6f7b3d611f.jpg?imageMogr2/auto-orient/strip|imageView2/1/w/96/h/96/format/webp)](https://www.jianshu.com/u/c8437f8273f2)

2020.02.26 14:57:13字数 919阅读 1,154

-   系统版本  
    MacOS Mojave 10.14.5
-   Xcode版本  
    Version11.3(11C29)

### 下载openJDK8源码

```
brew install mercurial
hg clone http://hg.openjdk.java.net/jdk8u/jdk8u-dev/
cd jdk8u-dev
bash ./get_source.sh
```

如果克隆失败，可以到`http://hg.openjdk.java.net/jdk8u/jdk8u-dev/`上面下载具体的模块zip或者tar.gz包进行解压；

下载下来后有以下几个的模块

模块

Contains

备注

.(root)

通用的配置和makefile文件

hotspot

构建 OpenJDK 虚拟机的源码和make文件

JVM源码

langtools

OpenJDK javac和语言工具的源码

jdk中提供的开发工具，比如javac,javap,jvisual以及一些国际化实现

jdk

构建openjdk运行时库和misc文件的源码和make文件

jdk源码，编译后生成的则是Java运行时库和一些杂项库lib

jaxp

OpenJDK JAXP源码

jaxws

OpenJDK JAX-WS功能的源码

corba

OpenJDK Corba功能的源码

nashorn

OpenJDK JavaScript的源码

然后是配置环境变量

```
# 设定语言选项，必须设置
export LANG=C
# Mac平台，C编译器不再是GCC，而是clang
export CC=clang
export CXX=clang++
export CXXFLAGS=-stdlib=libc++
# 是否使用clang，如果使用的是GCC编译，该选项应该设置为false
export USE_CLANG=true
# 跳过clang的一些严格的语法检查，不然会将N多的警告作为Error
export COMPILER_WARNINGS_FATAL=false
# 链接时使用的参数
export LFLAGS='-Xlinker -lstdc++'
# 使用64位数据模型
export LP64=1
# 告诉编译平台是64位，不然会按照32位来编译
export ARCH_DATA_MODEL=64
# 允许自动下载依赖
export ALLOW_DOWNLOADS=true
# 并行编译的线程数，编译时长，为了不影响其他工作，可以选择2
export HOTSPOT_BUILD_JOBS=2
export PARALLEL_COMPILE_JOBS=2 #ALT_PARALLEL_COMPILE_JOBS=2
# 是否跳过与先前版本的比较
export SKIP_COMPARE_IMAGES=true
# 是否使用预编译头文件，加快编译速度
export USE_PRECOMPILED_HEADER=true
# 是否使用增量编译
export INCREMENTAL_BUILD=true
# 编译内容
export BUILD_LANGTOOL=true
export BUILD_JAXP=true
export BUILD_JAXWS=true
export BUILD_CORBA=true
export BUILD_HOTSPOT=true
export BUILD_JDK=true
# 编译版本
export SKIP_DEBUG_BUILD=true
export SKIP_FASTDEBUG_BULID=false
export DEBUG_NAME=debug
# 避开javaws和浏览器Java插件之类部分的build
export BUILD_DEPLOY=false
export BUILD_INSTALL=false

# 最后需要干掉这两个环境变量（如果你配置过），不然会发生诡异的事件
unset JAVA_HOME
unset CLASSPATH
```

### 编译JDK

##### 准备的第三方

-   freetype ：`brew install freetype`
-   Xcode： Xcode11
-   XQuartz：[下载地址](https://links.jianshu.com/go?to=https%3A%2F%2Fwww.xquartz.org%2F)

#### configure

```

bash ./configure  --with-freetype-include=/usr/local/include/freetype2 --with-freetype-lib=/usr/local/lib/


bash ./configure --enable-debug --with-target-bits=64 --with-freetype-include=/usr/local/include/freetype2 --with-freetype-lib=/usr/local/lib --with-boot-jdk=/Library/Java/JavaVirtualMachines/jdk1.8.0_201.jdk/Contents/Home CXX=clang++ 
```

成功之后会有类似于下面的输出

```
A new configuration has been successfully created in
/Users/y2ss/Desktop/java-study/jdk8u-dev/build/macosx-x86_64-normal-server-release
using configure arguments '--with-freetype-include=/usr/local/include/freetype2 --with-freetype-lib=/usr/local/lib/'.

Configuration summary:
* Debug level:    release
* JDK variant:    normal
* JVM variants:   server
* OpenJDK target: OS: macosx, CPU architecture: x86, address length: 64

Tools summary:
* Boot JDK:       java version "1.8.0_201" Java(TM) SE Runtime Environment (build 1.8.0_201-b09) Java HotSpot(TM) 64-Bit Server VM (build 25.201-b09, mixed mode)  (at /Library/Java/JavaVirtualMachines/jdk1.8.0_201.jdk/Contents/Home)
* Toolchain:      gcc (GNU Compiler Collection)
* C Compiler:     Version Apple clang version 11.0.0 (clang-1100.0.33.16) Target: x86_64-apple-darwin18.6.0 Thread model: posix InstalledDir: /Applications/Xcode.app/Contents/Developer/Toolchains/XcodeDefault.xctoolchain/usr/bin (at /usr/bin/clang)
* C++ Compiler:   Version Apple clang version 11.0.0 (clang-1100.0.33.16) Target: x86_64-apple-darwin18.6.0 Thread model: posix InstalledDir: /Applications/Xcode.app/Contents/Developer/Toolchains/XcodeDefault.xctoolchain/usr/bin (at /usr/bin/clang++)

Build performance summary:
* Cores to use:   2
* Memory limit:   8192 MB
```

可能会遇到的错误

##### 1\. configure: error: Xcode 4 is required to build JDK 8, the version found was 11.1

解决办法：`vi common/autoconf/generated-configure.sh`，注释掉下面代码

```
# Fail-fast: verify we're building on Xcode 4, we cannot build with Xcode 5 or later
XCODE_VERSION=`$XCODEBUILD -version | grep '^Xcode ' | sed 's/Xcode //'`
XC_VERSION_PARTS=( ${XCODE_VERSION//./ } )
if test ! "${XC_VERSION_PARTS[0]}" = "4"; then
   as_fn_error $? "Xcode 4 is required to build JDK 8, the version found was $XCODE_VERSION. Use --with-xcode-path to specify the location of Xcode 4 or make Xcode 4 active by using xcode-select." "$LINENO" 5
fi
```

##### 2\. configure: error: GCC compiler is required. Try setting --with-tools-dir.

解决办法：`vi common/autoconf/generated-configure.sh`，注释掉所有以下代码

```
if test $? -ne 0; then
      { $as_echo "$as_me:${as_lineno-$LINENO}: The $COMPILER_NAME compiler (located as $COMPILER) does not seem to be the required GCC compiler." >&5
$as_echo "$as_me: The $COMPILER_NAME compiler (located as $COMPILER) does not seem to be the required GCC compiler." >&6;}
      { $as_echo "$as_me:${as_lineno-$LINENO}: The result from running with --version was: \"$COMPILER_VERSION_TEST\"" >&5
$as_echo "$as_me: The result from running with --version was: \"$COMPILER_VERSION_TEST\"" >&6;}
      as_fn_error $? "GCC compiler is required. Try setting --with-tools-dir." "$LINENO" 5
fi
```

#### make all

执行`make all`

可能会遇到的错误

##### 1\. fatal error: 'iostream' file not found

![](https://upload-images.jianshu.io/upload_images/7426929-f1e3950d631af412.jpg?imageMogr2/auto-orient/strip|imageView2/2/w/1200/format/webp)

找不到iostream库位置或者缺失，到[这里](https://links.jianshu.com/go?to=https%3A%2F%2Fgithub.com%2Fimkiwa%2Fxcode-missing-libstdc-)下载源文件后按照install.sh的命令敲，但是在我电脑上没有用，后来无意间在`config.log`上面发现这段日志  

![](https://upload-images.jianshu.io/upload_images/7426929-327719fbec154484.jpg?imageMogr2/auto-orient/strip|imageView2/2/w/1200/format/webp)

  
将c++文件夹复制到`/Library/Developer/CommandLineTools/SDKs/MacOSX.sdk/usr/include`后解决；

##### 2\. ld: library not found for -lstdc++

![](https://upload-images.jianshu.io/upload_images/7426929-ade9d6de07d5f41a.jpg?imageMogr2/auto-orient/strip|imageView2/2/w/1200/format/webp)

解决办法： 缺失libstdc++库，同样把上面下载的文件中lib下面的libstdc\*放到`/Library/Developer/CommandLineTools/SDKs/MacOSX.sdk/usr/lib`后解决；

##### 3\. error: invalid argument '-std=gnu++98' not allowed with 'C'

![](https://upload-images.jianshu.io/upload_images/7426929-81a0f1032386910c.jpg?imageMogr2/auto-orient/strip|imageView2/2/w/1200/format/webp)

解决办法：`vi common/autoconf/generated-configure.sh`找到`CXXSTD_CXXFLAG="-std=gnu++98"`并注释掉；然后清空build文件夹下面的文件，重新执行configure&&make all；

##### 4\. ld: symbol(s) not found for architecture x86\_64

![](https://upload-images.jianshu.io/upload_images/7426929-58756bc26deb061a.jpg?imageMogr2/auto-orient/strip|imageView2/2/w/1200/format/webp)

解决办法：在configure时候把参数`--with-debug-level=slowdebug`去掉，然后清空build文件夹下面的文件，重新执行configure&&make all；  
或者说如果要使用slowdebug则需要修改`jdk/src/macosx/native/sun/osxapp/ThreadUtilities.m`中的attachCurrentThread方法上添加static；

漫长的等待后终于成功，输出如下

  

![](https://upload-images.jianshu.io/upload_images/7426929-8c6dc0d1610a0a37.jpg?imageMogr2/auto-orient/strip|imageView2/2/w/1082/format/webp)

#### 测试

```
cd macosx-x86_64-normal-server-release/jdk/bin/ && ./java -version

成功之后会输出
openjdk version "1.8.0-internal"
OpenJDK Runtime Environment (build 1.8.0-internal-y2ss_2020_02_25_18_12-b00)
OpenJDK 64-Bit Server VM (build 25.71-b00, mixed mode)
```

但是执行`./java -version`时遇到一个JVM crash

```
#
# A fatal error has been detected by the Java Runtime Environment:
#
#  SIGILL (0x4) at pc=0x000000010fe801bf, pid=48116, tid=0x0000000000002203
#
# JRE version: OpenJDK Runtime Environment (8.0) (build 1.8.0-internal-zhengshuangxi_2019_06_21_16_42-b00)
# Java VM: OpenJDK 64-Bit Server VM (25.71-b00 mixed mode bsd-amd64 compressed oops)
# Problematic frame:
# V  [libjvm.dylib+0x4801bf]  PerfDataManager::destroy()+0xab
#
# Failed to write core dump. Core dumps have been disabled. To enable core dumping, try "ulimit -c unlimited" before starting Java again
#
# An error report file with more information is saved as:
#
# If you would like to submit a bug report, please visit:
#   http://bugreport.java.com/bugreport/crash.jsp
#

[error occurred during error reporting , id 0x4]

Abort trap: 6
```

解决办法：在hotspot/src/share/vm/runtime/perfData.cpp文件中找到

```
void PerfDataManager::destroy() {

  if (_all == NULL)
    
    return;

  for (int index = 0; index < _all->length(); index++) {
    PerfData* p = _all->at(index);
    
  }

  delete(_all);
  delete(_sampled);
  delete(_constants);

  _all = NULL;
  _sampled = NULL;
  _constants = NULL;
}
```

### 调试

作为前iOS开发，看到能用Xcode调试就毫不犹豫的选择了这个，新建一个macOS工程

![](https://upload-images.jianshu.io/upload_images/7426929-9c86580ac988024a.jpg)

![](https://upload-images.jianshu.io/upload_images/7426929-20508ee2246cba54.jpg)

创建好后将project下面的文件删除

![](https://upload-images.jianshu.io/upload_images/7426929-c5ced6e53215baf3.jpg)

编辑scheme，在Build中将原来项目的target删除

![](https://upload-images.jianshu.io/upload_images/7426929-1a9b23ef0ef66026.jpg)

在Executable中选择编译出来的java可执行程序

![](https://upload-images.jianshu.io/upload_images/7426929-0981f87059d52093.jpg)

在Arguments中可以添加java的参数，比如-cp就是执行java字节码class文件，如果遇到`错误: 找不到或无法加载主类 ArrayCopy.class`，那就是编译出的java字节码文件有问题；

![](https://upload-images.jianshu.io/upload_images/7426929-a8a266d6b2f701b8.jpg)

然后将openjdk目录导入到该项目下

![](https://upload-images.jianshu.io/upload_images/7426929-be4b63f69caa3a27.jpg)

就可以愉快的进行调试了

![](https://upload-images.jianshu.io/upload_images/7426929-13dee949e06c65a9.jpg)

当然还会遇到以上情况，只需要在lldb中输入`process handle SIGSEGV --stop=false`然后点击下一步即可解决；

![](https://upload-images.jianshu.io/upload_images/7426929-40658af0d7bf152a.jpg)

当然也可以像这篇[文章](https://links.jianshu.com/go?to=https%3A%2F%2Fwww.cnblogs.com%2Fkelthuzadx%2Fp%2F10972992.html)一样在断点上面添加动作，避免每次都在lldb中输入；

### 参考

[https://www.jianshu.com/p/d9a1e1072f37](https://www.jianshu.com/p/d9a1e1072f37)  
[https://www.cnblogs.com/kelthuzadx/p/10972992.html](https://links.jianshu.com/go?to=https%3A%2F%2Fwww.cnblogs.com%2Fkelthuzadx%2Fp%2F10972992.html)  
[https://www.zhoujunwen.com/2019/building-openjdk-8-on-mac-osx-catalina-10-15](https://links.jianshu.com/go?to=https%3A%2F%2Fwww.zhoujunwen.com%2F2019%2Fbuilding-openjdk-8-on-mac-osx-catalina-10-15)

更多精彩内容，就在简书APP

"小礼物走一走，来简书关注我"

还没有人赞赏，支持一下

[![  ](https://upload.jianshu.io/users/upload_avatars/7426929/078db31a-f822-4c13-9725-4b6f7b3d611f.jpg?imageMogr2/auto-orient/strip|imageView2/1/w/100/h/100/format/webp)](https://www.jianshu.com/u/c8437f8273f2)

总资产19共写了6.8W字获得35个赞共26个粉丝

### 被以下专题收入，发现更多相似内容

### 推荐阅读[更多精彩内容](https://www.jianshu.com/)

-   在学习深入理解Java虚拟机的过程中，觉得自己编译JDK是很酷的一件事。所以就尝试一下，由于老版本编译的教程数不胜...
    
    [![](https://upload.jianshu.io/users/upload_avatars/16857744/9367613f-6dfd-4bac-a4a5-517bb2320f6e.png?imageMogr2/auto-orient/strip|imageView2/1/w/48/h/48/format/webp)井地儿](https://www.jianshu.com/u/fd3cfb4b135e)阅读 1,285评论 0赞 0
    
-   前言 这是一段相当痛苦的过程。逐次记录如下。 因为有时候排查问题如果更深层次, 不可避免的需要从JDK源码入手。此...
    
    [![](https://upload-images.jianshu.io/upload_images/9989976-469fdee4730dd144.png?imageMogr2/auto-orient/strip|imageView2/1/w/300/h/240/format/webp)](https://www.jianshu.com/p/38e697dcbaa5)

-   必修四包括了三个章节：第一章：三角函数；第二章：平面向量；第三章：三角恒等变形。这三个章节中，平面向量作为工具出现...
    
    [![](https://upload-images.jianshu.io/upload_images/10035079-1ca1528ee9ee9c0e.png?imageMogr2/auto-orient/strip|imageView2/1/w/300/h/240/format/webp)](https://www.jianshu.com/p/278d74480556)
