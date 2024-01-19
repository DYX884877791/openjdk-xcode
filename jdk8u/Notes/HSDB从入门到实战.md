---
source: https://zzcoder.cn/2019/12/06/HSDB%E4%BB%8E%E5%85%A5%E9%97%A8%E5%88%B0%E5%AE%9E%E6%88%98/
---
`HSDB（Hotspot Debugger)`，是一款内置于 SA 中的 GUI 调试工具，可用于调试 JVM 运行时数据，从而进行故障排除

## [](https://zzcoder.cn/2019/12/06/HSDB%E4%BB%8E%E5%85%A5%E9%97%A8%E5%88%B0%E5%AE%9E%E6%88%98/#%E5%90%AF%E5%8A%A8HSDB "启动HSDB")启动HSDB

检测不同 JDK 版本需要使用不同的 `HSDB` 版本，否则容易出现无法扫描到对象等莫名其妙的问题

-   **Mac**：JDK7 和 JDK8 均可以采用以下的方式
    
    <table><tbody><tr><td><pre><span>1</span><br></pre></td><td><pre><span>$ sudo java -cp ,:/Library/Java/JavaVirtualMachines/jdk1.7.0_80.jdk/Contents/Home/lib/sa-jdi.jar sun.jvm.hotspot.HSDB</span><br></pre></td></tr></tbody></table>
    
    > 事实上经过测试，即使通过 JDK8 自带的 `sa-jdi.jar` 去扫描对象（`scanoops`）的时候也会发生扫不到的情况，但可以通过其他手段代替
    
    而 JDK11 的启动方式有些区别
    
    <table><tbody><tr><td><pre><span>1</span><br></pre></td><td><pre><span>$ /Library/Java/JavaVirtualMachines/jdk-11.0.1.jdk/Contents/Home/bin/jhsdb hsdb</span><br></pre></td></tr></tbody></table>
    
    > 事实上经过测试，该版本启动的 `HSDB` 会少支持一些指令（比如 `mem, whatis`），**因此目前不推荐使用该版本**
    
-   **Windows**:
    
    <table><tbody><tr><td><pre><span>1</span><br></pre></td><td><pre><span>$ java -classpath <span>"%JAVA_HOME%/lib/sa-jdi.jar"</span> sun.jvm.hotspot.HSDB</span><br></pre></td></tr></tbody></table>
    

其中启动版本可以使用 `/usr/libexec/java_home -V` 获取

> 若遇到 Unable to locate an executable at “/Users/xx/.jenv/versions/1.7/bin/jhsdb” (-1) 可通过 `Jenv` 切换到当前 Jdk 版本即可解决

## [](https://zzcoder.cn/2019/12/06/HSDB%E4%BB%8E%E5%85%A5%E9%97%A8%E5%88%B0%E5%AE%9E%E6%88%98/#JVM%E5%8F%82%E6%95%B0%E8%AE%BE%E7%BD%AE "JVM参数设置")JVM参数设置

`HSDB` 对 `Serial GC` 支持的较好，因此 Debug 时增加参数 `-XX:+UseSerialGC`，Debug 工具可以使用 IDE 或 JDB

## [](https://zzcoder.cn/2019/12/06/HSDB%E4%BB%8E%E5%85%A5%E9%97%A8%E5%88%B0%E5%AE%9E%E6%88%98/#%E8%8E%B7%E5%8F%96%E5%BA%94%E7%94%A8%E8%BF%9B%E7%A8%8Bid "获取应用进程id")获取应用进程id

jps 仅查找当前用户的 Java 进程，而不是当前系统中的所有进程

<table><tbody><tr><td><pre><span>1</span><br></pre></td><td><pre><span>$ jps</span><br></pre></td></tr></tbody></table>

-   默认**显示 pid** 以及 **main 方法对应的 class 名称**
-   -v：**输出传递给 JVM 的参数**
-   -l： **输出 main 方法对应的 class 的完整 package 名**

## [](https://zzcoder.cn/2019/12/06/HSDB%E4%BB%8E%E5%85%A5%E9%97%A8%E5%88%B0%E5%AE%9E%E6%88%98/#CLHSDB%E5%B8%B8%E7%94%A8%E6%8C%87%E4%BB%A4 "CLHSDB常用指令")CLHSDB常用指令

-   `universe`：查看堆空间信息
    
-   `scanoops start end [type]`：扫描指定空间中的 type 类型及其子类的实例
    
    > JDK8 版本的 `HSDB` 的 `scanoops` 会无法扫描到对象，但可以通过 GUI 界面的 `Tools -> Object Histogram`，输入想要查询的对象，之后双击来获取对象的地址，也可以继续在里面点击 `inspect` 来查看对象信息
    
-   `inspect`：查看对象（`OOP`）信息【使用 `tools->inspect`，输入对象地址有更详细的信息哦】
    
-   `revptrs`：反向指针，查找引用该对象的指针
    

## [](https://zzcoder.cn/2019/12/06/HSDB%E4%BB%8E%E5%85%A5%E9%97%A8%E5%88%B0%E5%AE%9E%E6%88%98/#HSDB-GUI%E7%95%8C%E9%9D%A2 "HSDB GUI界面")HSDB GUI界面

### [](https://zzcoder.cn/2019/12/06/HSDB%E4%BB%8E%E5%85%A5%E9%97%A8%E5%88%B0%E5%AE%9E%E6%88%98/#%E5%8F%AF%E8%A7%86%E5%8C%96%E7%BA%BF%E7%A8%8B%E6%A0%88 "可视化线程栈")可视化线程栈

[![](https://zzcoder.oss-cn-hangzhou.aliyuncs.com/jvm/HSDB-01.png)](https://zzcoder.oss-cn-hangzhou.aliyuncs.com/jvm/HSDB-01.png)

### [](https://zzcoder.cn/2019/12/06/HSDB%E4%BB%8E%E5%85%A5%E9%97%A8%E5%88%B0%E5%AE%9E%E6%88%98/#%E5%AF%B9%E8%B1%A1%E7%9B%B4%E6%96%B9%E5%9B%BE "对象直方图")对象直方图

`Tools -> Object Histogram`，我们可以通过对象直方图快速定位某个类型的对象的地址以供我们进一步分析

[![](https://zzcoder.oss-cn-hangzhou.aliyuncs.com/jvm/HSDB-02.png)](https://zzcoder.oss-cn-hangzhou.aliyuncs.com/jvm/HSDB-02.png)

[![](https://zzcoder.oss-cn-hangzhou.aliyuncs.com/jvm/HSBD-03.png)](https://zzcoder.oss-cn-hangzhou.aliyuncs.com/jvm/HSBD-03.png)

### [](https://zzcoder.cn/2019/12/06/HSDB%E4%BB%8E%E5%85%A5%E9%97%A8%E5%88%B0%E5%AE%9E%E6%88%98/#OOP%E4%BF%A1%E6%81%AF "OOP信息")OOP信息

我们可以根据对象地址在 `Tools -> Inspector` 获取对象的在 JVM 层的实例 `instanceOopDesc` 对象，它包括对象头 `_mark` 和 `_metadata` 以及实例信息

[![](https://zzcoder.oss-cn-hangzhou.aliyuncs.com/jvm/HSDB-04.png)](https://zzcoder.oss-cn-hangzhou.aliyuncs.com/jvm/HSDB-04.png)

### [](https://zzcoder.cn/2019/12/06/HSDB%E4%BB%8E%E5%85%A5%E9%97%A8%E5%88%B0%E5%AE%9E%E6%88%98/#%E5%A0%86%E4%BF%A1%E6%81%AF "堆信息")堆信息

我们可以通过 `Tools -> Heap Parameters` 获取堆信息，可以结合对象地址判断对象位置

[![](https://zzcoder.oss-cn-hangzhou.aliyuncs.com/jvm/HSDB-05.png)](https://zzcoder.oss-cn-hangzhou.aliyuncs.com/jvm/HSDB-05.png)

### [](https://zzcoder.cn/2019/12/06/HSDB%E4%BB%8E%E5%85%A5%E9%97%A8%E5%88%B0%E5%AE%9E%E6%88%98/#%E5%8A%A0%E8%BD%BD%E7%B1%BB%E5%88%97%E8%A1%A8 "加载类列表")加载类列表

我们可以通过 `Tools -> Class Browser` 来获取所有加载类列表

[![](https://zzcoder.oss-cn-hangzhou.aliyuncs.com/jvm/HSDB-06.png)](https://zzcoder.oss-cn-hangzhou.aliyuncs.com/jvm/HSDB-06.png)

#### [](https://zzcoder.cn/2019/12/06/HSDB%E4%BB%8E%E5%85%A5%E9%97%A8%E5%88%B0%E5%AE%9E%E6%88%98/#%E5%85%83%E6%95%B0%E6%8D%AE%E5%8C%BA "元数据区")元数据区

HotSpot VM 里有一套对象专门用来存放元数据，它们包括：

-   `Klass` 系对象，用于描述类型的总体信息【**通过 `OOP` 信息（`inspect`）可以看到 `instanceKlass` 对象**】
    
-   `ConstantPool/ConstantPoolCache` 对象：每个 `InstanceKlass` 关联着一个 `ConstantPool`，作为该类型的运行时常量池。这个常量池的结构跟 Class 文件里的常量池基本上是对应的
    
    [![](https://zzcoder.oss-cn-hangzhou.aliyuncs.com/jvm/HSDB-07.png)](https://zzcoder.oss-cn-hangzhou.aliyuncs.com/jvm/HSDB-07.png)
    
    [![](https://zzcoder.oss-cn-hangzhou.aliyuncs.com/jvm/HSDB-08.png)](https://zzcoder.oss-cn-hangzhou.aliyuncs.com/jvm/HSDB-08.png)
    
-   `Method` 对象，用来描述 Java 方法的总体信息，如方法入口地址、调用/循环计数器等等
    
    -   `ConstMethod` 对象，记录着 Java 方法的不变的描述信息，包括方法名、方法的访问修饰符、**字节码**、行号表、局部变量表等等。**注意，字节码指令被分配在 `constMethodOop` 对象的内存区域的末尾**
    -   `MethodData` 对象，记录着 Java 方法执行时的 profile 信息，例如某方法里的某个字节码之类是否从来没遇到过 null，某个条件跳转是否总是走同一个分支，等等。这些信息在解释器（多层编译模式下也在低层的编译生成的代码里）收集，然后供给 HotSpot Server Compiler 用于做激进优化。
    
    [![](https://zzcoder.oss-cn-hangzhou.aliyuncs.com/jvm/HSDB-09.png)](https://zzcoder.oss-cn-hangzhou.aliyuncs.com/jvm/HSDB-09.png)
    
    [![](https://zzcoder.oss-cn-hangzhou.aliyuncs.com/jvm/HSDB-10.png)](https://zzcoder.oss-cn-hangzhou.aliyuncs.com/jvm/HSDB-10.png)
    
-   `Symbol` 对象，对应 Class 文件常量池里的 `JVM_CONSTANT_Utf8` 类型的常量。有一个 VM 全局的 `SymbolTable` 管理着所有 `Symbol`。`Symbol` 由所有 Java 类所共享。
    

#### [](https://zzcoder.cn/2019/12/06/HSDB%E4%BB%8E%E5%85%A5%E9%97%A8%E5%88%B0%E5%AE%9E%E6%88%98/#%E7%94%9F%E6%88%90class%E6%96%87%E4%BB%B6 "生成class文件")生成class文件

到对应类下点击 create .class 后就可以在执行 HSDB 的目录下看到生成的 class文件，适合查看动态代理生成的字节码

## [](https://zzcoder.cn/2019/12/06/HSDB%E4%BB%8E%E5%85%A5%E9%97%A8%E5%88%B0%E5%AE%9E%E6%88%98/#%E5%AE%9E%E6%88%98 "实战")实战

### [](https://zzcoder.cn/2019/12/06/HSDB%E4%BB%8E%E5%85%A5%E9%97%A8%E5%88%B0%E5%AE%9E%E6%88%98/#%E5%88%86%E6%9E%90%E5%AF%B9%E8%B1%A1%E5%AD%98%E5%82%A8%E5%8C%BA%E5%9F%9F "分析对象存储区域")分析对象存储区域

下面代码中的静态变量，成员变量分别存储在什么地方呢？

<table><tbody><tr><td><pre><span>1</span><br><span>2</span><br><span>3</span><br><span>4</span><br><span>5</span><br><span>6</span><br><span>7</span><br><span>8</span><br><span>9</span><br><span>10</span><br><span>11</span><br><span>12</span><br><span>13</span><br><span>14</span><br><span>15</span><br><span>16</span><br><span>17</span><br><span>18</span><br><span>19</span><br><span>20</span><br><span>21</span><br></pre></td><td><pre><span><span>public</span> <span><span>class</span> <span>Main</span> </span>{</span><br><span></span><br><span>    <span>private</span> <span>static</span> VMShow StaticVmShow = <span>new</span> VMShow();</span><br><span>    <span>private</span> VMShow objVmShow = <span>new</span> VMShow();</span><br><span></span><br><span>    <span><span>public</span> <span>static</span> <span>void</span> <span>main</span><span>(String[] args)</span> </span>{</span><br><span>        fn();</span><br><span>    }</span><br><span></span><br><span>    <span><span>private</span> <span>static</span> VMShow <span>fn</span><span>()</span></span>{</span><br><span>        <span>return</span> <span>new</span> VMShow();</span><br><span>    }</span><br><span>}</span><br><span></span><br><span><span><span>class</span> <span>VMShow</span> </span>{</span><br><span>    <span>private</span> <span>int</span> basicInt = <span>1</span>;</span><br><span>    <span>private</span> Integer objInt = <span>2</span>;</span><br><span>    <span>private</span> <span>static</span> Integer staticInt = <span>3</span>;</span><br><span>    <span>private</span> String basicString = <span>"basicString"</span>;</span><br><span>    <span>private</span> <span>static</span> String staticString = <span>new</span> String(<span>"staticString"</span>);</span><br><span>}</span><br></pre></td></tr></tbody></table>

首先查看对象直方图可以找到三个 VMShow 对象

[![](https://zzcoder.oss-cn-hangzhou.aliyuncs.com/jvm/HSDB-11.png)](https://zzcoder.oss-cn-hangzhou.aliyuncs.com/jvm/HSDB-11.png)

那么如何确定这三个地址分别属于哪些变量呢？首先找静态变量，它在 JDK8 中是在 Class 对象中的，因此我们可以找它们的反向指针，如果是`java.lang.Class` 的那么就是静态变量

[![](https://zzcoder.oss-cn-hangzhou.aliyuncs.com/jvm/HSDB-12.png)](https://zzcoder.oss-cn-hangzhou.aliyuncs.com/jvm/HSDB-12.png)

我们可以从 ObjTest 的 `instanceKlass` 中的镜像找到 class 对象来验证是否是该对象的 class

[![](https://zzcoder.oss-cn-hangzhou.aliyuncs.com/jvm/HSDB-13.png)](https://zzcoder.oss-cn-hangzhou.aliyuncs.com/jvm/HSDB-13.png)

那么成员变量和局部变量如何区分呢？成员变量会被类实例引用，而局部变量地址则在会被被放在栈区

[![](https://zzcoder.oss-cn-hangzhou.aliyuncs.com/jvm/HSDB-14.png)](https://zzcoder.oss-cn-hangzhou.aliyuncs.com/jvm/HSDB-14.png)

那么局部变量的反向指针都是 null，怎么确定它就被栈区所引用呢？我们可以看可视化线程栈

[![](https://zzcoder.oss-cn-hangzhou.aliyuncs.com/jvm/HSDB-15.png)](https://zzcoder.oss-cn-hangzhou.aliyuncs.com/jvm/HSDB-15.png)

### [](https://zzcoder.cn/2019/12/06/HSDB%E4%BB%8E%E5%85%A5%E9%97%A8%E5%88%B0%E5%AE%9E%E6%88%98/#%E5%88%86%E6%9E%90%E5%AD%97%E7%AC%A6%E4%B8%B2%E5%AD%97%E9%9D%A2%E9%87%8F%E5%AD%98%E5%82%A8%E5%8C%BA%E5%9F%9F "分析字符串字面量存储区域")分析字符串字面量存储区域

<table><tbody><tr><td><pre><span>1</span><br><span>2</span><br><span>3</span><br><span>4</span><br><span>5</span><br><span>6</span><br><span>7</span><br><span>8</span><br><span>9</span><br><span>10</span><br></pre></td><td><pre><span><span>public</span> <span><span>class</span> <span>StringTest</span> </span>{</span><br><span></span><br><span>    <span><span>public</span> <span>static</span> <span>void</span> <span>main</span><span>(String[] args)</span> </span>{</span><br><span>        String s1 = <span>"a"</span>;</span><br><span>        String s2 = <span>"b"</span>;</span><br><span>        String s3 = s1 + s2;</span><br><span>        String s4 = <span>new</span> String(<span>"ab"</span>);</span><br><span>        System.out.println(s4);</span><br><span>    }</span><br><span>}</span><br></pre></td></tr></tbody></table>

上面一共涉及的字符串字面量和实例分别存储在什么地方呢？

1.  首先在 s2 处打上断点，启动 `HSDB` 监控该进程
    
2.  打开对象直方图发现只有 1 个 `a` 的字符串对象
    
    [![](https://zzcoder.oss-cn-hangzhou.aliyuncs.com/jvm/HSDB-16.png)](https://zzcoder.oss-cn-hangzhou.aliyuncs.com/jvm/HSDB-16.png)
    
3.  查找 StringTable 中 `a` 的对象地址
    
    <table><tbody><tr><td><pre><span>1</span><br></pre></td><td><pre><span>jseval "st = sa.vm.stringTable;st.stringsDo(function (s) { if (sapkg.oops.OopUtilities.stringOopToString(s).matches('^(a)')) {print(s + ': ');s.printValueOn(java.lang.System.out); println('')}})"</span><br></pre></td></tr></tbody></table>
    
    可以根据需要改变 `matches` 中的值来匹配
    
    [![](https://zzcoder.oss-cn-hangzhou.aliyuncs.com/jvm/HSDB-17.png)](https://zzcoder.oss-cn-hangzhou.aliyuncs.com/jvm/HSDB-17.png)
    
    可以看到这个对象地址就是 StringTable 中引用的地址
    
4.  然后打断点在 sout 上，重新开始监控进程
    
5.  重新使用对象直方图查看 String 值
    
    [![](https://zzcoder.oss-cn-hangzhou.aliyuncs.com/jvm/HSDB-18.png)](https://zzcoder.oss-cn-hangzhou.aliyuncs.com/jvm/HSDB-18.png)
    
    这里有5个值，`ab` 有3个：
    
    -   `ab` 字面量
    -   其中 s3 相当于 `new StringBuild().append("a").append("b").toString()`，会创建一个 `ab` 的实例
    -   s4会创建一个 `ab` 的实例
6.  我们重新打印 StringTable 相应的值来验证
    
    <table><tbody><tr><td><pre><span>1</span><br></pre></td><td><pre><span>jseval "st = sa.vm.stringTable;st.stringsDo(function (s) { if (sapkg.oops.OopUtilities.stringOopToString(s).matches('^(a|b).?')) {print(s + ': ');s.printValueOn(java.lang.System.out); println('')}})"</span><br></pre></td></tr></tbody></table>
    
    [![](https://zzcoder.oss-cn-hangzhou.aliyuncs.com/jvm/HSDB-19.png)](https://zzcoder.oss-cn-hangzhou.aliyuncs.com/jvm/HSDB-19.png)
    

那么运行时常量池中存放的是哪些呢？实际上它和 StringTable 一样是这些对象的引用，只不过 StringTable 是全局共享的，而运行时常量池只有该类的一些字面量。我们通过加载类列表可以查看

[![image-20190806204906357](https://zzcoder.oss-cn-hangzhou.aliyuncs.com/jvm/HSDB-20.png)](https://zzcoder.oss-cn-hangzhou.aliyuncs.com/jvm/HSDB-20.png)

### [](https://zzcoder.cn/2019/12/06/HSDB%E4%BB%8E%E5%85%A5%E9%97%A8%E5%88%B0%E5%AE%9E%E6%88%98/#%E5%88%86%E6%9E%90String-intern "分析String.intern")分析String.intern

<table><tbody><tr><td><pre><span>1</span><br><span>2</span><br><span>3</span><br><span>4</span><br><span>5</span><br><span>6</span><br><span>7</span><br><span>8</span><br><span>9</span><br><span>10</span><br><span>11</span><br><span>12</span><br><span>13</span><br><span>14</span><br></pre></td><td><pre><span><span>public</span> <span><span>class</span> <span>StringInternTest</span> </span>{</span><br><span></span><br><span>    <span><span>public</span> <span>static</span> <span>void</span> <span>main</span><span>(String[] args)</span> </span>{</span><br><span>        String s1 = <span>new</span> String(<span>"he"</span>) + <span>new</span> String(<span>"llo"</span>); </span><br><span>        s1.intern(); </span><br><span>        String s2=<span>"hello"</span>; </span><br><span>        System.out.println(s1==s2); </span><br><span></span><br><span>        String s3 = <span>new</span> String(<span>"1"</span>) + <span>new</span> String(<span>"2"</span>); </span><br><span>        String s4 = <span>"12"</span>; </span><br><span>        s3.intern(); </span><br><span>        System.out.println(s3 == s4);  </span><br><span>    }</span><br><span>}</span><br></pre></td></tr></tbody></table>

上述在编译器确定的字面量有 `he`, `llo`, `hello`, `1`, `2`, `12`，但在真正执行到语句前，符号引用不一定解析成直接引用，即字面量对应的对象会在执行到语句时（`idc` 指令）才会创建

首先看通过加载类列表查看字节码指令：

| line | bci | bytecode |
| --- | --- | --- |
| 7 | 0 | new #2 [Class java.lang.StringBuilder] |
| 7 | 3 | dup |
| 7 | 4 | invokespecial #3 [Method void ()] |
| 7 | 7 | new #4 [Class java.lang.String] |
| 7 | 10 | dup |
| 7 | 11 | ldc #5(0) [fast_aldc] |
| 7 | 13 | invokespecial #6 [Method void (java.lang.String)] |
| 7 | 16 | invokevirtual #7 [Method java.lang.StringBuilder append(java.lang.String)] |
| 7 | 19 | new #4 [Class java.lang.String] |
| 7 | 22 | dup |
| 7 | 23 | ldc #8(1) [fast_aldc] |
| 7 | 25 | invokespecial #6 [Method void (java.lang.String)] |
| 7 | 28 | invokevirtual #7 [Method java.lang.StringBuilder append(java.lang.String)] |
| 7 | 31 | invokevirtual #9 [Method java.lang.String toString()] |
| 7 | 34 | astore_1 |
| 8 | 35 | aload_1 |
| 8 | 36 | invokevirtual #10 [Method java.lang.String intern()] |
| 8 | 39 | pop |
| 9 | 40 | ldc #11(2) [fast_aldc] |
| 9 | 42 | astore_2 |
| 10 | 43 | getstatic #12 [Field java.io.PrintStream out] |
| 10 | 46 | aload_1 |
| 10 | 47 | aload_2 |
| 10 | 48 | if_acmpne 55 |
| 10 | 51 | iconst_1 |
| 10 | 52 | goto 56 |
| 10 | 55 | iconst_0 |
| 10 | 56 | invokevirtual #13 [Method void println(boolean)] |
| 12 | 59 | new #2 [Class java.lang.StringBuilder] |
| 12 | 62 | dup |
| 12 | 63 | invokespecial #3 [Method void ()] |
| 12 | 66 | new #4 [Class java.lang.String] |
| 12 | 69 | dup |
| 12 | 70 | ldc #14(3) [fast_aldc] |
| 12 | 72 | invokespecial #6 [Method void (java.lang.String)] |
| 12 | 75 | invokevirtual #7 [Method java.lang.StringBuilder append(java.lang.String)] |
| 12 | 78 | new #4 [Class java.lang.String] |
| 12 | 81 | dup |
| 12 | 82 | ldc #15(4) [fast_aldc] |
| 12 | 84 | invokespecial #6 [Method void (java.lang.String)] |
| 12 | 87 | invokevirtual #7 [Method java.lang.StringBuilder append(java.lang.String)] |
| 12 | 90 | invokevirtual #9 [Method java.lang.String toString()] |
| 12 | 93 | astore_3 |
| 13 | 94 | ldc #16(5) [fast_aldc] |
| 13 | 96 | astore #4 |
| 14 | 98 | aload_3 |
| 14 | 99 | invokevirtual #10 [Method java.lang.String intern()] |
| 14 | 102 | pop |
| 15 | 103 | getstatic #12 [Field java.io.PrintStream out] |
| 15 | 106 | aload_3 |
| 15 | 107 | aload #4 |
| 15 | 109 | if_acmpne 116 |
| 15 | 112 | iconst_1 |
| 15 | 113 | goto 117 |
| 15 | 116 | iconst_0 |
| 15 | 117 | invokevirtual #13 [Method void println(boolean)] |
| 16 | 120 | return |

可以看到确实有 6 个`idc`，但如果我们在第一行语句打上断点，会发现它们都不在 StringTable（但这里的 `he` 在，它可能被其他类用到了），然后执行第一行，会发现 `he` 和 `llo` 在了，但 `hello` 不在

<table><tbody><tr><td><pre><span>1</span><br></pre></td><td><pre><span>jseval "st = sa.vm.stringTable;st.stringsDo(function (s) { if (sapkg.oops.OopUtilities.stringOopToString(s).matches('^(he|llo|hello|1|2|12)')) {print(s + ': ');s.printValueOn(java.lang.System.out); println('')}})"</span><br></pre></td></tr></tbody></table>

[![](https://zzcoder.oss-cn-hangzhou.aliyuncs.com/jvm/HSDB-21.png)](https://zzcoder.oss-cn-hangzhou.aliyuncs.com/jvm/HSDB-21.png)

但是 `hello` 对象还是存在的（new）

[![](https://zzcoder.oss-cn-hangzhou.aliyuncs.com/jvm/HSDB-22.png)](https://zzcoder.oss-cn-hangzhou.aliyuncs.com/jvm/HSDB-22.png)

接着执行 s1.intern 会将 `hello` 对象的地址放入 StringTable

[![](https://zzcoder.oss-cn-hangzhou.aliyuncs.com/jvm/HSDB-23.png)](https://zzcoder.oss-cn-hangzhou.aliyuncs.com/jvm/HSDB-23.png)

再执行 `String s2="hello";` 会发现 `hello` 对象仍然只有一个，都指向同一个。

而继续在 6 打断点，即执行完 `String s4 = "12";`，因为 `12` 不在字符串常量池，那么会新建一个 `12` 的实例，并让字符串常量池引用它，这样会发现就有两个 `12` 了

[![](https://zzcoder.oss-cn-hangzhou.aliyuncs.com/jvm/HSDB-24.png)](https://zzcoder.oss-cn-hangzhou.aliyuncs.com/jvm/HSDB-24.png)
