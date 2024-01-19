---
source: https://hengyunabc.blog.csdn.net/article/details/26898657
---
## 安装hsdis

要查看JIT生成的汇编代码，要先装一个反汇编器：hsdis。从名字来看，即HotSpot disassembler。

实际就是一个动态链接库。网络上有已经编绎好的文件，直接下载即可。

国内的：http://hllvm.group.iteye.com/

也可以自己编绎，只是编绎hsdis，还是比较快的。

参考这里：http://www.chrisnewland.com/building-hsdis-on-linux-amd64-on-debian-369

官方的参考文档：

https://wikis.oracle.com/display/HotSpotInternals/PrintAssembly

**简而言之，安装只要把下载到，或者编绎好的so文件放到对应的Java安装路径下即可。**

**典型的情况是把下载到的 hsdis-amd64.so 放到 /usr/lib/jvm/java-7-oracle/jre/lib/amd64/ 目录下。**

可以用下面这个命令来查看是否安装成功。

**java -XX:+UnlockDiagnosticVMOptions -XX:+PrintAssembly -version**

如果输出有：

Java HotSpot(TM) 64-Bit Server VM warning: PrintAssembly is enabled

则安装成功。

## 安装JITWatch

hsdis自然是很重要，但是今天的主角是JITWatch，一个分析展现JIT日志等的图形界面工具，非常的好用。

[https://github.com/AdoptOpenJDK/jitwatch](https://github.com/AdoptOpenJDK/jitwatch)

首先下载jar包，这里下载的是JDK7版本的：

wget https://adopt-openjdk.ci.cloudbees.com/job/jitwatch/jdk=JDK_1.7/ws/jitwatch-1.0.0-SNAPSHOT-JDK_1.7.tar.gz

解压得到jar 文件。

下载代码文件，主要是为了相关的依赖jar：

wget  https://github.com/AdoptOpenJDK/jitwatch/archive/master.zip

解压，从lib目录得到相关的jar包。和前面得到的JITWatch的jar包放在一起。

用这个命令启动：

java -cp /usr/lib/jvm/java-7-oracle/lib/tools.jar:/usr/lib/jvm/java-7-oracle/jre/lib/jfxrt.jar:jitwatch-1.0.0-SNAPSHOT.jar:slf4j-api-1.7.7.jar:hamcrest-core-1.3.jar:logback-classic-1.1.2.jar:logback-core-1.1.2.jar com.chrisnewland.jitwatch.launch.LaunchUI

启动后显示的界面如下，左上方有一个sanbox的按钮，点击会在当前目录下生成一个sanbox文件夹，里面存放着sanbox这个示例相关的代码。

![](https://img-blog.csdn.net/20140525025847281?watermark/2/text/aHR0cDovL2Jsb2cuY3Nkbi5uZXQvaGVuZ3l1bmFiYw==/font/5a6L5L2T/fontsize/400/fill/I0JBQkFCMA==/dissolve/70/gravity/SouthEast)

点击sanbox就可以随便尝试下功能了。

## 生成JIT log文件

**首先要写一个足够复杂的类，让JIT编绎器认为它需要进行优化，不然产生的日志可能没什么内容。**我在这里被坑了不少时间，怎么死活都没有日志。。

```
public class Test {public volatile long sum = 0;public int add(int a, int b) {int temp = a + b;sum += temp;return temp;}public static void main(String[] args) {Test test = new Test();int sum = 0;for (int i = 0; i < 1000000; i++) {sum = test.add(sum, 1);}System.out.println("Sum:" + sum);System.out.println("Test.sum:" + test.sum);}}
```

加上一些参数来执行程序，就可以直接生成JIT log文件了，比如为Test类生成：

```
javac Test.javajava -server -XX:+UnlockDiagnosticVMOptions -XX:+TraceClassLoading  -XX:+PrintAssembly -XX:+LogCompilation -XX:LogFile=live.log  Test
```

注意生成的log文件里，可能会有一些class path相关的东东，所以JITWatch有可能会解析失败。这时注意看左下的“Errors”。

个人推荐在Eclipse里配置生成JIT log，这样使用起来更方便。

在“Run Configuration”里配置上JVM参数：

```
-server -XX:+UnlockDiagnosticVMOptions -XX:+TraceClassLoading  -XX:+PrintAssembly -XX:+LogCompilation -XX:LogFile=${java_type_name}.log
```

![](https://img-blog.csdn.net/20140525141541000?watermark/2/text/aHR0cDovL2Jsb2cuY3Nkbi5uZXQvaGVuZ3l1bmFiYw==/font/5a6L5L2T/fontsize/400/fill/I0JBQkFCMA==/dissolve/70/gravity/SouthEast)

然后，当执行时，会自动生成JIT log。

在JITWatch里，“Open Log"，打开生成的Test.log，然后配置config：

![](https://img-blog.csdn.net/20140525143155718?watermark/2/text/aHR0cDovL2Jsb2cuY3Nkbi5uZXQvaGVuZ3l1bmFiYw==/font/5a6L5L2T/fontsize/400/fill/I0JBQkFCMA==/dissolve/70/gravity/SouthEast)

然后，点”Start"就可以查看到分析的结果了。

这是一个分析结果的展示，左边是源代码，中间是ByteCode，右边是汇编代码：

![](https://img-blog.csdn.net/20140525144251062?watermark/2/text/aHR0cDovL2Jsb2cuY3Nkbi5uZXQvaGVuZ3l1bmFiYw==/font/5a6L5L2T/fontsize/400/fill/I0JBQkFCMA==/dissolve/70/gravity/SouthEast)

JITWatch还有很多强大的功能。具体请参考wiki：

[https://github.com/AdoptOpenJDK/jitwatch/wiki](https://github.com/AdoptOpenJDK/jitwatch/wiki)

比如查看函数是否inline，循环展开，分支预测等。

作者有一个PDF介绍：

http://www.chrisnewland.com/images/jitwatch/HotSpot_Profiling_Using_JITWatch.pdf

视频：

https://skillsmatter.com/skillscasts/5243-chris-newland-hotspot-profiling-with-jit-watch

## Java里volatile的实现

从生成的汇编代码，可以看出Java里volatile的实现是用lock指令前缀来实现的：

```
0x00007f80950601b0: lock addl $0x0,(%rsp)  ;*putfield sum
                                           ; - Test::add@12 (line 6)
                                           ; - Test::main@18 (line 16)
```

  
更加关于lock指令前缀，请参考这里：

http://stackoverflow.com/questions/8891067/what-does-the-lock-instruction-mean-in-x86-assembly

## 参考：

http://psy-lob-saw.blogspot.com/2013/01/java-print-assembly.html

http://www.chrisnewland.com/images/jitwatch/HotSpot_Profiling_Using_JITWatch.pdf

## 公众号

欢迎关注公众号：横云断岭的专栏，专注分享Java，Spring Boot，Arthas，Dubbo。

![横云断岭的专栏](https://img-blog.csdnimg.cn/20190113225220997.jpg?x-oss-process=image/watermark,type_ZmFuZ3poZW5naGVpdGk,shadow_10,text_aHR0cHM6Ly9ibG9nLmNzZG4ubmV0L2hlbmd5dW5hYmM=,size_16,color_FFFFFF,t_70)
