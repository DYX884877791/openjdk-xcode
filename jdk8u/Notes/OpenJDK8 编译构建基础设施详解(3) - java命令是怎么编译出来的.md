---
source: https://zhuanlan.zhihu.com/p/524611345
---
> 如果你有疑问Java命令是怎么编译出来的, 由哪些源文件编译而来. 那这篇文章正好能解答你的疑问.

## 回顾

这是OpenJDK8 编译构建基础设施详解系列的第三篇, 前面我们详细讲解了OpenJDK的构建编译系统的基本原理和make流程. 如果你想看之前的文章,可以点击如下的链接直达,

-   [OpenJDK8 编译构建基础设施详解(1) - A New OpenJDK Build-Infra Detail With GNU MAKE And AutoConf](https://zhuanlan.zhihu.com/p/518013598)
-   [OpenJDK8 编译构建基础设施详解(2) - Make流程解析](https://zhuanlan.zhihu.com/p/523472042)

如果你喜欢我的文章,可以关我的OpenJDK专栏:

## 摘要

此文会从正面 - 编译流程的角度剖析java命令怎么编译出来的.同时也会从反面结合编译日志获取一些信息. 原因有如下:

-   正面的分析细节特别多, 可能你会看着看着就迷糊了. 因此需要直接从编译日志中看一些直接详细的编译日志. 以获取一些明确的结果信息.
-   直接从日志分析,能够看到一些关键的点, 比如编译出某个目标target, 需要依赖哪些动态链接库等, 此类内容可以较方便的从日志获取. (当然需要一定的技巧以及对源码相对的了解, 即能够把日志对应到源码部分) , 但是日志本身是离散而割裂的.如果要直接看日志把全部流程串起来还是比较困难的.
-   同时由于构建系统代码的复杂性, 单看其中一个你都会感觉非常的吃力(虽然结合起来也很吃力). 因此只能结合起来,做一些相互印证, 把一些核心流程与逻辑验证清楚即可.

> 如果想要非常详细的编译日志, 请在编译的时候加上如下参数. `LOG=debug` , 比如我是用如下的命令进行编译的:  
> `make all LOG=debug`, 当然你也可以使用: `trace`此类的更详细的日志级别.

### 源码位置

我们知道平时调试`hello world` 的main方法位于如下位置: `src/share/bin/main.c` ,这里面是一个壳方法. 用于调用`JLI_Launch`:

```
// 代码位置:  src/share/bin/main.c

main(int argc, char **argv)
{
    int margc;
    char** margv;
    const jboolean const_javaw = JNI_FALSE;
    margc = argc;
    margv = argv;
    return JLI_Launch(margc, margv,
                   sizeof(const_jargs) / sizeof(char *), const_jargs,
                   sizeof(const_appclasspath) / sizeof(char *), const_appclasspath,
                   FULL_VERSION,
                   DOT_VERSION,
                   (const_progname != NULL) ? const_progname : *margv,
                   (const_launcher != NULL) ? const_launcher : *margv,
                   (const_jargs != NULL) ? JNI_TRUE : JNI_FALSE,
                   const_cpwildcard, const_javaw, const_ergo_class);
}
```

> 注: 上面的代码进行了精简, 把不要的win平台的宏定义的代码全部删除了.

### 目标命令目录

我们知道最后我们的命令是生成到了JDK的bin目录: `build/macosx-x86_64-normal-server-slowdebug/jdk/bin/java` 那我们可以顺着这个思路看看这个文件是怎么来的. 我们可以找到日志, 可以确认JDK的执行路径下的`java`命令实际是从编译目录的:`/jdk/objs/java_objs/java`拷贝而来的.

```
/bin/cp /path/to/jdkroot/jdk8u/build/macosx-x86_64-normal-server-slowdebug/jdk/objs/java_objs/java /path/to/jdkroot/jdk8u/build/macosx-x86_64-normal-server-slowdebug/jdk/bin/java
```

接下来,我们再找一下objs目录下的java文件是怎么来的.

```
# 找到了Linking 这个java命令的日志

Linking executable java
/usr/bin/clang -isysroot "/Applications/Xcode12.app/Contents/Developer/Platforms/MacOSX.platform/Developer/SDKs/MacOSX11.3.sdk" -iframework"/Applications/Xcode12.app/Contents/Developer/Platforms/MacOSX.platform/Developer/SDKs/MacOSX11.3.sdk/System/Library/Frameworks" -mmacosx-version-min=10.9.0 -Xlinker -rpath -Xlinker @loader_path/.  -Wl,-all_load /path/to/jdkroot/jdk8u/build/macosx-x86_64-normal-server-slowdebug/jdk/objs/libjli_static.a -framework Cocoa -framework Security -framework ApplicationServices -sectcreate __TEXT __info_plist /path/to/jdkroot/jdk8u/jdk/src/macosx/lib/Info-cmdline.plist  -Xlinker -install_name -Xlinker @rpath/java  -o /path/to/jdkroot/jdk8u/build/macosx-x86_64-normal-server-slowdebug/jdk/objs/java_objs/java /path/to/jdkroot/jdk8u/build/macosx-x86_64-normal-server-slowdebug/jdk/objs/java_objs/main.o   -pthread -lz
```

> 上面这个日志是从mac系统下找到的,因此是`clang` 编译器编译的.

我重新再找一版本linux环境下的日志, 下面的日志已经被我格式化后以便于理解.

```
# Linking java
#
# GCC 版本: gcc version 5.4.0 20160609 (Ubuntu 5.4.0-6ubuntu1~16.04.12) 
#
# GCC编译参数
#   -Wl,<arg>               Pass the comma separated arguments in <arg> to the linker:
#   -Wl.option              此选项传递option给连接程序;如果option中间有逗号,就将option分成多个选项,然后传递给会连接程序.
#                           把逗号分隔的参数传递给链接器.: 这里是: -Wl,-z,relro -> 解释为: 给链接器两个参数, -z和relro

#   -Xlinker <arg>          Pass <arg> to the linker
#                           实例: 
#                                -Xlinker --hash-style=both : 这是一个GNU的扩展语法 , 直接把option和参数直接在一个 -Xlinker中传入.
#                                -Xlinker -z -Xlinker defs  :
#                                -Xlinker -z -Xlinker noexecstack:
#                                -Xlinker --allow-shlib-undefined
#   -pie                    Produce a dynamically linked position independent executable on targets that
#                           support it. For predictable results, you must also specify the same set of options
#                           used for compilation (‘-fpie’, ‘-fPIE’, or model suboptions) when you specify
#                           this linker option.
#                                 
#                                -Xlinker -rpath -Xlinker \$ORIGIN/../lib/amd64/jli
#                                -Xlinker -rpath -Xlinker \$ORIGIN/../lib/amd64
#                                
#   -lpthread               : 连接多线程支持. linux下的多线支持库, 可以参考: https://wenku.baidu.com/view/4ed09d3fa46e58fafab069dc5022aaea998f4118.html
#
#                                # https://stackoverflow.com/questions/12637841/what-is-the-soname-option-for-building-shared-libraries-for
#                                -Xlinker -soname=lib.so 
#                                -Xlinker -version-script=/home/user/sourcecode/jdk8u/jdk/make/mapfiles/launchers/mapfile-x86_64
#   -o                      :
#                           -o /home/user/sourcecode/jdk8u/build/linux-x86_64-normal-server-slowdebug/jdk/objs/java_objs/java 
#
#   /home/user/sourcecode/jdk8u/build/linux-x86_64-normal-server-slowdebug/jdk/objs/java_objs/main.o
#   
#   -L <dir>                Add directory to library search path
#                           -L/home/user/sourcecode/jdk8u/build/linux-x86_64-normal-server-slowdebug/jdk/lib/amd64/jli 
#   -ljli                   : JVM自己的静态库 libjli.so
#   -ldl                    : 链接dl库   - 程序中手工加载动态链接库时要用的函数在这个库里面. 需要链接进来: https://www.cnblogs.com/SZxiaochun/p/7718621.html
#   -lc                     : 链接libc库 - https://blog.csdn.net/qq_38600065/article/details/104729290

Linking executable java
/usr/bin/gcc -Wl,-z,relro -Xlinker --hash-style=both -Xlinker -z -Xlinker defs -Xlinker -z -Xlinker noexecstack -Xlinker \
--allow-shlib-undefined -pie -Xlinker -rpath -Xlinker \$ORIGIN/../lib/amd64/jli -Xlinker -rpath -Xlinker \$ORIGIN/../lib/amd64   \
-lpthread -Xlinker -soname=lib.so -Xlinker -version-script=/home/user/sourcecode/jdk8u/jdk/make/mapfiles/launchers/mapfile-x86_64  \
-o \
/home/user/sourcecode/jdk8u/build/linux-x86_64-normal-server-slowdebug/jdk/objs/java_objs/java \
/home/user/sourcecode/jdk8u/build/linux-x86_64-normal-server-slowdebug/jdk/objs/java_objs/main.o    \
-L/home/user/sourcecode/jdk8u/build/linux-x86_64-normal-server-slowdebug/jdk/lib/amd64/jli -ljli -ldl -lc
```

注: 如果对于GCC的一些编译参数不是很了解的话,可以对照着GCC的官方手册查看: [https://gcc.gnu.org/onlinedocs/gcc-5.5.0/gcc.pdf](https://link.zhihu.com/?target=https%3A//gcc.gnu.org/onlinedocs/gcc-5.5.0/gcc.pdf)

这个可以看出,我们的java命令, 的确是通过 main.o 和其它的库链接在一起编译而来的. 而空上main.o 也是在`java_objs` 目录下, 我们可以大胆猜测一下,他就是我们的main.c编译的产物, 下面从日志中继续寻找来验证一下.

```
echo  "Compiling main.c (for java)"
echo  "Compiling main.c (for appletviewer)"
echo  "Compiling main.c (for extcheck)"
Compiling main.c (for java)
Compiling main.c (for appletviewer)
/usr/bin/gcc -Wall -Wno-parentheses -Wextra -Wno-unused -Wno-unused-parameter -Wformat=2 -pipe -fstack-protector -D_GNU_SOURCE -D_REENTRANT -D_LARGEFILE64_SOURCE -fno-omit-frame-pointer -D_LP64=1 -D_LITTLE_ENDIAN -DLINUX -DARCH='"amd64"' -Damd64 -DDEBUG -DRELEASE='"1.8.0-internal-debug"' -I/home/lijianhong/sourcecode/jdk8u/build/linux-x86_64-normal-server-slowdebug/jdk/include -I/home/lijianhong/sourcecode/jdk8u/build/linux-x86_64-normal-server-slowdebug/jdk/include/linux -I/home/lijianhong/sourcecode/jdk8u/jdk/src/share/javavm/export -I/home/lijianhong/sourcecode/jdk8u/jdk/src/solaris/javavm/export -I/home/lijianhong/sourcecode/jdk8u/jdk/src/share/native/common -I/home/lijianhong/sourcecode/jdk8u/jdk/src/solaris/native/common -fno-strict-aliasing -g -fPIE -I/home/lijianhong/sourcecode/jdk8u/jdk/src/share/bin -I/home/lijianhong/sourcecode/jdk8u/jdk/src/solaris/bin -I/home/lijianhong/sourcecode/jdk8u/jdk/src/linux/bin -DFULL_VERSION='"1.8.0-internal-debug-lijianhong_2022_02_15_20_53-b00"' -DJDK_MAJOR_VERSION='"1"' -DJDK_MINOR_VERSION='"8"' -DLIBARCHNAME='"amd64"' -DLAUNCHER_NAME='"openjdk"' -DPROGNAME='"java"' -DEXPAND_CLASSPATH_WILDCARDS        -g -O0   -DTHIS_FILE='"main.c"' -c -MMD -MF /home/lijianhong/sourcecode/jdk8u/build/linux-x86_64-normal-server-slowdebug/jdk/objs/java_objs/main.d -o /home/lijianhong/sourcecode/jdk8u/build/linux-x86_64-normal-server-slowdebug/jdk/objs/java_objs/main.o /home/lijianhong/sourcecode/jdk8u/jdk/src/share/bin/main.c
```

上面我们找到了编译`main.c`的日志, 简单分析日志. 可以得到如下信息:

-   编译的源文件是: **jdk8u/jdk/src/share/bin/main.c**
-   编译的输出目标是: **jdk8u/build/linux-x86_64-normal-server-slowdebug/jdk/objs/java_objs/main.o**
-   似乎main函数不只编译了一遍.如上面的日志,就好像一个main会编译很多次一样. 否则上面的日志读起来有些诡异.

-   **"Compiling main.c (for java)"**
-   **"Compiling main.c (for appletviewer)"**

-   输出目录是一个很特殊的**xx_objs**,这里是: java_objs , 因为我们从上面的信息得知这个: 目录的内容最终会被链接成 java,且最后会拷贝到各个目标位置, 一个是我们之前分析的:jdk8u/build/linux-x86_64-normal-server-slowdebug/jdk/bin; 这个是exported的jdk文件目录之一.

实际上main.c 的确会被编译多次, 我们可以来看一下下面的两个编译参数的对比,可以发现这个main方法的确被编译了多次. 但是不同的时候编译提供的编译参数是有区别的.`java命令`是使用了:

```
-DPROGNAME='"java"'\
-DEXPAND_CLASSPATH_WILDCARDS\
```

而`jar命令则是使用:

```
-DPROGNAME='"jar"'\
-DJAVA_ARGS='{ "-J-ms8m", "sun.tools.jar.Main", }'\
```

![](https://pic3.zhimg.com/v2-d887b10bc80ecc4b7b11bdc05ab7a2aa_b.jpg)

对于命令: java和jar两个launcher在编译main.c时的编译参数对比

实际上, JavaLauncher 的编译方式就是这样完成的. 使用同一个main方法. 使用不同的参数. 然后编译出不同的功能的命令工具. 其名字是Launcher,也比较好理解, 就是启动器的意思. 启动器就像是一个壳入口. 把相应的参数收集完成后 ,后面调用同一个`JLI_Launcher`方法,根据不同的参数初始化不同的执行环境. 基本的逻辑就是加载和引导初始化虚拟机. 然后把执行权限交给虚拟机. 虚拟机根据不同的参数加载不同的虚拟机类.

> 具体的Laucher的执行原理,在这里不过多的展开. 后面有时间可以单独分析一下这个执行流程. 其实如果要分析Java程序启动流程,肯定少不了这一步. 不过大家基本都是跳过这一个逻辑. 直接分析JavaMain函数了. 后面有机会给大家单独分析一下这个过程.

我们先按这个路径,把相关的东西都简单过一遍. 后面我们再接着用正面流程从构建系统的源代码角度与过程去分析编译的过程,以及Launcher的构建配置的技巧.

上面分析的linker的过程,除了用到了main.o外,还用到了另外一个动态链接库(除掉gun 提供的标准的libc以外,以及`dl`库. 这些都是编译环境直接提供的). 那就是:`jli` , 因为前面提到了main方法什么都没有. JLI_Launcher的方法由另外一个动态库提供. 我们先简单看一下这个库的编译日志:

```
# libjli.so 链接过程
Linking libjli.so
/usr/bin/gcc -Wl,-z,relro \
-Xlinker --hash-style=both \
-Xlinker -z -Xlinker defs \
-Xlinker -z -Xlinker noexecstack \
-shared \
-L/home/user/sourcecode/jdk8u/build/linux-x86_64-normal-server-slowdebug/jdk/lib/amd64 \
-L/home/user/sourcecode/jdk8u/build/linux-x86_64-normal-server-slowdebug/jdk/lib/amd64/server \
-Xlinker -z -Xlinker origin \
-Xlinker -rpath -Xlinker \$ORIGIN  \
-Xlinker -z -Xlinker origin \
-Xlinker -rpath -Xlinker \$ORIGIN/.. \
-Xlinker -version-script=/home/user/sourcecode/jdk8u/jdk/make/mapfiles/libjli/mapfile-vers  \
-Xlinker -soname=libjli.so \
-o /home/user/sourcecode/jdk8u/build/linux-x86_64-normal-server-slowdebug/jdk/lib/amd64/jli/libjli.so \
/home/user/sourcecode/jdk8u/build/linux-x86_64-normal-server-slowdebug/jdk/objs/libjli/ergo.o \
/home/user/sourcecode/jdk8u/build/linux-x86_64-normal-server-slowdebug/jdk/objs/libjli/ergo_i586.o \
/home/user/sourcecode/jdk8u/build/linux-x86_64-normal-server-slowdebug/jdk/objs/libjli/inffast.o \
/home/user/sourcecode/jdk8u/build/linux-x86_64-normal-server-slowdebug/jdk/objs/libjli/inflate.o \
/home/user/sourcecode/jdk8u/build/linux-x86_64-normal-server-slowdebug/jdk/objs/libjli/inftrees.o \
/home/user/sourcecode/jdk8u/build/linux-x86_64-normal-server-slowdebug/jdk/objs/libjli/java.o \
/home/user/sourcecode/jdk8u/build/linux-x86_64-normal-server-slowdebug/jdk/objs/libjli/java_md_common.o \
/home/user/sourcecode/jdk8u/build/linux-x86_64-normal-server-slowdebug/jdk/objs/libjli/java_md_solinux.o \
/home/user/sourcecode/jdk8u/build/linux-x86_64-normal-server-slowdebug/jdk/objs/libjli/jli_util.o \
/home/user/sourcecode/jdk8u/build/linux-x86_64-normal-server-slowdebug/jdk/objs/libjli/parse_manifest.o \
/home/user/sourcecode/jdk8u/build/linux-x86_64-normal-server-slowdebug/jdk/objs/libjli/splashscreen_stubs.o \
/home/user/sourcecode/jdk8u/build/linux-x86_64-normal-server-slowdebug/jdk/objs/libjli/version_comp.o \
/home/user/sourcecode/jdk8u/build/linux-x86_64-normal-server-slowdebug/jdk/objs/libjli/wildcard.o \
/home/user/sourcecode/jdk8u/build/linux-x86_64-normal-server-slowdebug/jdk/objs/libjli/zadler32.o \
/home/user/sourcecode/jdk8u/build/linux-x86_64-normal-server-slowdebug/jdk/objs/libjli/zcrc32.o \
/home/user/sourcecode/jdk8u/build/linux-x86_64-normal-server-slowdebug/jdk/objs/libjli/zutil.o    \
-ldl -lc -lpthread
```

这个日志里面,我们可以得知, 我们正在编译的是: `Linking libjli.so` , 这个文件的链接,需要很多的文件. 即objs文件. 我们挑一下`.o`文件的编译过程看看:**jli_util.o**

```
挑一个.o文件进行查看:

/usr/bin/gcc -Wall \
-Wno-parentheses \
-Wextra \
-Wno-unused \
-Wno-unused-parameter \
-Wformat=2 \
-pipe \
-fstack-protector \
-D_GNU_SOURCE \
-D_REENTRANT \
-D_LARGEFILE64_SOURCE \
-fno-omit-frame-pointer \
-D_LP64=1 \
-D_LITTLE_ENDIAN \
-DLINUX \
-DARCH='"amd64"' \
-Damd64 \
-DDEBUG \
-DRELEASE='"1.8.0-internal-debug"' \
-I/home/user/sourcecode/jdk8u/build/linux-x86_64-normal-server-slowdebug/jdk/include \
-I/home/user/sourcecode/jdk8u/build/linux-x86_64-normal-server-slowdebug/jdk/include/linux \
-I/home/user/sourcecode/jdk8u/jdk/src/share/javavm/export \
-I/home/user/sourcecode/jdk8u/jdk/src/solaris/javavm/export \
-I/home/user/sourcecode/jdk8u/jdk/src/share/native/common \
-I/home/user/sourcecode/jdk8u/jdk/src/solaris/native/common \
-fno-strict-aliasing -g -fPIC \
-I/home/user/sourcecode/jdk8u/build/linux-x86_64-normal-server-slowdebug/jdk/gensrc_headers \
-I/home/user/sourcecode/jdk8u/jdk/src/share/bin \
-I/home/user/sourcecode/jdk8u/jdk/src/solaris/bin \
-DLIBARCHNAME='"amd64"' \
-I/home/user/sourcecode/jdk8u/jdk/src/share/native/java/util/zip/zlib        \
-g -O0   \
-DTHIS_FILE='"jli_util.c"' \
-c -MMD -MF /home/user/sourcecode/jdk8u/build/linux-x86_64-normal-server-slowdebug/jdk/objs/libjli/jli_util.d \
-o /home/user/sourcecode/jdk8u/build/linux-x86_64-normal-server-slowdebug/jdk/objs/libjli/jli_util.o \
/home/user/sourcecode/jdk8u/jdk/src/share/bin/jli_util.c
```

同理,如果要看其它的目标obj文件是怎么来的,也可以直接搜索日志.下面我们就从正面的编译配置来看一下是怎么配置处理的.

## CompileLaunchers.gmk

> 文件目录: ./jdk8u/jdk/make/CompileLaunchers.gmk

### 重要概念: Launcher

基本在jdk的bin目录下的所有的命令都是通过同一个main函数编译出来的.

```
default: all

include $(SPEC)
include MakeBase.gmk
include NativeCompilation.gmk

# Setup the java compilers for the JDK build.
include Setup.gmk

# Build tools
include Tools.gmk

BUILD_LAUNCHERS =


define SetupLauncher
  # TODO: Fix mapfile on solaris. Won't work with ld as linker.
  # Parameter 1 is the name of the launcher (java, javac, jar...)
  # Parameter 2 is extra CFLAGS
  # Parameter 3 is extra LDFLAGS
  # Parameter 4 is extra LDFLAGS_SUFFIX_posix 连接标识的后缀_posix系统
  # Parameter 5 is extra LDFLAGS_SUFFIX_windows 连接标识的后缀_windows系统
  # Parameter 6 is optional Windows JLI library (full path) : windows jli库的全路径
  # Parameter 7 is optional Windows resource (RC) flags     : windows 资源文件标识
  # Parameter 8 is optional Windows version resource file (.rc) : windows 版本资源文件(.rc)
  # Parameter 9 is different output dir : 不同的输出目录
  # Parameter 10 if set, link statically with c runtime on windows. : 如果设置参数10 , 静态链接c运行时,在windows上.
  # Parameter 11 if set, override plist file on macosx. : 如果设置,覆盖 plist文件在 macosx上

  $1_WINDOWS_JLI_LIB := $(JDK_OUTPUTDIR)/objs/libjli/jli.lib
  ifneq ($6, )
    $1_WINDOWS_JLI_LIB := $6
  endif
  $1_VERSION_INFO_RESOURCE := $(JDK_TOPDIR)/src/windows/resource/version.rc
  ifneq ($8, )
    $1_VERSION_INFO_RESOURCE := $8
  endif

  #
  #  ...................................
  #

  # 这个call的意义已经从函数的定义的地方进行较完全的剖析了. 
    # call的本质是模板变量的变量值替换. 返回值是替换后的模板本身.(String).
    # 由于这里是直接call,而不是eval一起使用,是因为: 这个call生成的内容是更大的一个模板变量: SetupLauncher 的变量内容的一部分.
    # 因此这个eval的执行,由 SetupLauncher 这个函数的eval调用一起处理.
    $(call SetupNativeCompilation,BUILD_LAUNCHER_$1, \             #1: BUILD_LAUNCHER_java
        SRC := $(JDK_TOPDIR)/src/share/bin, \                      #2: SRC -  源代码目录,JDK_TOPDIR = ./jdk
        INCLUDE_FILES := main.c, \                                 #3: INCLUDE_FILES - 包含的文件, 这里只有一个 main.c
        LANG := C, \                                               #4: LANG - 语言类型: C
        OPTIMIZATION := $$($1_OPTIMIZATION_ARG), \                 #5: OPTIMIZATION - 优化你起床了吗
        CFLAGS := $$($1_CFLAGS) \                                  #6: CFLAGS - 编译标识. (优化参数)
            -I$(JDK_TOPDIR)/src/share/bin \
            -I$(JDK_TOPDIR)/src/$(OPENJDK_TARGET_OS_API_DIR)/bin \
            -I$(JDK_TOPDIR)/src/$(OPENJDK_TARGET_OS)/bin \
            -DFULL_VERSION='"$(FULL_VERSION)"' \
            -DJDK_MAJOR_VERSION='"$(JDK_MAJOR_VERSION)"' \
            -DJDK_MINOR_VERSION='"$(JDK_MINOR_VERSION)"' \
            -DLIBARCHNAME='"$(OPENJDK_TARGET_CPU_LEGACY)"' \
            -DLAUNCHER_NAME='"$(LAUNCHER_NAME)"' \
            -DPROGNAME='"$1"' $(DPACKAGEPATH) \
            $2, \     <========================  这个是setupLanucher传入的参数2 $2: CCFLAGS , 编译标识
        CFLAGS_solaris := -KPIC, \                                 #7: CFLAGS_solaris
        LDFLAGS := $(LDFLAGS_JDKEXE) \                             #8: LDFLAGS
            $(ORIGIN_ARG) \
            $$($1_LDFLAGS), \
        LDFLAGS_macosx := $(call SET_SHARED_LIBRARY_NAME,$1), \    #9: LDFLAGS_macosx
        LDFLAGS_linux := -lpthread \                               #10: LDFLAGS_linux
            $(call SET_SHARED_LIBRARY_NAME,$(LIBRARY_PREFIX)$(SHARED_LIBRARY_SUFFIX)), \
        LDFLAGS_solaris := $$($1_LDFLAGS_solaris) \                #11: LDFLAGS_solaris
            $(call SET_SHARED_LIBRARY_NAME,$(LIBRARY_PREFIX)$(SHARED_LIBRARY_SUFFIX)), \
        MAPFILE := $$($1_MAPFILE), \                               #12: MAPFILE
        LDFLAGS_SUFFIX := $(LDFLAGS_JDKEXE_SUFFIX) $$($1_LDFLAGS_SUFFIX), \ #13:LDFLAGS_SUFFIX
        LDFLAGS_SUFFIX_posix := $4, \                                 #14: LDFLAGS_SUFFIX_posix (这里引用的上一级调用的参数$4为空)
        LDFLAGS_SUFFIX_windows := $$($1_WINDOWS_JLI_LIB) \            #15: LDFLAGS_SUFFIX_windows
            $(JDK_OUTPUTDIR)/objs/libjava/java.lib advapi32.lib $5, \
        LDFLAGS_SUFFIX_linux := -L$(JDK_OUTPUTDIR)/lib$(OPENJDK_TARGET_CPU_LIBDIR)/jli -ljli $(LIBDL) -lc, \ #16: LDFLAGS_SUFFIX_linux
        LDFLAGS_SUFFIX_solaris := -L$(JDK_OUTPUTDIR)/lib$(OPENJDK_TARGET_CPU_LIBDIR)/jli -ljli -lthread $(LIBDL) -lc, \  #17: LDFLAGS_SUFFIX_solaris
        OBJECT_DIR := $(JDK_OUTPUTDIR)/objs/$1_objs$(OUTPUT_SUBDIR), \#18: OBJECT_DIR
        OUTPUT_DIR := $$($1_OUTPUT_DIR_ARG)$(OUTPUT_SUBDIR), \        #19: OUTPUT_DIR
        PROGRAM := $1, \                                              #20: PROGRAM
        DEBUG_SYMBOLS := true, \                                      #21: DEBUG_SYMBOLS
        VERSIONINFO_RESOURCE := $$($1_VERSION_INFO_RESOURCE), \       #22: VERSIONINFO_RESOURCE
        RC_FLAGS := $(RC_FLAGS) \                                     #23: RC_FLAGS
            -D "JDK_FNAME=$1$(EXE_SUFFIX)" \
            -D "JDK_INTERNAL_NAME=$1" \
            -D "JDK_FTYPE=0x1L" \
            $7, \
        MANIFEST := $(JDK_TOPDIR)/src/windows/resource/java.manifest, \#24: MANIFEST
        CODESIGN := $$($1_CODESIGN))                                  #25: CODESIGN => $(java_CODESIGN) 

  BUILD_LAUNCHERS += $$(BUILD_LAUNCHER_$1)

  ifneq (,$(filter $(OPENJDK_TARGET_OS), macosx aix))
    $$(BUILD_LAUNCHER_$1): $(JDK_OUTPUTDIR)/objs/libjli_static.a
  endif

  ifeq ($(OPENJDK_TARGET_OS), windows)
    $$(BUILD_LAUNCHER_$1): $(JDK_OUTPUTDIR)/objs/libjava/java.lib \
        $$($1_WINDOWS_JLI_LIB)
  endif
endef

# ############################################################################################
# Launcher设置 函数调用
# ############################################################################################

# On windows, the debuginfo files get the same name as for java.dll. Build
# into another dir and copy selectively so debuginfo for java.dll isn't
# overwritten.
$(eval $(call SetupLauncher,java, \
    -DEXPAND_CLASSPATH_WILDCARDS,,,user32.lib comctl32.lib, \
    $(JDK_OUTPUTDIR)/objs/jli_static.lib, $(JAVA_RC_FLAGS), \
    $(JDK_TOPDIR)/src/windows/resource/java.rc, $(JDK_OUTPUTDIR)/objs/java_objs,true))

$(JDK_OUTPUTDIR)/bin$(OUTPUT_SUBDIR)/java$(EXE_SUFFIX): $(BUILD_LAUNCHER_java)
$(MKDIR) -p $(@D)
$(RM) $@
$(CP) $(JDK_OUTPUTDIR)/objs/java_objs$(OUTPUT_SUBDIR)/java$(EXE_SUFFIX) $@

ifeq ($(ZIP_DEBUGINFO_FILES), true)
  DEBUGINFO_EXT := .diz
else ifeq ($(OPENJDK_TARGET_OS), macosx)
  DEBUGINFO_EXT := .dSYM
else ifeq ($(OPENJDK_TARGET_OS), windows)
  DEBUGINFO_EXT := .pdb
else
  DEBUGINFO_EXT := .debuginfo
endif

$(JDK_OUTPUTDIR)/bin$(OUTPUT_SUBDIR)/java$(DEBUGINFO_EXT): $(BUILD_LAUNCHER_java)
$(MKDIR) -p $(@D)
$(RM) $@
$(CP) -R $(JDK_OUTPUTDIR)/objs/java_objs$(OUTPUT_SUBDIR)/java$(DEBUGINFO_EXT) $@

BUILD_LAUNCHERS += $(JDK_OUTPUTDIR)/bin$(OUTPUT_SUBDIR)/java$(EXE_SUFFIX)
ifeq ($(ENABLE_DEBUG_SYMBOLS), true)
  ifneq ($(POST_STRIP_CMD), )
    ifneq ($(STRIP_POLICY), no_strip)
      BUILD_LAUNCHERS += $(JDK_OUTPUTDIR)/bin$(OUTPUT_SUBDIR)/java$(DEBUGINFO_EXT)
    endif
  endif
endif

ifeq ($(OPENJDK_TARGET_OS), windows)
  $(eval $(call SetupLauncher,javaw, \
      -DJAVAW -DEXPAND_CLASSPATH_WILDCARDS,,,user32.lib comctl32.lib, \
      $(JDK_OUTPUTDIR)/objs/jli_static.lib, $(JAVA_RC_FLAGS), \
      $(JDK_TOPDIR)/src/windows/resource/java.rc,,true))
endif


ifndef BUILD_HEADLESS_ONLY
  $(eval $(call SetupLauncher,appletviewer, \
      -DJAVA_ARGS='{ "-J-ms8m"$(COMMA) "sun.applet.Main"$(COMMA) }',, \
      $(XLIBS)))
endif

$(eval $(call SetupLauncher,extcheck, \
    -DJAVA_ARGS='{ "-J-ms8m"$(COMMA) "com.sun.tools.extcheck.Main"$(COMMA) }'))

$(eval $(call SetupLauncher,idlj, \
    -DJAVA_ARGS='{ "-J-ms8m"$(COMMA) "com.sun.tools.corba.se.idl.toJavaPortable.Compile"$(COMMA) }'))

$(eval $(call SetupLauncher,jar, \
    -DJAVA_ARGS='{ "-J-ms8m"$(COMMA) "sun.tools.jar.Main"$(COMMA) }'))



##########################################################################################

$(BUILD_LAUNCHERS): $(JDK_TOPDIR)/make/CompileLaunchers.gmk

all: $(BUILD_LAUNCHERS)

.PHONY: all
```

上面这段代码是经过精简后的 launcher.gmk的内容. 其主要使用流程和大逻辑在上面的核心代码中标识了出来. 核心代码我再摘抄出来给大家看看.

```
# 调用上面的设置launcher函数

# On windows, the debuginfo files get the same name as for java.dll. Build
# into another dir and copy selectively so debuginfo for java.dll isn't
# overwritten.
$(eval $(call SetupLauncher,java, \                 #1  launcher 名称
        -DEXPAND_CLASSPATH_WILDCARDS,               #2  CFLAGS
        ,                                           #3  LDFLAGS
        ,                                           #4  LDFLAGS_SUFFIX_posix
        user32.lib comctl32.lib, \                  #5  LDFLAGS_SUFFIX_windows 
        $(JDK_OUTPUTDIR)/objs/jli_static.lib,\      #6  optional Windows JLI library (full path) : windows jli库的全路径  
        $(JAVA_RC_FLAGS), \                         #7  optional Windows resource (RC) flags     : windows 资源文件标识
        $(JDK_TOPDIR)/src/windows/resource/java.rc, #8  optional Windows version resource file (.rc) : windows 版本资源文件(.rc)
        $(JDK_OUTPUTDIR)/objs/java_objs,            #9  different output dir : 不同的输出目录
        true                                        #10 if set, link statically with c runtime on windows. : 如果设置参数10 , 静态链接c运行时,在windows上.
    )
)
```

上面就是一个设置`Java Launcher`的代码. 传入launcher的名称等参数. 然后里面的核心逻辑是调用, SetupNativeCompilation , 也就是本地代码的编译配置设置. 这里会完成编译相关的文件, 参数以及输出目录的配置.

```
define add_native_source
  # param 1 = BUILD_MYPACKAGE
  # parma 2 = the source file name (..../alfa.c or .../beta.cpp)
  # param 3 = the bin dir that stores all .o (.obj) and .d files.
  # param 4 = the c flags to the compiler
  # param 5 = the c compiler
  # param 6 = the c++ flags to the compiler
  # param 7 = the c++ compiler
  # param 8 = the flags to the assembler

  ifneq (,$$(filter %.c,$2))
    # Compile as a C file 
  else ifneq (,$$(filter %.m,$2))
    # Compile as a objective-c file
  else ifneq (,$$(filter %.s,$2))
    # Compile as assembler file
  else
    # Compile as a C++ file
    $1_$2_FLAGS=$6 $$($1_$(notdir $2)_CXXFLAGS) -DTHIS_FILE='"$$(<F)"' -c
    $1_$2_COMP=$7
    $1_$2_DEP_FLAG:=$(CXX_FLAG_DEPS)
  endif
  # Generate the .o (.obj) file name and place it in the bin dir.
  $1_$2_OBJ:=$3/$$(patsubst %.cpp,%$(OBJ_SUFFIX),$$(patsubst %.c,%$(OBJ_SUFFIX),$$(patsubst %.m,%$(OBJ_SUFFIX),$$(patsubst %.s,%$(OBJ_SUFFIX),$$(notdir $2)))))
  # Only continue if this object file hasn't been processed already. This lets the first found
  # source file override any other with the same name.
  
endef

define SetupNativeCompilation
  # param 1 is for example BUILD_MYPACKAGE
  # param 2,3,4,5,6,7,8 are named args.
  #   SRC one or more directory roots to scan for C/C++ files.
  #   LANG C or C++
  #   CFLAGS the compiler flags to be used, used both for C and C++.
  #   CXXFLAGS the compiler flags to be used for c++, if set overrides CFLAGS.
  #   LDFLAGS the linker flags to be used, used both for C and C++.
  #   LDFLAGS_SUFFIX the linker flags to be added last on the commandline
  #       typically the libraries linked to.
  #   ARFLAGS the archiver flags to be used
  #   OBJECT_DIR the directory where we store the object files
  #   LIBRARY the resulting library file
  #   PROGRAM the resulting exec file
  #   INCLUDES only pick source from these directories
  #   EXCLUDES do not pick source from these directories
  #   INCLUDE_FILES only compile exactly these files!
  #   EXCLUDE_FILES with these names
  #   VERSIONINFO_RESOURCE Input file for RC. Setting this implies that RC will be run
  #   RC_FLAGS flags for RC.
  #   MAPFILE mapfile
  #   REORDER reorder file
  #   DEBUG_SYMBOLS add debug symbols (if configured on)
  #   CC the compiler to use, default is $(CC)
  #   LDEXE the linker to use for linking executables, default is $(LDEXE)
  #   OPTIMIZATION sets optimization level to NONE, LOW, HIGH, HIGHEST
  $(foreach i,2 3 4 5 6 7 8 9 10 11 12 13 14 15 16 17 18 19 20 21 22 23 24 25 26, $(if $($i),$1_$(strip $($i)))$(NEWLINE))
  $(call LogSetupMacroEntry,SetupNativeCompilation($1),$2,$3,$4,$5,$6,$7,$8,$9,$(10),$(11),$(12),$(13),$(14),$(15),$(16),$(17),$(18),$(19),$(20),$(21),$(22),$(23),$(24),$(25),$(26))
  $(if $(27),$(error Internal makefile error: Too many arguments to SetupNativeCompilation, please update NativeCompilation.gmk))

  # ....
endef
```

我们再回到日志看一下`SetupNativeCompilation`的相关的设置的输出:

> 数据来源: build.log

```
SetupNativeCompilation(BUILD_LAUNCHER_java) 
 [2] SRC := /home/user/sourcecode/jdk8u/jdk/src/share/bin 
 [3] INCLUDE_FILES := main.c 
 [4] LANG := C 
 [5] OPTIMIZATION := $(java_OPTIMIZATION_ARG) 
 [6] CFLAGS := $(java_CFLAGS) -I/home/user/sourcecode/jdk8u/jdk/src/share/bin -I/home/user/sourcecode/jdk8u/jdk/src/solaris/bin -I/home/user/sourcecode/jdk8u/jdk/src/linux/bin -DFULL_VERSION='"1.8.0-internal-debug-user_2022_02_15_20_53-b00"' -DJDK_MAJOR_VERSION='"1"' -DJDK_MINOR_VERSION='"8"' -DLIBARCHNAME='"amd64"' -DLAUNCHER_NAME='"openjdk"' -DPROGNAME='"java"' -DEXPAND_CLASSPATH_WILDCARDS 
 [7] CFLAGS_solaris := -KPIC 
 [8] LDFLAGS := -Wl,-z,relro -Xlinker --hash-style=both -Xlinker -z -Xlinker defs -Xlinker -z -Xlinker noexecstack -Xlinker --allow-shlib-undefined -pie -Xlinker -rpath -Xlinker \$$ORIGIN/../lib/amd64/jli -Xlinker -rpath -Xlinker \$$ORIGIN/../lib/amd64 $(java_LDFLAGS) 
 [9] LDFLAGS_macosx := -Xlinker -soname=java 
 [10] LDFLAGS_linux := -lpthread -Xlinker -soname=lib.so 
 [11] LDFLAGS_solaris := $(java_LDFLAGS_solaris) -Xlinker -soname=lib.so 
 [12] MAPFILE := $(java_MAPFILE) 
 [13] LDFLAGS_SUFFIX := $(java_LDFLAGS_SUFFIX) 
 [14] LDFLAGS_SUFFIX_posix := 
 [15] LDFLAGS_SUFFIX_windows := $(java_WINDOWS_JLI_LIB) /home/user/sourcecode/jdk8u/build/linux-x86_64-normal-server-slowdebug/jdk/objs/libjava/java.lib advapi32.lib user32.lib comctl32.lib 
 [16] LDFLAGS_SUFFIX_linux := -L/home/user/sourcecode/jdk8u/build/linux-x86_64-normal-server-slowdebug/jdk/lib/amd64/jli -ljli -ldl -lc 
 [17] LDFLAGS_SUFFIX_solaris := -L/home/user/sourcecode/jdk8u/build/linux-x86_64-normal-server-slowdebug/jdk/lib/amd64/jli -ljli -lthread -ldl -lc 
 [18] OBJECT_DIR := /home/user/sourcecode/jdk8u/build/linux-x86_64-normal-server-slowdebug/jdk/objs/java_objs 
 [19] OUTPUT_DIR := $(java_OUTPUT_DIR_ARG) 
 [20] PROGRAM := java 
 [21] DEBUG_SYMBOLS := true 
 [22] VERSIONINFO_RESOURCE := $(java_VERSION_INFO_RESOURCE) 
 [23] RC_FLAGS := -D "JDK_FNAME=java" -D "JDK_INTERNAL_NAME=java" -D "JDK_FTYPE=0x1L" -i "/home/user/sourcecode/jdk8u/jdk/src/windows/resource/icons" 
 [24] MANIFEST := /home/user/sourcecode/jdk8u/jdk/src/windows/resource/java.manifest 
 [25] CODESIGN := $(java_CODESIGN)
```

最后再回到上面的main函数, 我们看看调试起来看看一些参数, 上面我们有说到java这一个launcher的编译的时候有一个常量(预处理常量 -D传入的)是"java",这个与我们调试的图能够匹配起来. 这个值会影响后面的JLI_Launcher函数的具体行为. 具体JLI_Launcher是怎么工作的, 我们下一篇文章见.

![](https://pic1.zhimg.com/v2-9d1ed27fd964b0ff08e5b3efbf1d1c58_b.jpg)

HostSpot启动时的main函数的debug断点

## 总结

写了这么长,好像什么也没有写一样. 最终只是抛出一个结果是我们的launcher大概是怎么链接和工作的. 以及大致的配置流程. 实际上面的makefile的流程会繁琐得多, 本有一些复杂但是又精妙的makefile的一些片段代码特别有趣.但是由于内容过多.和繁琐, 直接僵硬的讲一段逻辑又不是很好. 有些逻辑我也没有细看. 所以最后也就索引省略了. 如果你能大概明白怎么去分析编译日志以及怎么去看makefile的配置. 并且大概弄明白java是怎么编译出来的, 那这篇文章的目的就达到了. 毕竟没有讲完全的地方,你已经有能力自己去探索了. Have fun.

参考:

1.  [GCC 手册](https://link.zhihu.com/?target=https%3A//gcc.gnu.org/onlinedocs/gcc-5.5.0/gcc.pdf)
2.  [openjdk1.8中的Makefile](https://link.zhihu.com/?target=https%3A//www.lehoon.cn/backend/2018/12/06/openjdk-makefile-note.html%23Jdk%25E7%259B%25AE%25E5%25BD%2595)
