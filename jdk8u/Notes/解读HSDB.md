---
source: https://www.jianshu.com/p/f6f9b14d5f21
---
HSDB（Hotspot Debugger)，是一款内置于 SA 中的 GUI 调试工具，可用于调试 JVM 运行时数据，从而进行故障排除

`启动HSDB`  
检测不同 JDK 版本需要使用不同的 HSDB 版本，否则容易出现无法扫描到对象等莫名其妙的问题

-   Mac：JDK7 和 JDK8 均可以采用以下的方式

```
$ sudo java -cp ,:/Library/Java/JavaVirtualMachines/jdk1.7.0_80.jdk/Contents/Home/lib/sa-jdi.jar sun.jvm.hotspot.HSDB
```

发现在我的mac上jdk版本各种报错，最后无解，直接安装了jdk11, [mac上配置多版本jdk](https://www.jianshu.com/p/723a7f41bfa9)  
而 JDK11 的启动方式有些区别

```
$/Library/Java/JavaVirtualMachines/jdk-11.0.6.jdk/Contents/Home/bin/jhsdb hsdb
```

其中启动版本可以使用 /usr/libexec/java_home -V 获取  
HSDB 对 Serial GC 支持的较好，因此 Debug 时增加参数 -XX:+UseSerialGC。**注意运行程序java的版本和hsdb的java版本要一致才行**。

`获取应用进程id`  
jps 仅查找当前用户的 Java 进程，而不是当前系统中的所有进程

-   默认显示 pid 以及 main 方法对应的 class 名称
-   -v：输出传递给 JVM 的参数
-   -l： 输出 main 方法对应的 class 的完整 package 名

`CLHSDB常用指令`  
universe：查看堆空间信息  
scanoops start end [type]：扫描指定空间中的 type 类型及其子类的实例

> JDK8 版本的 HSDB 的 scanoops 会无法扫描到对象，但可以通过 GUI 界面的 Tools -> Object Histogram，输入想要查询的对象，之后双击来获取对象的地址，也可以继续在里面点击 inspect 来查看对象信息

-   inspect：查看对象（OOP）信息【使用 tools->inspect，输入对象地址有更详细的信息哦】

**HSDB GUI界面**  
`可视化线程栈`  

![](https://upload-images.jianshu.io/upload_images/11345047-665db91daa533924.png?imageMogr2/auto-orient/strip|imageView2/2/w/1200/format/webp)

image.png

`对象直方图`  
Tools -> Object Histogram，我们可以通过对象直方图快速定位某个类型的对象的地址以供我们进一步分析

![](https://upload-images.jianshu.io/upload_images/11345047-e89510913a9c603d.png?imageMogr2/auto-orient/strip|imageView2/2/w/1200/format/webp)

image.png

![](https://upload-images.jianshu.io/upload_images/11345047-bd223c7bdfd5b3d2.png?imageMogr2/auto-orient/strip|imageView2/2/w/1200/format/webp)

image.png

`OOP信息`  
我们可以根据对象地址在 Tools -> Inspector 获取对象的在 JVM 层的实例 instanceOopDesc 对象，它包括对象头 _mark 和 _metadata 以及实例信息

![](https://upload-images.jianshu.io/upload_images/11345047-11d4e6102a760a4d.png?imageMogr2/auto-orient/strip|imageView2/2/w/1200/format/webp)

image.png

`元数据区`  
HotSpot VM 里有一套对象专门用来存放元数据，它们包括：

-   Klass 系对象，用于描述类型的总体信息【通过 OOP 信息（inspect）可以看到 instanceKlass 对象】
-   ConstantPool/ConstantPoolCache 对象：每个 InstanceKlass 关联着一个 ConstantPool，作为该类型的运行时常量池。这个常量池的结构跟 Class 文件里的常量池基本上是对应的

![](https://upload-images.jianshu.io/upload_images/11345047-3072833ce5c13341.png?imageMogr2/auto-orient/strip|imageView2/2/w/1200/format/webp)

image.png

-   Method 对象，用来描述 Java 方法的总体信息，如方法入口地址、调用/循环计数器等等
    -   ConstMethod 对象，记录着 Java 方法的不变的描述信息，包括方法名、方法的访问修饰符、字节码、行号表、局部变量表等等。注意，字节码指令被分配在 constMethodOop 对象的内存区域的末尾
        -   MethodData 对象，记录着 Java 方法执行时的 profile 信息，例如某方法里的某个字节码之类是否从来没遇到过 null，某个条件跳转是否总是走同一个分支，等等。这些信息在解释器（多层编译模式下也在低层的编译生成的代码里）收集，然后供给 HotSpot Server Compiler 用于做激进优化。

![](https://upload-images.jianshu.io/upload_images/11345047-2c03adfab5c6833f.png)

image.png

![](https://upload-images.jianshu.io/upload_images/11345047-112239b1e64af95d.png)

image.png

-   Symbol 对象，对应 Class 文件常量池里的 JVM_CONSTANT_Utf8 类型的常量。有一个 VM 全局的 SymbolTable 管理着所有 Symbol。Symbol 由所有 Java 类所共享。

#### 实例分析

```
public class Test {  
    static Test2 t1 = new Test2();  
           Test2 t2 = new Test2();  
    public void fn() {  
        Test2 t3 = new Test2();       
    }  
}  
  
class Test2 {  
  
}  
```

这个程序的t1、t2、t3三个变量本身（而不是这三个变量所指向的对象）到底在哪里。

```
t1在存Java静态变量的地方，概念上在JVM的方法区（method area）里
t2在Java堆里，作为Test的一个实例的字段存在
t3在Java线程的调用栈里，作为Test.fn()的一个局部变量存在
```

不过就这么简单的回答大家都会，满足不了对JVM的实现感兴趣的同学们的好奇心。说到底，这“方法区”到底是啥？Java堆在哪里？Java线程的调用栈又是啥样的？

那就让我们跑点例子，借助调试器来看看在一个实际运行中的JVM里是啥状况。  
写个启动类来跑上面问题中的代码：

```
public class Main {  
    public static void main(String[] args) {  
        Test test = new Test();  
        test.fn();  
    }  
}  
```

Serviceability Agent是个非常便于探索HotSpot VM内部实现的API, 而HSDB则是在SA基础上包装起来的一个调试器。这次我们就用HSDB来做实验。 SA的一个限制是它只实现了调试snapshot的功能：要么要让被调试的目标进程完全暂停，要么就调试core dump。所以我们在用HSDB做实验前，得先让我们的Java程序运行到我们关注的点上才行。 理想情况下我们会希望让这Java程序停在Test.java的第6行，也就是Test.fn()中t3局部变量已经进入作用域，而该方法又尚未返回的地方。怎样才能停在这里呢？

其实用个Java层的调试器即可。大家平时可能习惯了在Eclipse、IntelliJ IDEA、NetBeans等Java IDE里使用Java层调试器，但为了减少对外部工具的依赖，本文将使用Oracle JDK自带的jdb工具来完成此任务。

jdb跟上面列举的IDE里包含的调试器底下依赖着同一套调试API，也就是Java Platform Debugger Architecture (JPDA)功能也类似，只是界面是命令行的，表明上看起来不太一样而已。

为了方便后续步骤，启动jdb的时候可以设定让目标Java程序使用serial GC和10MB的Java heap。  
启动jdb之后可以用stop in命令在指定的Java方法入口处设置断点，  
然后用run命令指定主类名称来启动Java程序，  
等跑到断点看看位置是否已经到满足需求，还没到的话可以用step、next之类的命令来向前进。  
具体步骤如下：

```
> stop in jvm.hsdb.Test.fn
正在延迟断点jvm.hsdb.Test.fn。
将在加载类后设置。
> run jvm.hsdb.Main
运行 jvm.hsdb.Main
设置未捕获的java.lang.Throwable
设置延迟的未捕获的java.lang.Throwable
> 
VM 已启动: 设置延迟的断点jvm.hsdb.Test.fn

断点命中: "线程=main", jvm.hsdb.Test.fn(), 行=7 bci=0
7            Test2 t3 = new Test2();       

main[1] next
> 
已完成的步骤: "线程=main", jvm.hsdb.Test.fn(), 行=8 bci=8
8        }  

main[1] 

```

按照上述步骤执行完最后一个next命令之后，我们就来到了最初想要的Test.java的第8行，也就是Test.fn()返回前的位置。

接下来把这个jdb窗口放一边，另开一个命令行窗口用jps命令看看我们要调试的Java进程的pid是多少： 4981

```
4994 SALauncher
5266 Jps
4981 Main
1734 Launcher
4972 TTY
```

然后启动HSDB：  
$/Library/Java/JavaVirtualMachines/jdk-11.0.6.jdk/Contents/Home/bin/jhsdb hsdb

启动HSDB之后，把它连接到目标进程上。从菜单里选择File -> Attach to HotSpot process：

在弹出的对话框里输入刚才记下的pid然后按OK：

  

![](https://upload-images.jianshu.io/upload_images/11345047-c5e94b6742033552.png)

image.png

这会儿就连接到目标进程了：

  

![](https://upload-images.jianshu.io/upload_images/11345047-a1d380ec47096921.png)

image.png

刚开始打开的窗口是Java Threads，里面有个线程列表。双击代表线程的行会打开一个Oop Inspector窗口显示HotSpot VM里记录线程的一些基本信息的C++对象的内容。  
不过这里我们更可能会关心的是线程栈的内存数据。先选择main线程，然后点击Java Threads窗口里的工具栏按钮从左数第2个可以打开Stack Memory窗口来显示main线程的栈：

  

![](https://upload-images.jianshu.io/upload_images/11345047-2ca920ac2ab92c16.png)

image.png

Stack Memory窗口的内容有三栏：  
左起第1栏是内存地址，请让我提醒一下本文里提到“内存地址”的地方都是指虚拟内存意义上的地址，不是“物理内存地址”，请不要弄混了这俩概念；  
第2栏是该地址上存的数据，以字宽为单位  
第3栏是对数据的注释，竖线表示范围，横线或斜线连接范围与注释文字。

现在让我们打开HSDB里的控制台，以便用命令来了解更多信息。  
在菜单里选择Windows -> Console：

  

![](https://upload-images.jianshu.io/upload_images/11345047-d34282c58f2cfb57.png)

image.png

然后会得到一个空白的Command Line窗口。在里面敲一下回车就会出现hsdb>提示符。

（用过CLHSDB的同学可能会发现这就是把CLHSDB嵌入在了HSDB的图形界面里）

不知道有什么命令可用的同学可以先用help命令看看命令列表。

可以用universe命令来查看GC堆的地址范围和使用情况：

```
sdb> universe
Heap Parameters:
Gen 0:   eden [0x00000007ff600000,0x00000007ff6c4de8,0x00000007ff8b0000) space capacity = 2818048, 28.61470067223837 used
  from [0x00000007ff8b0000,0x00000007ff8b0000,0x00000007ff900000) space capacity = 327680, 0.0 used
  to   [0x00000007ff900000,0x00000007ff900000,0x00000007ff950000) space capacity = 327680, 0.0 usedInvocations: 0

Gen 1:   old  [0x00000007ff950000,0x00000007ff950000,0x0000000800000000) space capacity = 7012352, 0.0 usedInvocations: 0
```

在我们的Java代码里，执行到Test.fn()末尾为止应该创建了3个Test2的实例。它们必然在GC堆里，但都在哪里呢？用scanoops命令来看：

```

hsdb> scanoops 0x00000007ff600000 0x00000007ff6c4de8 jvm.hsdb.Test2
0x00000007ff6b42d0 jvm/hsdb/Test2
0x00000007ff6b42f0 jvm/hsdb/Test2
0x00000007ff6b4300 jvm/hsdb/Test2
hsdb> 
```

scanoops接受两个必选参数和一个可选参数：必选参数是要扫描的地址范围，一个是起始地址一个是结束地址；可选参数用于指定要扫描什么类型的对象实例。实际扫描的时候会扫出指定的类型及其派生类的实例。

这里可以看到确实扫出了3个Test2的实例。内容有两列：左边是对象的起始地址，右边是对象的实际类型。 从它们所在的地址，对照前面universe命令看到的GC堆的地址范围，可以知道它们都在eden里。

还可以用inspect命令来查看对象的内容：

```
hsdb> inspect 0x00000007ff6b42d0
instance of Oop for jvm/hsdb/Test2 @ 0x00000007ff6b42d0 @ 0x00000007ff6b42d0 (size = 16)
_mark: 5
_metadata._compressed_klass: InstanceKlass for jvm/hsdb/Test2
hsdb> 
```

可见一个Test2的实例要16字节。因为Test2类没有任何Java层的实例字段，这里就没有任何Java实例字段可显示。

还想看到更裸的数据的同学可以在MemoryViewer来看实际内存里的数据长啥样：

  

![](https://upload-images.jianshu.io/upload_images/11345047-7cba18e261e2aa64.png)

image.png

上面的数字都是啥来的呢？

0x00000007ff6b42d0: _mark: 0x0000000000000001  
0x00000007ff6b42d8: _metadata._compressed_klass: 0x0000000000060460

一个Test2的实例包含2个给VM用的隐含字段作为对象头，和0个Java字段。  
对象头的第一个字段是mark word，记录该对象的GC状态、同步状态、identity hash code之类的多种信息。  
对象头的第二个字段是个类型信息指针，klass pointer。

顺带发张Inspector的截图来展示HotSpot VM里描述Test2类的VM对象长啥样吧。

在菜单里选Tools -> Inspector，在地址里输入前面看到的klass地址：

  

![](https://upload-images.jianshu.io/upload_images/11345047-645df40f8fe9159f.png)

image.png

InstanceKlass存着Java类型的名字、继承关系、实现接口关系，字段信息，方法信息，运行时常量池的指针，还有内嵌的虚方法表（vtable）、接口方法表（itable）和记录对象里什么位置上有GC会关心的指针（oop map）等等。

留意到这个InstanceKlass是给VM内部用的，并不直接暴露给Java层；InstanceKlass不是java.lang.Class的实例。

在HotSpot VM里，java.lang.Class的实例被称为“Java mirror”，意思是它是VM内部用的klass对象的“镜像”，把klass对象包装了一层来暴露给Java层使用。  
在InstanceKlass里有个_java_mirror字段引用着它对应的Java mirror，而mirror里也有个隐藏字段指向其对应的InstanceKlass。

所以当我们写obj.getClass()，在HotSpot VM里实际上经过了两层间接引用才能找到最终的Class对象：

```
obj->_klass->_java_mirror  
```

前面对HSDB的操作和HotSpot VM里的一些内部数据结构有了一定的了解，现在让我们回到主题：找指针！

于是我们要找t1、t2、t3这三个变量，等同于找出存有指向上述3个Test2实例的地址的存储位置。

不嫌麻烦的话手工扫描内存去找也能找到，不过幸好HSDB内建了revptrs命令，可以找出“反向指针”——如果a变量引用着b对象，那么从b对象出发去找a变量就是找一个“反向指针”。

先拿第一个Test2的实例试试看：

```
hsdb> revptrs 0x00000007ff6b42d0
Computing reverse pointers...
Done.
null
Oop for java/lang/Class @ 0x00000007ff6b3440
```

还真的找到了一个包含指向Test2实例的指针，在一个java.lang.Class的实例里。  
可以看到这个Class对象也在eden里，具体来说在main线程的TLAB里。  
这个Class对象是如何引用到Test2的实例的呢？再用inspect命令：

```
inspect 0x00000007ff6b3440
instance of Oop for java/lang/Class @ 0x00000007ff6b3440 @ 0x00000007ff6b3440 (size = 120)
<<Reverse pointers>>: 
t1: Oop for jvm/hsdb/Test2 @ 0x00000007ff6b42d0 Oop for jvm/hsdb/Test2 @ 0x00000007ff6b42d0
```

可以看到，这个Class对象里存着Test类的静态变量t1，指向着第一个Test2实例。

成功找到t1了！这个有点特别，本来JVM规范里也没明确规定静态变量要存在哪里，通常认为它应该在概念中的“方法区”里；但现在在JDK11的HotSpot VM里它实质上也被放在Java heap里了。可以把这种特例看作是HotSpot VM把方法区的一部分数据也放在Java heap里了。

再接再厉，用revptrs看看第二个Test2实例有谁引用：

```
revptrs 0x00000007ff6b42f0
Oop for jvm/hsdb/Test @ 0x00000007ff6b42e0
hsdb> inspect 0x00000007ff6b42e0
instance of Oop for jvm/hsdb/Test @ 0x00000007ff6b42e0 @ 0x00000007ff6b42e0 (size = 16)
<<Reverse pointers>>: 
_mark: 5
_metadata._compressed_klass: InstanceKlass for jvm/hsdb/Test
t2: Oop for jvm/hsdb/Test2 @ 0x00000007ff6b42f0 Oop for jvm/hsdb/Test2 @ 0x00000007ff6b42f0
hsdb> 
```

可以看到这个Test实例里有个成员字段t2，指向了第二个Test2实例。  
于是t2也找到了！在Java堆里，作为Test的实例的成员字段存在。

那么赶紧试试用revptrs命令看第三个Test2实例：

```
revptrs 0x00000007ff6b4300
null
```

啥？没找到？！SA这也太弱小了吧。明明就在那里…  
0x00000007ff6b4300 回到前面打开的Stack Memory窗口看，仔细看会发现那个窗口里正好就有0x00000007ff6b4300这数字，

  

![](https://upload-images.jianshu.io/upload_images/11345047-30946f3f0514ed3a.png)

image.png

如果图里看得不清楚的话，我再用文字重新写一遍（两道横线之间的是Test.fn()的栈帧内容，前后的则是别的东西）：

```
-------------------------------------------------------------------------------------------------------------  
Stack frame for Test.fn() @bci=8, line=6, pc=0x0000000002893ca5, methodOop=0x00000000fb077f78 (Interpreted frame)  
0x000000000287f808: 0x000000000287f808 expression stack bottom          <- rsp  
0x000000000287f810: 0x00000000fb077f58 bytecode pointer    = 0x00000000fb077f50 (base) + 8 (bytecode index) in PermGen  
0x000000000287f818: 0x000000000287f860 pointer to locals  
0x000000000287f820: 0x00000000fb078360 constant pool cache = ConstantPoolCache for Test in PermGen  
0x000000000287f828: 0x0000000000000000 method data oop     = null  
0x000000000287f830: 0x00000000fb077f78 method oop          = Method for Test.fn()V in PermGen  
0x000000000287f838: 0x0000000000000000 last Java stack pointer (not set)  
0x000000000287f840: 0x000000000287f860 old stack pointer (saved rsp)  
0x000000000287f848: 0x000000000287f8a8 old frame pointer (saved rbp)    <- rbp  
0x000000000287f850: 0x0000000002886298 return address      = in interpreter codelet "return entry points" [0x00000000028858b8, 0x00000000028876c0)  7688 bytes  
0x000000000287f858: 0x00000000fa49a740 local[1] "t3"       = Oop for Test2 in NewGen  
0x000000000287f860: 0x00000000fa49a720 local[0] "this"     = Oop for Test in NewGen  
-------------------------------------------------------------------------------------------------------------  
0x000000000287f868: 0x000000000287f868   
0x000000000287f870: 0x00000000fb077039   
0x000000000287f878: 0x000000000287f8c0   
0x000000000287f880: 0x00000000fb077350   
0x000000000287f888: 0x0000000000000000   
0x000000000287f890: 0x00000000fb077060   
0x000000000287f898: 0x000000000287f860   
0x000000000287f8a0: 0x000000000287f8c0   
0x000000000287f8a8: 0x000000000287f9a0   
0x000000000287f8b0: 0x000000000288062a   
0x000000000287f8b8: 0x00000000fa49a720   
0x000000000287f8c0: 0x00000000fa498ea8   
0x000000000287f8c8: 0x0000000000000000   
0x000000000287f8d0: 0x0000000000000000   
0x000000000287f8d8: 0x0000000000000000   
```

回顾JVM规范里所描述的Java栈帧结构，包括：

[ 操作数栈 (operand stack) ]  
[ 栈帧信息 (dynamic linking) ]  
[ 局部变量区 (local variables) ]

  

![](https://upload-images.jianshu.io/upload_images/11345047-1c0491096f8692ae.png)

image.png

再跟HotSpot VM的解释器所使用的栈帧布局对比看看，是不是正好能对应上？局部变量区（locals）有了，VM所需的栈帧信息也有了；执行到这个位置operand stack正好是空的所以看不到它。  
（HotSpot VM里把operand stack叫做expression stack。这是因为operand stack通常只在表达式求值过程中才有内容）  
从Test.fn()的栈帧中我们可以看到t3变量就在locals[1]的位置上。t3变量也找到了！大功告成！
