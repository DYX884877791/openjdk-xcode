---
source: https://hengyunabc.blog.csdn.net/article/details/16912775
---
编绎整个[OpenJDK](https://so.csdn.net/so/search?q=OpenJDK&spm=1001.2101.3001.7020)要很久，而且有很多东西是不需要的。研究HotSpot的话，其实只要下HotSpot部分的代码就可以了。

下面简单记录下编绎调试HotSpot一些步骤。

## 一、编绎

进入hotsopt的make目录下：

cd code/cpp/openjdk/hotspot/make/

用make help可以看到有很多有用的信息。当然查看[Makefile](https://so.csdn.net/so/search?q=Makefile&spm=1001.2101.3001.7020)文件，里面也有很多有用的注释。

make help会输出当前的一些环境变量的设置，如果不对，自然编绎不过去。

设置环境变量：

```
unset JAVA_HOME
export ARCH_DATA_MODEL=64
export JDK_IMPORT_PATH=/usr/lib/jvm/java-7-oracle
export ALT_BOOTDIR=/usr/lib/jvm/java-7-oracle
export ZIP_DEBUGINFO_FILES=0                      //这个貌似不起作用。。这些变量的定义貌似都在def.make文件里。还有一个这样的参数：FULL_DEBUG_SYMBOLS
```

用make all_beug来编绎。

编绎后好，**到目录下openjdk/hotspot/build/linux/linux_amd64_compiler2/jvmg，执行 unzip libjvm.diz，解压得到调试信息文件**。

这个算是个坑，默认情况下，会压缩调试信息文件，这样用gdb调试时，就会出现下面的提示：

no debugging symbols found

从编绎的输出信息来看，是有一个ZIP_DEBUGINFO_FILES的参数，但是设置了环境变量却不起效。

## 二、调试

在openjdk/hotspot/build/linux/linux_amd64_compiler2/jvmg目录下，有一个hotspot的脚本，只要执行这个脚本，就可以启动Java进程了。

用 ./hotspot -gdb 就会自动进入gdb调试，并停在main函数入口。

后面还可以加一些jvm的启动参数等。

**这个./hotsopt 脚本是怎么工作的？**

用 sh -x ./hotspot 来查看这个脚本的执行过程，可以发现实际上是设置了 LD_LIBRARY_PATH的环境变量，再调用了一个./gamma 的程序。

```
+ LD_LIBRARY_PATH=/home/hengyunabc/code/cpp/openjdk/hotspot/build/linux/linux_amd64_compiler2/jvmg:/usr/lib/jvm/java-7-oracle/jre/lib/amd64
+ export LD_LIBRARY_PATH
+ JPARMS= 
+ LAUNCHER=/home/hengyunabc/code/cpp/openjdk/hotspot/build/linux/linux_amd64_compiler2/jvmg/gamma
+ [ ! -x /home/hengyunabc/code/cpp/openjdk/hotspot/build/linux/linux_amd64_compiler2/jvmg/gamma ]
+ GDBSRCDIR=/home/hengyunabc/code/cpp/openjdk/hotspot/build/linux/linux_amd64_compiler2/jvmg
+ cd /home/hengyunabc/code/cpp/openjdk/hotspot/build/linux/linux_amd64_compiler2/jvmg/../../..
+ pwd
+ BASEDIR=/home/hengyunabc/code/cpp/openjdk/hotspot/build
+ LD_PRELOAD= exec /home/hengyunabc/code/cpp/openjdk/hotspot/build/linux/linux_amd64_compiler2/jvmg/gamma
```

  
**实际上是通过设置LD_LIBRARY_PATH 环境变量，去优先加载编绎好的libjvm.so。**hotsopt jvm的代码都编绎链接在libjvm.so这个文件里。

查看./hotspot里的这个函数init_gdb，就知道它是怎么启动并设置好gdb的了：

```
init_gdb() {
# Create a gdb script in case we should run inside gdb
    GDBSCR=/tmp/hsl.$$
    rm -f $GDBSCR
    cat >>$GDBSCR <<EOF
cd `pwd`
handle SIGUSR1 nostop noprint
handle SIGUSR2 nostop noprint
set args $JPARMS
file $LAUNCHER
directory $GDBSRCDIR
# Get us to a point where we can set breakpoints in libjvm.so
break InitializeJVM
run
# Stop in InitializeJVM
delete 1
# We can now set breakpoints wherever we like
EOF
}
```

所以，其实也可以这样开始调试：

export LD_LIBRARY_PATH=/home/hengyunabc/code/cpp/openjdk/hotspot/build/linux/linux_amd64_compiler2/jvmg:/usr/lib/jvm/java-7-oracle/jre/lib/amd64  
gdb  
在gdb里执行file ./gamma，然后就可以调试了。

## 三、使用[Eclipse](https://so.csdn.net/so/search?q=Eclipse&spm=1001.2101.3001.7020)来调试

尽管gdb功能强大，命令丰富，但是在查看调试的变量时，十分的不方便。特别是hotsopt里，很多东西都是用指针来存放的，有时要跳转好几层才能查看到想要的信息。

下载Eclipse的CDT版，或者安装CDT的插件。

### 导入Eclipse工程：

"File", "Import", "C/C++", "Existing Code as Makefile Project":

![](https://img-blog.csdn.net/20131208160145562)

先择Linux GCC：

![](https://img-blog.csdn.net/20131208160407140)

然后，就可以把项目导到Eclipse里了。会有很多错误提示，但是不影响我们的调试。

### 在Eclipse里调试：

首先，要设置要调试的文件的路径：

![](https://img-blog.csdn.net/20131208160548750)

设置LD_LIBRARY_PATH：

![](https://img-blog.csdn.net/20131208160641531)

然后就可以调试了。还有一个地方比较重要：

**想在运行时输入gdb指令，可以在console view，在右边的下拉里，可以发现有一个gbd的console，还有一个gdb trace的console。** 

## 四、一些有用的东东

jvmg1目录下是O1优化下的，fastdebug目录下是O3优化的。

java 进程的main入口在：openjdk/hotspot/src/share/tools/launcher/java.c 文件里。

在gdb里，用info sharedlibrary 命令查看实际使用到的是哪些so文件。

用file命令来判断一个可执行文件，so是32位的还是64位的。

查看一个so文件是否包含调试信息，可以用readelf -S xxx.so 命令来查看是否有debug相关的段。这个方法不一定准确，因为调试信息有可能放在外部文件里。

## 公众号

欢迎关注公众号：横云断岭的专栏，专注分享Java，Spring Boot，Arthas，Dubbo。

![横云断岭的专栏](https://img-blog.csdnimg.cn/20190113225220997.jpg?x-oss-process=image/watermark,type_ZmFuZ3poZW5naGVpdGk,shadow_10,text_aHR0cHM6Ly9ibG9nLmNzZG4ubmV0L2hlbmd5dW5hYmM=,size_16,color_FFFFFF,t_70)
