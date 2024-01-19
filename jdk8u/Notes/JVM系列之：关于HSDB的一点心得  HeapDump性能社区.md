---
source: https://heapdump.cn/article/3535254
---
之前未接触过 HSDB 工具，在深入学习反射时，研究其源码时需要了解生成的字节码文件，恰巧看到别人使用了 HSDB 工具，因此花时间学习了一番。

HSDB（Hotspot Debugger)，是一款内置于 SA 中的 GUI 调试工具，可用于调试 JVM 运行时数据，从而进行故障排除。

### sa-jdi.jar

在 Java9 之前，JAVA_HOME/lib 目录下有个 sa-jdi.jar，可以通过如下命令启动HSDB(`图形界面`)及CLHSDB(`命令行`)。

```
java -cp /Library/Java/JavaVirtualMachines/jdk1.8.0_301.jdk/Contents/Home/lib/sa-jdi.jar sun.jvm.hotspot.HSDB
```

sa-jdi.jar中的sa的全称为 Serviceability Agent，它之前是sun公司提供的一个用于协助调试 HotSpot 的组件，而 HSDB 便是使用Serviceability Agent 来实现的。

由于Serviceability Agent 在使用的时候会先attach进程，然后暂停进程进行snapshot，最后deattach进程(`进程恢复运行`)，所以在使用 HSDB 时要注意。

### jhsdb

jhsdb 是 Java9 引入的，可以在 JAVA_HOME/bin 目录下找到 jhsdb；它取代了 JDK9 之前的 JAVA_HOME/lib/sa-jdi.jar，可以通过下述命令来启动 HSDB。

```
$ cd /Library/Java/JavaVirtualMachines/jdk-9.0.4.jdk/Contents/Home/bin/
$ jhsdb hsdb
```

jhsdb 有 clhsdb、debugd、hsdb、jstack、jmap、jinfo、jsnap 这些 mode 可以使用。

其中 hsdb 为 ui debugger，就是 jdk9 之前的 sun.jvm.hotspot.HSDB；而 clhsdb 即为 jdk9 之前的sun.jvm.hotspot.CLHSDB。

## HSDB实操

### 启动HSDB

检测不同 JDK 版本需要使用不同的 HSDB 版本，否则容易出现无法扫描到对象等莫名其妙的问题。

Mac：JDK7 和 JDK8 均可以采用以下的方式

```
$ java -cp /Library/Java/JavaVirtualMachines/jdk1.8.0_301.jdk/Contents/Home/lib/sa-jdi.jar sun.jvm.hotspot.HSDB
```

如果执行报错，则前面加上 sudo，或者更改 sa-jdi.jar 的权限。

```
sudo chmod -R 777 sa-jdi.jar 
```

本地安装的是 JDK8，在启动 HSDB 后，发现无法连接到 Java 进程，在 attach 过程中会提示如下错误：

![](https://img-blog.csdnimg.cn/img_convert/2a858cf0383928a7de14c6d857b30c82.png)

网上搜索相关解决方案，建议更换 JDK 版本。可以去参考 [Mac下安装多个版本的JDK并随意切换](https://blog.csdn.net/wo541075754/article/details/115283006)

个人在配置的过程中遇到了这样一个问题：在切换 JDK 版本时，发现不生效，网上各种查找方案，动手尝试，最后都没有成功。解决方案：**手动修改 .bash_profile 文件，增加注释。**

首次尝试 JDK 11，但是还是无法 attach Java 进程，试了好久都不行，只能再次尝试 JDK9.

而 JDK9 的启动方式有些区别

```
$ cd /Library/Java/JavaVirtualMachines/jdk-9.0.4.jdk/Contents/Home/bin/
$ jhsdb hsdb
```

其中启动版本可以使用 /usr/libexec/java_home -V 获取 HSDB 对 Serial GC 支持的较好，因此 Debug 时增加参数 -XX:+UseSerialGC。**注意运行程序 Java 的版本和 hsdb 的 Java 版本要一致才行**。

注意：如果后续想要下载 .class 文件，启动 hsdb 时，需要执行 sudo jhsdb hsdb 命令。

### HSDB可视化界面

比如说有这么一个 Java 程序，我们使用 Thread.sleep 方法让其长久等待，然后获取其进程 id。

```
public class InvokeTest {
      

  public static void printException(int num) {
      
    new Exception("#" + num).printStackTrace();
  }

  public static void main(String[] args)
      throws ClassNotFoundException, NoSuchMethodException, InvocationTargetException, IllegalAccessException, InterruptedException {
      
    Class<?> cl = Class.forName("InvokeTest");
    Method method = cl.getMethod("printException", int.class);
    for (int i = 1; i < 20; i++) {
      
      method.invoke(null, i);
      if (i == 17) {
      
        Thread.sleep(Integer.MAX_VALUE);
      }
    }
  }
}
```

然后在 terminal 窗口执行 jps 命令：

```
27995 InvokeTest
```

然后在 HSDB 界面点击 file 的 attach，输入 pid，如果按照上述步骤操作，是可以操作成功的。

![](https://img-blog.csdnimg.cn/img_convert/bd6eccad00046c15409727347d56e02c.png)

attach 成功后，效果如下所示：

![](https://img-blog.csdnimg.cn/img_convert/173e8c12a741604dea8c03f830329123.png)

更多操作选择推荐阅读：[解读HSDB](https://www.jianshu.com/p/f6f9b14d5f21)

### 分析对象存储区域

下面代码中的 heatStatic、heat、heatWay 分别存储在什么地方呢？

```
package com.msdn.java.hotspot.hsdb;

public class Heat2 {
      

  private static Heat heatStatic = new Heat();
  private Heat heat = new Heat();

  public void generate() {
      
    Heat heatWay = new Heat();
    System.out.println("way way");
  }
}

class Heat{
      

}
```

测试类

```
package com.msdn.java.hotspot.hsdb;

public class HeatTest {
      
  public static void main(String[] args) {
      
    Heat2 heat2 = new Heat2();
    heat2.generate();
  }
}
```

关于上述问题，我们大概都知道该怎么回答：

> heatStatic 属于静态变量，引用应该是放在方法区中，对象实例位于堆中；
> 
> heat 属于成员变量，在堆上，作为 Heat2 对象实例的属性字段；
> 
> heatWay 属于局部变量，位于 Java 线程的调用栈上。

那么如何来看看这些变量在 JVM 中是怎么存储的？这里借助 HSDB 工具来进行演示。

此处我们使用 IDEA 进行断点调试，后续会再介绍 JDB 如何进行代码调试。

IDEA 执行前需要增加 JVM 参数配置，HSDB 对 `Serial GC` 支持的较好，因此 Debug 时增加参数 `-XX:+UseSerialGC`；此外设置 Java Heap 为 10MB；UseCompressedOops 参数用来压缩 64位指针，节省内存空间。关于该参数的详细介绍，推荐阅读[本文](https://qastack.cn/programming/11054548/what-does-the-u*****pressedoops-jvm-flag-do-and-when-should-i-use-it)。

最终 JVM 参数配置如下：

```
-XX:+UseSerialGC  -Xmn10M -XX:-UseCompressedOops
```

![](https://img-blog.csdnimg.cn/img_convert/384df8e6d911da975ac193f27cdd2e61.png)

然后在 Heat2 中的 System 语句处打上断点，开始 debug 执行上述代码。

接着打开命令行窗口执行 jps 命令查看我们要调试的 Java 进程的 pid 是多少：

```
% jps
9977 HeatTest
```

接着我们按照上文讲解启动 HSDB，注意在 IDEA 中执行代码时，Java 版本为 Java9，要与 HSDB 相关的 Java 版本一致。接下来的操作步骤可以参考 R大的[文章](https://www.iteye.com/blog/rednaxelafx-1847971#comments)，或者其他相似的文章。

在 attach 成功后，选中 main线程并打开其栈信息，接着打开 console 窗口，下面我将自测的命令及结果都列举了出来，并简要介绍其作用，以及可能遇到的问题。

首先执行 help 命令，查看所有可用的命令

```
hsdb> help
Available commands:
  assert true | false
  attach pid | exec core
  buildreplayjars [ all | app | boot ]  | [ prefix ]
  detach
  dis address [length]
  disassemble address
  dumpcfg {
       -a | id }
  dumpcodecache
  dumpideal {
       -a | id }
  dumpilt {
       -a | id }
  dumpreplaydata {
       <address > | -a | <thread_id> }
  echo [ true | false ]
  examine [ address/count ] | [ address,address]
  field [ type [ name fieldtype isStatic offset address ] ]
  findpc address
  flags [ flag | -nd ]
  help [ command ]
  history
  inspect expression
  intConstant [ name [ value ] ]
  jdis address
  jhisto
  jstack [-v]
  livenmethods
  longConstant [ name [ value ] ]
  pmap
  print expression
  printall
  printas type expression
  printmdo [ -a | expression ]
  printstatics [ type ]
  pstack [-v]
  quit
  reattach
  revptrs address
  scanoops start end [ type ]
  search [ heap | perm | rawheap | codecache | threads ] value
  source filename
  symbol address
  symboldump
  symboltable name
  thread {
       -a | id }
  threads
  tokenize ...
  type [ type [ name super isOop isInteger isUnsigned size ] ]
  universe
  verbose true | false
  versioncheck [ true | false ]
  vmstructsdump
  where {
       -a | id }
  
  hsdb> where 3587
Thread 3587 Address: 0x00007fb25c00a800

Java Stack Trace for main
Thread state = BLOCKED
 - public void generate() @0x0000000116953ff8 @bci = 8, line = 15, pc = 0x0000000123cdacd7, oop = 0x000000013316f128 (Interpreted)
 - public static void main(java.lang.String[]) @0x00000001169539b0 @bci = 9, line = 11, pc = 0x0000000123caf4ba (Interpreted)

hsdb> 
```

关于上述命令的讲解可以参考[本文](https://www.cxybb.com/article/qq_31865983/98480703#10%E3%80%81where%C2%A0)。

1、universe 命令来查看GC堆的地址范围和使用情况，可以看到我们创建的三个对象都是在 eden 区。因为使用的是 Java9，所以已经不存在 Perm gen 区了，

```
hsdb> universe
Heap Parameters:
Gen 0:   eden [0x0000000132e00000,0x000000013318c970,0x0000000133600000) space capacity = 8388608, 44.36473846435547 used
  from [0x0000000133600000,0x0000000133600000,0x0000000133700000) space capacity = 1048576, 0.0 used
  to   [0x0000000133700000,0x0000000133700000,0x0000000133800000) space capacity = 1048576, 0.0 usedInvocations: 0

Gen 1:   old  [0x0000000133800000,0x0000000133800000,0x0000000142e00000) space capacity = 257949696, 0.0 usedInvocations: 0
```

不借助命令的话，还可以这样操作来查看。

![](https://img-blog.csdnimg.cn/img_convert/b04e31cb2e3ff5e3aaee25093958d0db.png)

2、scanoops 查看类型

Java 代码里，执行到 System 输出语句时应该创建了3个 Heat 的实例，它们必然在 GC 堆里，但都在哪里，可以用scanoops命令来看：

```
hsdb> scanoops 0x0000000132e00000 0x000000013318c970 com.msdn.java.hotspot.hsdb.Heat
0x000000013316f118 com/msdn/java/hotspot/hsdb/Heat
0x000000013316f140 com/msdn/java/hotspot/hsdb/Heat
0x000000013316f150 com/msdn/java/hotspot/hsdb/Heat
```

scanoops 接受两个必选参数和一个可选参数：必选参数是要扫描的地址范围，一个是起始地址一个是结束地址；可选参数用于指定要扫描什么类型的对象实例。实际扫描的时候会扫出指定的类型及其派生类的实例。

从 universe 命令返回结果可知，对象是在 eden 里分配的内存（注意used），所以执行 scanoops 命令时地址范围可以从 eden 中获取。

3、findpc 命令可以进一步知道这些对象都在 eden 之中分配给 main 线程的 thread-local allocation buffer (TLAB)中

网上的多数文章都介绍 whatis 命令，不过我个人在尝试的过程中执行该命令报错，如下述所示：

```
hsdb> whatis 0x000000012736efe8
Unrecognized command.  Try help...
```

命令不行，那么换种思路，使用 HSDB 可视化窗口来查看对象的地址信息。

![](https://img-blog.csdnimg.cn/img_convert/40f8ba72ea146a37f330759c0dfac3e7.png)

至于为什么无法使用 whatis 命令，原因是 Java9 的 HSDB 已经没有 whatis 命令了，取而代之的是 findpc 命令。

```
hsdb> findpc 0x000000013316f118
Address 0x000000013316f118: In thread-local allocation buffer for thread "main" (3587)  [0x00000001331639f8,0x000000013316f160,0x000000013318c730,{
      0x000000013318c970})
```

4、inspect命令来查看对象的内容：

```
hsdb> inspect 0x000000013316f118
instance of Oop for com/msdn/java/hotspot/hsdb/Heat @ 0x000000013316f118 @ 0x000000013316f118 (size = 16)
_mark: 1
_metadata._klass: InstanceKlass for com/msdn/java/hotspot/hsdb/Heat
```

可见一个 heatStatic 实例要16字节。因为 Heat 类没有任何 Java 层的实例字段，这里就没有任何 Java 实例字段可显示。

或者通过可视化工具来查看：

![](https://img-blog.csdnimg.cn/img_convert/44358f1bae5d0d19be222c93c499815c.png)

一个 Heat 的实例包含 2个给 VM 用的隐含字段作为对象头，和0个Java字段。

对象头的第一个字段是mark word，记录该对象的GC状态、同步状态、identity hash code之类的多种信息。

对象头的第二个字段是个类型信息指针，klass pointer。这里因为默认开启了压缩指针，所以本来应该是64位的指针存在了32位字段里。

最后还有4个字节是为了满足对齐需求而做的填充（padding）。

5、mem命令来看实际内存里的数据格式

我们执行 help 时发现已经没有 mem 命令了，那么现在只能通过 HSDB 可视化工具来获取信息。

![](https://img-blog.csdnimg.cn/img_convert/b3d5729c4e2249d06f75cee05475f076.png)

关于这块的讲解可以参考 R大的文章，文章中讲述还是使用 mem 命令，格式如下：`mem 0x000000013316f118 2`

mem 命令接受的两个参数都必选，一个是起始地址，另一个是以字宽为单位的“长度”。

虽然我们通过 inspect 命令是知道 Heat 实例有 16 字节，为什么给2暂不可知。

在实践的过程中，发现了一个类似的命令：

```
hsdb> examine 0x000000013316f118/2
0x000000013316f118: 0x0000000000000001 0x0000000116954620
```

6、revptrs 反向指针

JVM 通过引用来定位堆上的具体对象，有两种实现方式：**句柄池和直接指针**。目前 Java 默认使用的 HotSpot 虚拟机采用的便是直接指针进行对象访问的。

我们在执行 Java 程序时加了 UseCompressedOops 参数，即使不加，Java9 也会默认开启压缩指针。启用“压缩指针”的功能把64位指针压缩到只用32位来存。压缩指针与非压缩指针直接有非常简单的1对1对应关系，前者可以看作后者的特例。关于压缩指针，感兴趣的朋友可以阅读[本文](https://juejin.cn/post/6844903768077647880)。

于是我们要找 heatStatic、heat、heatWay 这三个变量，等同于找出存有指向上述3个 Heat 实例的地址的存储位置。

不嫌麻烦的话手工扫描内存去找也能找到，不过幸好HSDB内建了revptrs命令，可以找出“反向指针”——**如果a变量引用着b对象，那么从b对象出发去找a变量就是找一个“反向指针”。**

```
hsdb> revptrs 0x000000013316f118
null
Oop for java/lang/Class @ 0x000000013316d660
```

确实找到了一个 Heat 实例的指针，在一个 java.lang.Class 的实例里。

用 findpc 命令来看看这个Class对象在哪里：

```
hsdb> findpc 0x000000013316d660
Address 0x000000013316d660: In thread-local allocation buffer for thread "main" (3587)  [0x00000001331639f8,0x000000013316f160,0x000000013318c730,{
      0x000000013318c970})
```

可以看到这个 Class 对象也在 eden 里，具体来说在 main 线程的 TLAB 里。

这个 Class 对象是如何引用到 Heat 的实例的呢？再用 inspect 命令：

```
hsdb> inspect 0x000000013316d660
instance of Oop for java/lang/Class @ 0x000000013316d660 @ 0x000000013316d660 (size = 184)
<<Reverse pointers>>: 
heatStatic: Oop for com/msdn/java/hotspot/hsdb/Heat @ 0x000000013316f118 Oop for com/msdn/java/hotspot/hsdb/Heat @ 0x000000013316f118
```

可以看到，这个 Class 对象里存着 Heat 类的静态变量 heatStatic，指向着第一个 Heat 实例。注意该对象没有对象头。

静态变量按照定义存放在方法区，虽然 Java 虚拟机规范把方法区描述为堆的一个逻辑部分，但是它却有一个别名叫做 Non-Heap（非堆）。**但现在在 JDK7 的 HotSpot VM 里它实质上也被放在 Java heap 里了。可以把这种特例看作是 HotSpot VM 把方法区的一部分数据也放在 Java heap 里了。**

关于静态变量的存储位置，如果想深入研究，可以参考[本文](https://yuck1125.github.io/2019/10/17/use-HSDB-verify-Class-in-heap/)。

通过可视化工具操作也可以得到上述结果：

![](https://img-blog.csdnimg.cn/img_convert/2041207780cc3ac9d586641858969f2a.png)

最终得到同样的结果：

![](https://img-blog.csdnimg.cn/img_convert/898dea035a1036a14fbf493d828d1027.png)

同理，我们查找一下第二个变量 heat 的存储信息。

```
hsdb> revptrs 0x000000013316f140
null
Oop for com/msdn/java/hotspot/hsdb/Heat2 @ 0x000000013316f128
hsdb> findpc 0x000000013316f128
Address 0x000000013316f128: In thread-local allocation buffer for thread "main" (3587)  [0x00000001331639f8,0x000000013316f160,0x000000013318c730,{
      0x000000013318c970})
hsdb> inspect 0x000000013316f128
instance of Oop for com/msdn/java/hotspot/hsdb/Heat2 @ 0x000000013316f128 @ 0x000000013316f128 (size = 24)
<<Reverse pointers>>: 
_mark: 1
_metadata._klass: InstanceKlass for com/msdn/java/hotspot/hsdb/Heat2
heat: Oop for com/msdn/java/hotspot/hsdb/Heat @ 0x000000013316f140 Oop for com/msdn/java/hotspot/hsdb/Heat @ 0x000000013316f140
```

接着来找第三个变量 heatWay：

```
hsdb> revptrs 0x000000013316f150
null
null
```

回到我们的 HSDB 可视化界面，可以发现如下信息：

![](https://img-blog.csdnimg.cn/img_convert/63ad18c72d8d3e57ba0b0e86c2bcc499.png)

Stack Memory 窗口的内容有三栏：

-   左起第1栏是内存地址，提醒一下本文里提到“内存地址”的地方都是指虚拟内存意义上的地址，不是“物理内存地址”，不要弄混了这俩概念；
-   第2栏是该地址上存的数据，以字宽为单位
-   第3栏是对数据的注释，竖线表示范围，横线或斜线连接范围与注释文字。

仔细看会发现那个窗口里正好就有 0x000000013316f150 这数字，位于 0x00007000068e29e0 地址上，而这恰恰对应 main 线程上 generate()的栈桢。

关于静态变量、成员变量和局部变量的存储位置，我们通过上文查看 JVM 底层数据可以验证我们最初的回答。

关于成员变量 heat，还有一种验证方式。

1、首先获取 Heat2 对象的地址

```
hsdb> scanoops 0x0000000132e00000 0x000000013318c970 com.msdn.java.hotspot.hsdb.Heat2
0x000000013316f128 com/msdn/java/hotspot/hsdb/Heat2
```

2、inspect 该地址，可以看到其包含了 heat 对象。

![](https://img-blog.csdnimg.cn/img_convert/657d887f8af4af0c70b98cf36229d4bd.png)

### 问题记录

**attach 成功后搜索字节码文件，点击 create class 时报错**

```
java.io.IOException: No such file or directory
```

错误原因：权限不够

解决方案：

1、首先尝试修改 jdk/bin 文件夹的权限，尝试无效；

2、命令改为：sudo jhsdb hsdb，最后成功得到字节码文件，然后去 jdk/bin 文件夹下可以看到一个新的文件夹，里面存放了我们想要的字节码文件。

## JDB

Java调试器（JDB）是Java类在命令行中调试程序的工具。 它实现了 Java 平台调试器体系结构。 它有助于使用Java调试接口（JDI）检测和修复 Java 程序中的错误。

作为开发，我们都在本地调试过项目，一般都使用 IDEA 等工具，方便快捷。那如果没有 IDEA、Eclipse 等工具时，只给你一份代码和配置好的 Java 环境，又该如何进行调试呢？

### JDB使用

**验证 JDB 安装**

```
% jdb -version
这是jdb版本 9.0 (Java SE 版本 9.0.4)
```

**JDB 语法**

```
jdb [ options ] [ class ] [ arguments ]
```

它从 Java Development Kit 调用 jdb.exe。

-   options
    
    其中包括用于以高效方式调试Java程序的命令行选项。 JDB启动程序接受所有选项（例如-D，-classpath和-X）以及一些其他高级选项，例如（-attach，-listen，-launch等）。执行 jdb -help 可以看到各参数详细介绍，该部分最为重要。
    

![](https://img-blog.csdnimg.cn/img_convert/151c7c5aaf2d26167b3ce70317a156ec.png)

-   class
    
    在其上执行调试操作的类名。
    
-   arguments
    
    这些是在运行时为程序提供的输入值。 例如，arg [0]，arg [1]到main（）方法。
    

**调试命令**

执行 help 可以查看所有可执行的命令，介绍一下常用的命令：

1、添加断点

```
stop at com.msdn.MyClass:22
stop in com.msdn.MyClass.<init>#构造函数
stop in com.msdn.MyClass.<clinit>#静态代码块
```

需要注意的是上述语句在 windows 上可以执行，mac 上不需要包名，直接使用文件名。

2、调试

```
step                #执行当前行
step up             #一直执行, 直到当前方法返回到其调用方
stepi               #执行当前指令
next                #步进一行 (调用)
cont                #从断点处继续执行
```

3、查看变量

```
print <expr>           #输出表达式的值
dump <expr>            #输出所有对象信息
eval <expr>            #对表达式求值 (与 print 相同)
set <lvalue> = <expr>  #向字段/变量/数组元素分配新值
locals                 #输出当前堆栈帧中的所有本地变量
```

4、其他

```
list [line number|method] -- 输出源代码
use (或 sourcepath) [source file path] #显示或更改源路径(目录)
run #运行
```

**本机调试**

进入 class 文件所在目录

```
jdb -XX:+UseSerialGC  -Xmn10M com.msdn.java.hotspot.hsdb.HeatTest2 --启动jdb,可带参数
```

在 windows 上可以用上述带包名的形式进行调试。

```
jdb -XX:+UseSerialGC  -Xmn10M HeatTest
```

在 Mac 上不需要包名。

### 测试案例

注意：Java 文件不需要包名，打开命令行窗口进入该文件所在目录，然后进行之后的操作。

```
public class HeatTest2 {

  private static Test heatStatic = new Test();
  private Test heat = new Test();

  public void generate() {
    Test heatWay = new Test();
  }

  public static void main(String[] args) {
    HeatTest2 heatTest = new HeatTest2();
    heatTest.generate();
  }
}

class Test {
  private String name;
}
```

首先编译 Java 源文件，

```
javac -g HeatTest2.java
```

进入 class 文件所在目录，然后启动 JDB

```
jdb -XX:+UseSerialGC  -Xmn10M HeatTest2
> help
#执行 help 可以查看所有可执行的命令，这里就不一一列举了

> stop in HeatTest2.main
正在延迟断点HeatTest2.main。
将在加载类后设置。
> run
运行HeatTest2
设置未捕获的java.lang.Throwable
设置延迟的未捕获的java.lang.Throwable
> 
VM 已启动: 设置延迟的断点HeatTest2.main

断点命中: "线程=main", HeatTest2.main(), 行=19 bci=0
19        HeatTest2 heatTest = new HeatTest2();

main[1] step
> 
已完成的步骤: "线程=main", HeatTest2.<init>(), 行=8 bci=0
8    public class HeatTest2 {

main[1] step
> 
已完成的步骤: "线程=main", HeatTest2.<init>(), 行=11 bci=4
11      private Test heat = new Test();

main[1] next
> 
已完成的步骤: "线程=main", HeatTest2.main(), 行=19 bci=7
19        HeatTest2 heatTest = new HeatTest2();

main[1] next
> 
已完成的步骤: "线程=main", HeatTest2.main(), 行=20 bci=8
20        heatTest.generate();

main[1] next
> 
已完成的步骤: "线程=main", HeatTest2.main(), 行=21 bci=12
21      }

main[1] next
> 
应用程序已退出
```

如果我们想要在 generate 方法中设置断点，则可以这样做：

```
> stop at HeatTest2:14
正在延迟断点HeatTest2:14。
将在加载类后设置。
> run
运行HeatTest2
设置未捕获的java.lang.Throwable
设置延迟的未捕获的java.lang.Throwable
> 
VM 已启动: 设置延迟的断点HeatTest2:14

断点命中: "线程=main", HeatTest2.generate(), 行=14 bci=0
14        Test heatWay = new Test();

main[1] next
> 
已完成的步骤: "线程=main", HeatTest2.generate(), 行=15 bci=8
15      }

main[1] next
> 
已完成的步骤: "线程=main", HeatTest2.main(), 行=21 bci=12
21      }

main[1] next
> 
应用程序已退出
```

### JVM系列阅读

[JVM系列之：你真的了解垃圾回收吗](https://heapdump.cn/article/3611733)

[JVM系列之：JVM是如何创建对象的](https://heapdump.cn/article/3606298)

[JVM系列之：JVM是怎么实现invokedynamic的？](https://heapdump.cn/article/3573623)

[JVM系列之：关于方法句柄的那些事](https://heapdump.cn/article/3543046)

[JVM系列之：关于HSDB的一点心得](https://heapdump.cn/article/3535254)

[JVM系列之：JVM是如何实现反射的](https://heapdump.cn/article/3530561)

[JVM系列之：关于JVM类加载的那些事](https://heapdump.cn/article/3522792)

[JVM系列之：聊一聊Java异常](https://heapdump.cn/article/3519925)

[JVM系列之：JVM如何执行方法调用](https://heapdump.cn/article/3504428)

[JVM系列之：聊聊Java的数据类型](https://heapdump.cn/article/3490433)

[JVM系列之：宏观分析Java代码是如何执行的](https://heapdump.cn/article/3489687)

## 参考文献

[JDB - 快速指南](https://iowiki.com/jdb/jdb_quick_guide.html)

[mac下在终端使用jdb调试java代码](https://blog.csdn.net/japanstudylang/article/details/97372192)

[聊聊openjdk的jhsdb工具](https://juejin.cn/post/6844903808057753613)

[解读HSDB](https://www.jianshu.com/p/f6f9b14d5f21)
