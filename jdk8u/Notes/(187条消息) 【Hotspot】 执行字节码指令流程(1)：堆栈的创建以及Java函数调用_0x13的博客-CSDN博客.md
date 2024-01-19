---
source: https://blog.csdn.net/qq_34448345/article/details/130360277
---
**什么是基于栈的虚拟机：**

关于JVM中调用Java函数需要了解一下基于栈的虚拟机：

栈是在内存中单独维护的一个结构，栈中有局部变量表，执行操作比如a+b需要调用将两个变量加载到操作数栈中，相加后放到栈顶然后弹出得到计算结果，这样cpu可能执行三条以上的指令。

另外还有一种基于寄存器的虚拟机，没有栈的结构而是构造一个虚拟的寄存器列表，映射cpu上面的真实寄存器，执行a+b时只需要发送一条指令并携带两个寄存器的地址就可以执行相加。

这两个区别在于基于寄存器的虚拟机和cpu硬件绑定较深不好移植但是性能更好，基于栈的虚拟机大部分操作就是入栈出栈扩展性好但是性能低一些因为用到的指令更多。

基于栈的虚拟机，有JVM，CPython以及.Net CLR。基于寄存器的，有Dalvik以及Lua5.0。所以研究 Java 函数调用实际上是看栈帧的入栈、出栈处理。

![](https://img-blog.csdnimg.cn/7135c5b7a4034dd8ab2999add4bac50d.png)

虚拟机都需要设计字节码指令，字节码指令还需要进一步转换为汇编，汇编得到目标平台的机器码才能够执行，比如上文的 a+b 操作使用 ASM 中的 Opcode 就是下面4条指令：

```
1 iload_0    //操作数栈读取局部变量的第1个slot2 iload_1    //操作数栈读取局部变量的第2个slot3 iadd    //将栈顶的两个slot相加4 istore_2    //保存到局部变量中第3个slo
```

其中 Opcode（操作码，也就是字节码指令）定义为一个系统中的最小操作指令，ASM中的 Opcode 就定义了 JVM 中要执行的最小操作指令。一个Java函数会按顺序翻译为一组 Opcode，然后一条一条执行。

到这一步了实际上还是不能跨平台，因为 Opcode 设计是通用的，还需要按照不同平台执行汇编代码最终进行执行。这一部分硬件相关的代码在 hotspt/src/os_cpu目录下：

```
hotspot/src/os_cpu/aix_ppchotspot/src/os_cpu/bsd_x86hotspot/src/os_cpu/bsd_zerohotspot/src/os_cpu/linux_ppchotspot/src/os_cpu/linux_sparchotspot/src/os_cpu/linux_x86hotspot/src/os_cpu/linux_zerohotspot/src/os_cpu/solaris_sparchotspot/src/os_cpu/solaris_x86hotspot/src/os_cpu/windwos_x86
```

上面左边是操作系统，右边是指令架构，对应的代码在 hotspot/src/os和 hotspot/src/cpu

```
hotspot/src/os/aix    IBM基于AT&T Unix System V开发的一套类UNIX操作系统，运行在IBM专有的Power系列芯片设计的小型机硬件系统之上。hotspot/src/os/bsd    BSD (Berkeley Software Distribution，伯克利软件套件)是Unix的衍生系统。hotspot/src/os/linux    Linux，全称GNU/Linux，是一种免费使用和自由传播的类UNIX操作系统。hotspot/src/os/posix    POSIX（Portable Operating System Interface）是Unix系统的一个设计标准，以兼容uinx。hotspot/src/os/solaris    Solaris 是 Sun Microsystems研发的计算机操作系统。hotspot/src/os/windows    Microsoft Windows是美国微软公司以图形用户界面为基础研发的操作系统。hotspot/src/cpu/ppc    精简指令集（RISC）架构的中央处理器（CPU），其基本的设计源自IBM的POWER（Performance Optimized With Enhanced RISC。hotspot/src/cpu/sparc    SPARC （Scalable Processor Architecture）是一种精简指令集（RISC）指令集架构。hotspot/src/cpu/x86    复杂指令集hotspot/src/cpu/zero
```

比如Java函数调用的硬件层之上的最后一个入口是 generate_call_stub() 函数会有下面几个实现：

![](https://img-blog.csdnimg.cn/7fe48023d2504b6591a139becc6a5474.png)

栈区域：

用来存放基本数据类型和引用数据类型的实例的（也就是实例对象的在堆中的首地址）还有就是堆栈是线程独享的。每一个线程都有自己的线程栈。

局部变量在栈内存中，JVM为每一个类分配一个栈帧，然后引用类型的局部变量指向堆内存中的地址），但是堆是内存中共享的区域，所以要考虑线程安全的问题。

堆区域：

用来存放程序动态生成的数据。（new 出来的对象的实例存储在堆中，但是仅仅存储的是成员变量，也就是平时所说的实例变量，成员变量的值则存储在常量池中。成员方法是此类所实现实例共享的，并不是每一次new 都会创建成员方法。成员方法被存储在方法区，并不是存储在第一个创建的对象中，因为那样的话，第一个对象被回收，后面创建的对象也就没有方法引用了。静态变量也存储在方法区中。

方法区（非堆）：

在堆中为其分配的一部分内存）：里面存储的是一些。类类型加载的东西（也就是反射中的.class之后的Class），用于存储已经被虚拟机加载的类的信息、常量、静态变量等。与堆一样，是被线程共享的内存区域，要注意线程安全问题。

**1.如何调用 java 方法**

从c/c++中调用java方法的地方主要有下列一些情况，这些都调用的 javaCalls.cpp 模块：

(1)调用 Java 的静态 main() 函数

(2)主类加载时调用 Java 类 LauncherHelper.checkAndLoadMain() 函数

(3)初始化 Java 类时调用构造函数，通过 JavaCalls::call_default_constructor()函数

(4)类加载时调用类加载器的 loadClass() 函数

JavaCalls 模块：

Java虚拟机规范定义的字节码指令函数共有5种，分别为invokestatic、invokedynamic、invokestatic、invokespecial、invokevirtual几种方法调用指令。

这些call_static()、call_virtual()函数内部调用了call()函数。

```
class JavaCalls: AllStatic {  static void call_helper(JavaValue* result, methodHandle* method, JavaCallArguments* args, TRAPS);public:  // receiver表示方法的接收者，如A.main()调用中，A就是方法的接收者  // 构造函数进行java对象初始化时调用  static void call_default_constructor(JavaThread* thread, methodHandle method, Handle receiver, TRAPS);  // 使用如下函数调用Java中一些特殊的方法，如类初始化方法<clinit>等  // The receiver must be first oop in argument list  static void call_special(JavaValue* result, KlassHandle klass, Symbol* name, Symbol* signature, JavaCallArguments* args, TRAPS);  static void call_special(JavaValue* result, Handle receiver, KlassHandle klass, Symbol* name, Symbol* signature, TRAPS); // No args  static void call_special(JavaValue* result, Handle receiver, KlassHandle klass, Symbol* name, Symbol* signature, Handle arg1, TRAPS);  static void call_special(JavaValue* result, Handle receiver, KlassHandle klass, Symbol* name, Symbol* signature, Handle arg1, Handle arg2, TRAPS);  // 使用如下函数调用动态分派的一些方法  // The receiver must be first oop in argument list  static void call_virtual(JavaValue* result, KlassHandle spec_klass, Symbol* name, Symbol* signature, JavaCallArguments* args, TRAPS);  static void call_virtual(JavaValue* result, Handle receiver, KlassHandle spec_klass, Symbol* name, Symbol* signature, TRAPS); // No args  static void call_virtual(JavaValue* result, Handle receiver, KlassHandle spec_klass, Symbol* name, Symbol* signature, Handle arg1, TRAPS);  static void call_virtual(JavaValue* result, Handle receiver, KlassHandle spec_klass, Symbol* name, Symbol* signature, Handle arg1, Handle arg2, TRAPS);  // 使用如下函数调用Java静态方法  static void call_static(JavaValue* result, KlassHandle klass, Symbol* name, Symbol* signature, JavaCallArguments* args, TRAPS);  static void call_static(JavaValue* result, KlassHandle klass, Symbol* name, Symbol* signature, TRAPS);  static void call_static(JavaValue* result, KlassHandle klass, Symbol* name, Symbol* signature, Handle arg1, TRAPS);  static void call_static(JavaValue* result, KlassHandle klass, Symbol* name, Symbol* signature, Handle arg1, Handle arg2, TRAPS);  // 更低一层的接口，上面一些函数可能会最终调用到如下这个函数  static void call(JavaValue* result, methodHandle method, JavaCallArguments* args, TRAPS);};
```

其中关注 call_helper() 来调试 Java Method 的调用流程。什么时Stub代码？Stub代码是HotSpot生成的固定调用点的代码。为什么需要Stub代码，HotSpot内部与Java代码调用的地方有两种形式JNI和Stub。JNI调用方式需要Java代码与JNI代码一一对应，每一个Java方法都对应一个JNI函数。而Stub是HosSpot内部为了统一调用Java函数而生成的固定调用点。通过手工汇编编写的一段存储于内存中的统一调用点。HotSpot内部按Java方法功能类别生成了多个调用点的Stub代码。当虚拟机执行到一个Java方法调用时，会统一转到合适的Stub调用点。该调用点会进行栈帧创建，参数传递处理，大大简化了设计。例如其中  JavaCalls::call_virtual()就是Stub调用的一个用例。

**2.ASM库类**

ASM库类：

ASM是一个操作Java字节码的类库，作用是对Java字节码（.class）进行拆分、修改、合并。可以修改字节码。以Spring-Aop为例就是使用ASM对目标实例新增、删除函数以植入想要的功能。

ASM处理字节码（ByteCode）数据的思路是这样的：

第一步，将.class文件拆分成多个部分；第二步，对某一个部分的信息进行修改；第三步，将多个部分重新组织成一个新的.class文件。

不管是编译执行还是解释执行，Java 代码最终底层执行的都是机器代码。字节码需要先得到一条一条的字节码指令 Opcode，而 Opcode 加上操作数就能转化为机器代码来执行。

所有 Java 源代码到机器代码的转换流程如下：

Java源码 -> Java字节码 -> Opcode指令 -> 机器代码

Opcode指令在JVM文档中一共定义了 205个：

![](https://img-blog.csdnimg.cn/1683b30cabf1476481d12be4a63cb645.png)

Return 相关的 Opcode：

172(ireturn)、174(freturn)、176(areturn)、173(lreturn)、175(dreturn)、177(return)

Constant 相关的 Opcode：

1(aconst_null)、6(iconst_3)、11(fconst_0)、16(bipush)

2(iconst_m1)、7(iconst_4)、12(fconst_1)、17(sipush)

3(iconst_0)、8(iconst_5)、13(fconst_2)、18(ldc)

4(iconst_1)、9(lconst_0)、14(dconst_0)、19(ldc_w)

5(iconst_2)、10(lconst_1)、15(dconst_1)、20(ldc2_w)

Transfer values 相关的 Opcode：

21(iload)、28(iload_2)、35(fload_1)、42(aload_0

22(lload)、29(iload_3)、36(fload_2)、43(aload_1

23(fload)、30(lload_0)、37(fload_3)、44(aload_2

24(dload)、31(lload_1)、38(dload_0)、45(aload_3

25(aload)、32(lload_2)、39(dload_1)、46

26(iload_0)、33(lload_3)、40(dload_2)、47

27(iload_1)、34(fload_0)、41(dload_3)、48

Stack 相关的 Opcode：

87(pop)、90(dup_x1)、93(dup2_x1)

88(pop2)、91(dup_x2)、94(dup2_x2)

89(dup)、92(dup2)、95(swap)

加载和储存指令：

* 将一个局部变量加载到操作数栈： iload, iload_n, lload, lload_n, fload, fload_n, dload, dload_n, aload, aload_n；

* 将一个数值从操作数栈存储到局部变量表： istore, istore_n, lstore_, lstore_n, fstore， fstore_n, dstore_, dstore_n, astore, astore_n；

* 将一个常量加载到操作数栈： bipush, sipush, ldc, ldc_w, ldc2_w， aconst_null, iconst_m1, iconst_i, lconst_l, fconst_f, dconst_d；

* 扩充局部变量表的访问索引的指令： wide；

运算指令：

* 加法指令： iadd, ladd, fadd, dadd。

* 减法指令： isub, lsub, fsub, dsub。

* 乘法指令： imul, lmul, fmul, dmul。

* 除法指令： idiv, ldiv, fdiv, ddiv。

* 求余指令： irem, lrem, frem, drem。

* 取反指令： ineg, lneg, fneg, dneg。

* 位移指令： ishl, ishr, iushr, lshl, lshr, lushr。

* 按位或指令： ior, lor。

* 按位与指令： iand, land。

* 按位异或指令： ixor, lxor。

* 局部变量自增指令： iinc。

* 比较指令： dcmpg, dcmpl, fcmpg, fcmpl, lcmp。

以一个类型转换的 Java 函数为例：

```
public long convert(){        short shortNum = 50;        int intNum = 1000;        long result = shortNum  * intNum  + 1000000;return result;}
```

最终生成的 Opcode 序列：

```
public long convert();Code://声明了栈的最大深度、本地字数和传入参数数，//对于对象方法，会传入this引用，因此这里Arg_szie=1，//如上的程序，this会占用1个 字，shortNum 和 intNum分别占1个字，result占2个字(long)，因此这里Locals=5Stack=2, Locals=5, Args_size=10: bipush 50 //将50入到栈，在栈中会占1个字的位置//将栈顶值弹出设给第2个本地变量（传入参数也会以本地变量的方式存在，//在这了第1个参数是this），这两段指令等价于short shortNum  = 80，//从这里可以看出，JVM直接把short当做integer来运算的2: istore_13: sipush 1000 //与上类似，把1000入到栈顶，这里1000超过了b所能表示的范围，所以是sipush6: istore_2 //同样的，把堆栈值弹出并设给第3个本地变量，这两段等价于int  intNum = 10007: iload_18: iload_2 //把第2个本地变量(shortNum 和 intNum)入栈9: imul //乘运算，弹出2个栈顶值(shortNum 和 intNum)，并把运算结果入栈，这时候栈顶值就是 shortNum *  intNum10: ldc #16; //1000000超过short能够表示的范围，会以常量池中条目的形式存在，这里#16就是1000000，这里把1000000入栈//弹出栈顶值2个字的值，并进行add操作，把add结果再入栈，//这时shortNum * intNum和1000000被弹出栈，并把 shortNum * intNum+1000000的值入栈12: iadd13: i2l //从栈顶弹出1个字的值，并转换成l型，再入到栈中（这时候，shortNum * intNum  +1000000会占用栈顶2个字的位置。14: lstore_3 //从栈顶弹出2个字（因为是l型的），并把结果赋给第4和第5个local位置（l需要占2个位置），想当于把运算结果赋给result15: lload_3 //将第4和第5个local位置的值入栈16: lreturn //返回指令，将栈顶2个位置的值弹出，并压入方法调用者的操作栈（上一个方法的操作栈），同时把本方法的操作栈清空
```

通过 javap 工具反编译 class 文件，javap  -verbose Debug.class 可以得到上诉 Opcode 序列。其中每个Opcode 需要转换的机器代码片段的模板定义在  TemplateTable 中：

```
class TemplateTable: AllStatic {static void nop();static void aconst_null();static void iconst(int value);static void lconst(int value);static void fconst(int value);static void dconst(int value);};
```

**3.如何初始化调用栈**

对于一些java、scala等jvm语言，可以通过如下方式获取方法调用栈，因为有虚拟机的概念，使得这类虚拟机语言可以方便的获取调用栈顺序来进行代码调试。例如：

```
Scala语言：val elements = (new Throwable).getStackTrace.reversefor (i <- 1 to elements.length - 1) {     System.out.println("栈: " + elements(i))}Java语言：StackTraceElement[] trace= Thread.currentThread().getStackTrace();for(int i=trace.length-1;i>=0;i--){     System.out.println("栈："+Thread.currentThread().getStackTrace()[i].getClassName();}
```

而这一部分栈信息就是依附在主栈中，并且记录在 Stack 的黄色部分中：

![](https://img-blog.csdnimg.cn/69c47a668341420cb0279958ba3695c2.png)

例如 Java 静态 main 函数的调用，中间会经过 JavaMain() ，调用 linux 函数创建线程以及独立的栈帧空间：

```
jdk/src/share/bin/main.c    =>     程序入口jdk/src/solaris/bin/java_md_solinux.c ContinueInNewThread0()     =>     调用pthread_create创建新线程,启动JavaMain入口jdk/src/share/bin/java.c JavaMain()     =>     真正的jvm初始化入口hotspot/src/share/vm/runtime.c Threads::create_vm()     =>     jvm初始化入口，下面以 linux 系统为例hotspot/src/os/linux/vm/os_linux.cpp os::create_thread()    =>    创建线程 pthread_create，是类Unix操作系统（Unix、Linux、Mac OS X等）的创建线程的函数。hotspot/src/share/vm/runtime/os.cpp start_thread()     =>    启动线程hotspot/src/os/linux/vm/os_linux.cpp os::pd_start_thread()     =>    启动linux线程，调用notify()唤醒jdk/src/share/bin/java.c LoadMainClass()     =>     加载一个类并验证主类是否存在jdk/src/share/bin/java.c     =>     调用main方法,封装成jni上面过程就是蓝色部分栈的建立过程，而最终函数都会调用 JavaCalls  模块，而 Java 等每个函数的栈空间会在 call_helper() 中进行处理。
```

**4.J** **ava 堆栈建立以及 call_helper() 全流程**

调用  JavaCalls::call_helper  流程如下：

(1)首先根据配置判断 JaveMethod 是否需要首次编译，是的话调用JIT进行编译。

调用 CompilationPolicy::must_be_compiled(method) 进行判断。

调用 CompileBroker::compile_method(method) 进行编译。

(2)从 address 上获取 entry_point 对象。这个对象用于后续找出 JavaMethod 第一个字节码指令 Opcode 然后调用并创建 Java 栈，也就是 JavaMethod 的最终调用入口。

每种 Java方法对应的 entry_point 会在 generate_normal_entry() 中进行创建,初始化三个东西 1.局部变量表 2.方法栈信息 3.操作数栈信息，用于辅助 JavaMethod 的调用。

entry_point 会为传输传入 StubRoutines::call_stub()。

(3)调用 JavaCallWrapper link(method, receiver, result, CHECK) 创建 JavaCallWrapper 对象。

JavaCallWrapper 会为传输传入 StubRoutines::call_stub()。

每次执行Java方法调用时都需要创建一个新的JavaCallWrapper实例，然后在方法调用结束销毁这个实例，

通过JavaCallWrapper实例的创建和销毁来保存方法调用前当前线程的上一个栈帧，

重新分配或者销毁一个handle block，保存和重置Java调用栈的fp/sp。JavaCallWrapper实例的指针保存在调用栈上。

JavaCallWrapper定义的属性都是私有，说明如下：

_thread：JavaThread*，关联的Java线程实例

_handles：JNIHandleBlock*，实际保存JNI引用的内存块的指针

_callee_method: Method*，准备调用的Java方法

_receiver：oop 执行方法调用的接受对象实例

_anchor：JavaFrameAnchor，用于记录线程的执行状态，比如pc计数器

_result：JavaValue*，保存方法调用结果对象

(4)创建 StubRoutines::call_stub()函数返回一个函数指针,通过指针来调用这个函数、

这里就是 Java 帧的创建入口，传递了8个参数：

```
StubRoutines::call_stub()(        (address)&link, // 类型为JavaCallWrapper// (intptr_t*)&(result->_value),        result_val_address, // 函数返回值地址        result_type,method(),  // 当前要执行的 Java Method,这里获取出来是元数据信息,里面包含字节码信息        entry_point, // 用于后续帧开辟, entry_point 会从 method() 中获取出 Java Method 的第一个字节码命令(Opcode),也就是整个 Java Method 的调用入口        args->parameters(), // 就是 Java Method 的函数参数        args->size_of_parameters(), // 用函数调用Caller栈来传递,这个就是Java Method 的函数参数大小 size,占用内存大小(字节)        CHECK // 用函数调用Caller栈来传递,CHECK是宏,定义的线程对象 thread      );
```

JavaCalls::call_helper 整个代码如下:

```
void JavaCalls::call_helper(JavaValue* result, methodHandle* m, JavaCallArguments* args, TRAPS) {// 省略// 检查目标Java方法是否是开启了首次执行必须被编译,是的话调用JIT编译器去编译目标方法// 如果配置了-Xint选项就是解释模式执行,也就是需要运行时才进行编译那么就不会走这里// 如果配置了编译执行则会走这里  assert(!thread->is_Compiler_thread(), "cannot compile from the compiler");if (CompilationPolicy::must_be_compiled(method)) {    CompileBroker::compile_method(method, InvocationEntryBci,                                  CompilationPolicy::policy()->initial_compile_level(),                                  methodHandle(), 0, "must_be_compiled", CHECK);  }// 获取保存在 address 属性上的 entry_point// 每种 Java方法对应的 entry_point 会在 generate_normal_entry() 中进行创建,初始化三个东西 1.局部变量表 2.方法栈信息 3.操作数栈信息// 创建的这些 entry_point 通过 _entry_table 进行缓存用于快速查找// 在方法连接时 Method::link_method() 会调用 Interpreter::entry_for_method() 获取 Java 方法入口 entry_point// 然后得到方法入口 entry_point 后调用 set_interpreter_entry() 进行保存,表存在属性 address 上// 而这里就是将这个 entry_point 取出来,直接从 address 属性上获取address entry_point = method->from_interpreted_entry();if (JvmtiExport::can_post_interpreter_events() && thread->is_interp_only_mode()) {    entry_point = method->interpreter_entry();  }  BasicType result_type = runtime_type_from(result);  bool oop_result_flag = (result->get_type() == T_OBJECT || result->get_type() == T_ARRAY);  intptr_t* result_val_address = (intptr_t*)(result->get_value_addr());  Handle receiver = (!method->is_static()) ? args->receiver() : Handle();if (thread->stack_yellow_zone_disabled()) {    thread->reguard_stack();  }// to Javaif (!os::stack_shadow_pages_available(THREAD, method)) {    Exceptions::throw_stack_overflow_exception(THREAD, __FILE__, __LINE__, method);return;  } else {    os::bang_stack_shadow_pages();  }// 调用 Java Method  { JavaCallWrapper link(method, receiver, result, CHECK);    { HandleMark hm(thread);  // HandleMark used by HandleMarkCleaner// StubRoutines::call_stub()函数返回一个函数指针,通过指针来调用这个函数// Linux X86架构下的C/C++函数调用约定，在这个约定下，以下寄存器用于传递参数,六个参数以内用寄存器传递,操作六个则使用调用栈来传递额外的参数// 第1个参数：rdi c_rarg0// 第2个参数：rsi c_rarg1// 第3个参数：rdx c_rarg2// 第4个参数：rcx c_rarg3// 第5个参数：r8 c_rarg4// 第6个参数：r9 c_rarg5      StubRoutines::call_stub()(        (address)&link, // 类型为JavaCallWrapper// (intptr_t*)&(result->_value),        result_val_address, // 函数返回值地址        result_type,method(),  // 当前要执行的 Java Method,这里获取出来是元数据信息,里面包含字节码信息        entry_point, // 用于后续帧开辟, entry_point 会从 method() 中获取出 Java Method 的第一个字节码命令(Opcode),也就是整个 Java Method 的调用入口        args->parameters(), // 就是 Java Method 的函数参数        args->size_of_parameters(), // 用函数调用Caller栈来传递,这个就是Java Method 的函数参数大小 size,占用内存大小(字节)        CHECK // 用函数调用Caller栈来传递,CHECK是宏,定义的线程对象 thread      );      result = link.result();  // circumvent MS C++ 5.0 compiler bug (result is clobbered across call)if (oop_result_flag) {        thread->set_vm_result((oop) result->get_jobject());      }    }  }if (oop_result_flag) {    result->set_jobject((jobject)thread->vm_result());    thread->set_vm_result(NULL);  }}
```

Linux X86架构下的C/C++函数调用约定，在这个约定下，以下寄存器用于传递参数,六个参数以内用寄存器传递,操作六个则使用调用栈来传递额外的参数，此时栈结构如下：

![](https://img-blog.csdnimg.cn/cd021d70dcd64878bb48b00316af7ab8.png)

**5. StubRoutines::call_stub() 创建栈帧，参数压栈并调用 JavaMethod**

接着就进入了  StubRoutines::call_stub() 参数了，一共传递了8个参数进去。实际上是 CallStud 这个结构体，封装了 Java Method 的调用，定义如下：

```
// Calls to Javatypedef void (*CallStub)(address   link,    // 连接器  intptr_t* result,    // 函数返回值的地址  BasicType result_type, // 函数返回值类型Method* method, // JVM内部所表示的Java方法对象address   entry_point, // JVM调用Java方法的例程入口，所有Java方法调用之前都需要先执行 entry_point 中这段机器指令再跳转到 Java方法上  intptr_t* parameters,  int       size_of_parameters,  TRAPS);
```

CallStub是一个函数指针并强制转换成为了 _call_stub_entry 类型并指向了 generate_call_stub() 函数。这个映射定义在 stubGenerator_x86_64.cpp文件中，由初始化进行设置：

```
void generate_initial() {// 省略  StubRoutines::_call_stub_entry = generate_call_stub(StubRoutines::_call_stub_return_address);}
```

接下来就进入  generate_call_stub() 这个函数了，这一步会根据前面传递的8个参数创建 Java Method 的栈、将6个参数亚栈、暂存寄存器状态、调用 JavaMethod、保存返回值、退栈等全部流程。

首先查看开辟新栈帧的机器指令片段：

__ enter();

__ subptr(rsp, -rsp_after_call_off * wordSize);

实际上对应的汇编指令：

// push   %rbp

// mov    %rsp,%rbp

// sub    $0x60,%rsp

在执行完上面两行机器指令片段之后，帧帧结构就变化为：

![](https://img-blog.csdnimg.cn/b3ab6e4848af4bb690d856be33294404.png)

处理开辟新栈帧之外，还有6个参数需要设置到栈帧中来进行传递，处理6个参数之外还有一些其他寄存器的值需要设置。这个过程称为现场保存、参数压栈。看 _movptr(xx) 部分代码。

源码机器指令：

```
__ movptr(parameters,   c_rarg5); // parameters__ movptr(entry_point,  c_rarg4); // entry_point__ movptr(method,       c_rarg3); // method__ movl(result_type,  c_rarg2);   // result type__ movptr(result,       c_rarg1); // result__ movptr(call_wrapper, c_rarg0); // call wrapper
```

因为函数接下来要做的操作是为Java方法准备参数并调用Java方法，我们并不知道Java方法会不会破坏这些寄存器中的值，所以要保存下来，等调用完成后进行恢复。

此时的栈帧信息如下：

![](https://img-blog.csdnimg.cn/9a3d9f73b19a48b585b4214ea190f55c.png)

到这里 Jave Method 真正调用前还差传入的参数，需要向栈帧中压入实际的参数，个数就是 parameter size，得到的栈结构如下：

![](https://img-blog.csdnimg.cn/d18a89630ff943478bb16474dae9b07e.png)

最后一步就是调用 Java Method，源码指令如下：

```
// call Java function__ BIND(parameters_done);__ movptr(rbx, method);             // 将method地址包含的数据接Method*拷贝到rbx中__ movptr(c_rarg1, entry_point);    // 将解释器的入口地址拷贝到c_rarg1寄存器中__ mov(r13, rsp);                   // 将rsp寄存器的数据拷贝到r13寄存器中BLOCK_COMMENT("call Java function");__ call(c_rarg1);    // 调用解释器的解释函数，从而调用Java方法，后续就是 entry_point 的工作啦。生成的汇编代码如下：mov     -0x18(%rbp),%rbx      // 将Method*送到%rbx中mov     -0x10(%rbp),%rsi      // 将entry_point送到%rsi中mov     %rsp,%r13    // 将调用者的栈顶指针保存到%r13中callq   *%rsi    // 调用Java方法     后面再分析 entry_point 的调用工作，此时 generate_call_stub() 函数走到这一步就还剩调用结果，返回值处理、退栈操作了。在处理结果之前先将前面暂存的寄存器状态进行恢复，机器指令如下：#ifdef _WIN64for (int i = 15; i >= 6; i--) {      __ movdqu(as_XMMRegister(i), xmm_save(i));    }#endif    __ movptr(r15, r15_save);    __ movptr(r14, r14_save);    __ movptr(r13, r13_save);    __ movptr(r12, r12_save);    __ movptr(rbx, rbx_save);#ifdef _WIN64    __ movptr(rdi, rdi_save);    __ movptr(rsi, rsi_save);#else    __ ldmxcsr(mxcsr_save);然后保存返回值、退栈：// restore rsp__ addptr(rsp, -rsp_after_call_off * wordSize);// return__ pop(rbp);__ ret(0);转换成汇编如下：add    $0x60,%rsp    // %rsp加上0x60，也就是执行退栈操作，也就相当于弹出了callee_save寄存器和压栈的那6个参数pop    %rbpretq    // 方法返回，指令中的q表示64位操作数，就是指的栈中存储的return address是64位的
```

退栈之后，帧结构恢复到 call_helper() 调用之前，信息如下：

![](https://img-blog.csdnimg.cn/185f67ac69ab446ab304a1d0fda43acf.png)

注意按照后缀就知道，JVM会根据不同的操作系统使用的cpu指令集来调用不同的实现，例如这里的x86_64所对应的源文件 hotspot/src/cpu/x86/vm/stubGenerator_x86_64.cpp 。

generate_call_stub() 整个代码如下：

```
address generate_call_stub(address& return_address) {    assert((int)frame::entry_frame_after_call_words == -(int)rsp_after_call_off + 1 &&           (int)frame::entry_frame_call_wrapper_offset == (int)call_wrapper_off,"adjust this code");    StubCodeMark mark(this, "StubRoutines", "call_stub");address start = __ pc();// same as in generate_catch_exception()!    const Address rsp_after_call(rbp, rsp_after_call_off * wordSize);    const Address call_wrapper  (rbp, call_wrapper_off   * wordSize);    const Address result        (rbp, result_off         * wordSize);    const Address result_type   (rbp, result_type_off    * wordSize);    const Address method        (rbp, method_off         * wordSize);    const Address entry_point   (rbp, entry_point_off    * wordSize);    const Address parameters    (rbp, parameters_off     * wordSize);    const Address parameter_size(rbp, parameter_size_off * wordSize);// same as in generate_catch_exception()!    const Address thread        (rbp, thread_off         * wordSize);    const Address r15_save(rbp, r15_off * wordSize);    const Address r14_save(rbp, r14_off * wordSize);    const Address r13_save(rbp, r13_off * wordSize);    const Address r12_save(rbp, r12_off * wordSize);    const Address rbx_save(rbp, rbx_off * wordSize);// 开辟新的栈帧,参数压栈,可以翻译为下面三条汇编，用于开辟新栈帧// push   %rbp// mov    %rsp,%rbp// sub    $0x60,%rsp    __ enter();    __ subptr(rsp, -rsp_after_call_off * wordSize);// 写入六个剩余的参数#ifndef _WIN64    __ movptr(parameters,   c_rarg5); // parameters    __ movptr(entry_point,  c_rarg4); // entry_point#endif    __ movptr(method,       c_rarg3); // method    __ movl(result_type,  c_rarg2);   // result type    __ movptr(result,       c_rarg1); // result    __ movptr(call_wrapper, c_rarg0); // call wrapper// save regs belonging to calling function    __ movptr(rbx_save, rbx);    __ movptr(r12_save, r12);    __ movptr(r13_save, r13);    __ movptr(r14_save, r14);    __ movptr(r15_save, r15);#ifdef _WIN64for (int i = 6; i <= 15; i++) {      __ movdqu(xmm_save(i), as_XMMRegister(i));    }    const Address rdi_save(rbp, rdi_off * wordSize);    const Address rsi_save(rbp, rsi_off * wordSize);    __ movptr(rsi_save, rsi);    __ movptr(rdi_save, rdi);#else    const Address mxcsr_save(rbp, mxcsr_off * wordSize);    {      Label skip_ldmx;      __ stmxcsr(mxcsr_save);      __ movl(rax, mxcsr_save);      __ andl(rax, MXCSR_MASK);    // Only check control and mask bits      ExternalAddress mxcsr_std(StubRoutines::addr_mxcsr_std());      __ cmp32(rax, mxcsr_std);      __ jcc(Assembler::equal, skip_ldmx);      __ ldmxcsr(mxcsr_std);      __ bind(skip_ldmx);    }#endif// Load up thread register// 加载线程寄存器,转换为汇编代码如下,原理是将栈帧中 0x18(%rbp) 这个变量存储到 %r15 寄存器中:// mov    0x18(%rbp),%r15// mov    0x1764212b(%rip),%r12   # 0x00007fdf5c6428a8    __ movptr(r15_thread, thread);    __ reinit_heapbase();#ifdef ASSERT// make sure we have no pending exceptions    {      Label L;      __ cmpptr(Address(r15_thread, Thread::pending_exception_offset()), (int32_t)NULL_WORD);      __ jcc(Assembler::equal, L);      __ stop("StubRoutines::call_stub: entered with pending exception");      __ bind(L);    }#endif// pass parameters if anyBLOCK_COMMENT("pass parameters if any");    Label parameters_done;    __ movl(c_rarg3, parameter_size);    __ testl(c_rarg3, c_rarg3);    __ jcc(Assembler::zero, parameters_done);    Label loop;    __ movptr(c_rarg2, parameters);       // parameter pointer    __ movl(c_rarg1, c_rarg3);            // parameter counter is in c_rarg1    __ BIND(loop);    __ movptr(rax, Address(c_rarg2, 0));// get parameter    __ addptr(c_rarg2, wordSize);       // advance to next parameter    __ decrementl(c_rarg1);             // decrement counter    __ push(rax);                       // pass parameter    __ jcc(Assembler::notZero, loop);// call Java function// 生成的汇编代码如下：// mov     -0x18(%rbp),%rbx      // 将Method*送到%rbx中// mov     -0x10(%rbp),%rsi      // 将entry_point送到%rsi中// mov     %rsp,%r13    // 将调用者的栈顶指针保存到%r13中// callq   *%rsi    // 调用Java方法    __ BIND(parameters_done);    __ movptr(rbx, method);             // 将method地址包含的数据接Method*拷贝到rbx中    __ movptr(c_rarg1, entry_point);    // 将解释器的入口地址拷贝到c_rarg1寄存器中    __ mov(r13, rsp);                   // 将rsp寄存器的数据拷贝到r13寄存器中BLOCK_COMMENT("call Java function");    __ call(c_rarg1);    // 调用解释器的解释函数，从而调用Java方法，后续就是 entry_point 的工作啦。BLOCK_COMMENT("call_stub_return_address:");return_address = __ pc();// store result depending on type (everything that is not// T_OBJECT, T_LONG, T_FLOAT or T_DOUBLE is treated as T_INT)    __ movptr(c_rarg0, result);    Label is_long, is_float, is_double, exit;    __ movl(c_rarg1, result_type);    __ cmpl(c_rarg1, T_OBJECT);    __ jcc(Assembler::equal, is_long);    __ cmpl(c_rarg1, T_LONG);    __ jcc(Assembler::equal, is_long);    __ cmpl(c_rarg1, T_FLOAT);    __ jcc(Assembler::equal, is_float);    __ cmpl(c_rarg1, T_DOUBLE);    __ jcc(Assembler::equal, is_double);// handle T_INT case    __ movl(Address(c_rarg0, 0), rax);    __ BIND(exit);// pop parameters    __ lea(rsp, rsp_after_call);#ifdef ASSERT// verify that threads correspond    {      Label L, S;      __ cmpptr(r15_thread, thread);      __ jcc(Assembler::notEqual, S);      __ get_thread(rbx);      __ cmpptr(r15_thread, rbx);      __ jcc(Assembler::equal, L);      __ bind(S);      __ jcc(Assembler::equal, L);      __ stop("StubRoutines::call_stub: threads must correspond");      __ bind(L);    }#endif// 这是前面暂存的一些寄存器,为了防止java调用中造成的修改,这里需要进行恢复还原#ifdef _WIN64for (int i = 15; i >= 6; i--) {      __ movdqu(as_XMMRegister(i), xmm_save(i));    }#endif    __ movptr(r15, r15_save);    __ movptr(r14, r14_save);    __ movptr(r13, r13_save);    __ movptr(r12, r12_save);    __ movptr(rbx, rbx_save);#ifdef _WIN64    __ movptr(rdi, rdi_save);    __ movptr(rsi, rsi_save);#else    __ ldmxcsr(mxcsr_save);#endif// 转换成汇编如下：// add    $0x60,%rsp    // %rsp加上0x60，也就是执行退栈操作，也就相当于弹出了callee_save寄存器和压栈的那6个参数// pop    %rbp// retq    // 方法返回，指令中的q表示64位操作数，就是指的栈中存储的return address是64位的// restore rsp    __ addptr(rsp, -rsp_after_call_off * wordSize);// return    __ pop(rbp);    __ ret(0);// handle return types different from T_INT    __ BIND(is_long);    __ movq(Address(c_rarg0, 0), rax);    __ jmp(exit);    __ BIND(is_float);    __ movflt(Address(c_rarg0, 0), xmm0);    __ jmp(exit);    __ BIND(is_double);    __ movdbl(Address(c_rarg0, 0), xmm0);    __ jmp(exit);return start;  }
```

**6.** **entry_point 工作原理**

那么  entry_point 到底是什么，和 ASM 中的 Opcode 又有什么联系呢？

在JVM初始化期间会调用 TemplateInterpreterGenerator::generate_all()函数会生成许多例程（也就是机器指令片段，英文叫Stub）、

包括调用set_entry_points_for_all_bytes()函数生成各个字节码对应的例程。最终会调用到TemplateInterpreterGenerator::generate_and_dispatch()函数。

Interpreter 是什么：

Interpreter是对外的一个解释器的包装类，通过宏定义的方式决定使用CppInterpreter或者TemplateInterpreter。

InterpreterCodelet表示一段解释器代码，所有的解释器代码都放在InterpreterCodelet中。

AbstractInterpreter的定义位于hotspot src/share/vm/interpreter/abstractInterpreter.hpp中，

是CppInterpreter和TemplateInterpreter共同的基类，用来抽象平台独立的解释器相关的属性和方法。

AbstractInterpreter定义的属性都是protected，如下：

```
_code：StubQueue*，用来保存生成的汇编代码的_notice_safepoints：bool，是否激活了安全点机制_native_entry_begin：address，JIT编译器产生的本地代码在内存中的起始位置_native_entry_end：address，JIT编译器产生的本地代码在内存中的终止位置_entry_table：address数组，处理不同类型的方法的方法调用的入口地址，数组的长度就是枚举number_of_method_entries的值_native_abi_to_tosca：address数组，处理不同类型的本地方法调用返回值的入口地址，数组的长度是枚举number_of_result_handlers的值，目前为10_slow_signature_handler：address，本地方法生成签名的入口地址_rethrow_exception_entry：address，重新抛出异常的入口地址AbstractInterpreter定义了一个表示方法类型的枚举MethodKind，每个类型都对应_entry_table中一个数组元素，即一个处理该类型方法的方法调用的入口地址
```

初始化调用顺序如下：

```
hotspot/src/share/vm/runtime/init.cpp init_globals()     =>    全局模块初始化入口hotspot/src/share/vm/interpreter/interpreter.cpp interpreter_init()     =>    模板解释器初始化hotspot/src/share/vm/interpreter/templateInterpreter TemplateInterpreterGenerator::generate_all()     =>    生成每中Java方法类型的entry_pointhotspot/src/share/vm/interpreter/templateInterpreter.cpp TemplateInterpreterGenerator::set_entry_points_for_all_bytes()    =>    循环对 DispatchTable 中的 Bytecodes::Code 调用 set_entry_points() 进行设置hotspot/src/share/vm/interpreter/templateInterpreter.cpp TemplateInterpreterGenerator::set_entry_points()hotspot/src/cpu/x86/vm/templateInterpreter_x86_64.cpp TemplateInterpreterGenerator::set_vtos_entry_points()     =>    将字节码指令生成汇编代码
```

为了方便的找出不同的Java方法需要的通用 entry_point 入口，会将一共 MethodKind 种 entry_point 缓存到表种：

hotspot/src/share/vm/interpreter/interpreter.cpp set_entry_for_kind()     =>    将entry保存到_entry_table缓存中

目前 MethodKind 定义如下：

```
enum MethodKind {  zerolocals,                                                 // method needs locals initialization  zerolocals_synchronized,                                    // method needs locals initialization & is synchronizednative,                                                     // native methodnative_synchronized,                                        // native method & is synchronized  empty,                                                      // empty method (code: _return)  accessor,                                                   // accessor method (code: _aload_0, _getfield, _(a|i)return)  abstract,                                                   // abstract method (throws an AbstractMethodException)method_handle_invoke_FIRST,                                 // java.lang.invoke.MethodHandles::invokeExact, etc.method_handle_invoke_LAST                                   = (method_handle_invoke_FIRST+ (vmIntrinsics::LAST_MH_SIG_POLY                                                                    - vmIntrinsics::FIRST_MH_SIG_POLY)),  java_lang_math_sin,                                         // implementation of java.lang.Math.sin   (x)  java_lang_math_cos,                                         // implementation of java.lang.Math.cos   (x)  java_lang_math_tan,                                         // implementation of java.lang.Math.tan   (x)  java_lang_math_abs,                                         // implementation of java.lang.Math.abs   (x)  java_lang_math_sqrt,                                        // implementation of java.lang.Math.sqrt  (x)  java_lang_math_log,                                         // implementation of java.lang.Math.log   (x)  java_lang_math_log10,                                       // implementation of java.lang.Math.log10 (x)  java_lang_math_pow,                                         // implementation of java.lang.Math.pow   (x,y)  java_lang_math_exp,                                         // implementation of java.lang.Math.exp   (x)  java_lang_ref_reference_get,                                // implementation of java.lang.ref.Reference.get()  java_util_zip_CRC32_update,                                 // implementation of java.util.zip.CRC32.update()  java_util_zip_CRC32_updateBytes,                            // implementation of java.util.zip.CRC32.updateBytes()  java_util_zip_CRC32_updateByteBuffer,                       // implementation of java.util.zip.CRC32.updateByteBuffer()number_of_method_entries,invalid = -1};
```

在进行方法链接时将 entry_point 设置到对应的属性上：

hotspot/src/share/vm/oops/method.cpp Method::link_method()     =>    调用entry_for_method()获取已经创建好的entry,然后调用set_interpreter_entry()保存到目标属性上。

最终在 stubGenerator_x86_64.cpp  的  generate_call_stub() 函数种会触发 entry_point 的调用。

在执行 entry_point 时，普通的 Java 方法（也就是MethodKing=zerolocals）会最终进入 InterpreterGenerator::generate_normal_entry(bool synchronized)。

这个是普通 Java 函数调用时，entry_point 的入口，这一步会跳转到 Java 方法的第一个字节码指令：

```
InterpreterGenerator::generate_normal_entry(bool synchronized) 代码如下：address InterpreterGenerator::generate_normal_entry(bool synchronized) {  bool inc_counter  = UseCompiler || CountCompiledCalls;// ebx: Method*// r13: sender sp// entry_point函数的代码入口地址address entry_point = __ pc();// 当前rbx中存储的是指向Method的指针，通过Method*找到ConstMethod*  const Address constMethod(rbx, Method::const_offset());  const Address access_flags(rbx, Method::access_flags_offset());  const Address size_of_parameters(rdx,ConstMethod::size_of_parameters_offset());  const Address size_of_alocals(rdx, ConstMethod::size_of_locals_offset());// get parameter size (always needed)// 上面已经说明了获取各种方法元数据的计算方式，// 但并没有执行计算，下面会生成对应的汇编来执行计算// 计算ConstMethod*，保存在rdx里面  __ movptr(rdx, constMethod);  __ load_unsigned_short(rcx, size_of_parameters);  __ load_unsigned_short(rdx, size_of_locals); // get size of locals in words  __ subl(rdx, rcx); // rdx = no. of additional localsgenerate_stack_overflow_check();// get return address// 返回地址是在CallStub中保存的，如果不弹出堆栈到rax，中间// 会有个return address使的局部变量表不是连续的，// 这会导致其中的局部变量计算方式不一致，所以暂时将返// 回地址存储到rax中  __ pop(rax);// compute beginning of parameters (r14)// 计算第1个参数的地址：当前栈顶地址 + 变量大小 * 8 - 一个字大小// 注意，因为地址保存在低地址上，而堆栈是向低地址扩展的，所以只// 需加n-1个变量大小就可以得到第1个参数的地址  __ lea(r14, Address(rsp, rcx, Address::times_8, -wordSize));// rdx - # of additional locals// allocate space for locals// explicitly initialize locals// 把函数的局部变量设置为0,也就是做初始化，防止之前遗留下的值影响// rdx：被调用方法的局部变量可使用的大小  {    Label exit, loop;    __ testl(rdx, rdx);    __ jcc(Assembler::lessEqual, exit); // do nothing if rdx <= 0    __ bind(loop);    __ push((int) NULL_WORD); // initialize local variables    __ decrementl(rdx); // until everything initialized    __ jcc(Assembler::greater, loop);    __ bind(exit);  }// initialize fixed part of activation frame// 生成固定桢generate_fixed_frame(false);  const Address do_not_unlock_if_synchronized(r15_thread,in_bytes(JavaThread::do_not_unlock_if_synchronized_offset()));  __ movbool(do_not_unlock_if_synchronized, true);  __ profile_parameters_type(rax, rcx, rdx);  Label invocation_counter_overflow;  Label profile_method;  Label profile_method_continue;if (inc_counter) {generate_counter_incr(&invocation_counter_overflow,&profile_method,&profile_method_continue);if (ProfileInterpreter) {      __ bind(profile_method_continue);    }  }  Label continue_after_compile;  __ bind(continue_after_compile);  bang_stack_shadow_pages(false);  __ movbool(do_not_unlock_if_synchronized, false);// 如果是同步方法时，还需要执行lock_method()函数，所以// 会影响到栈帧布局if (synchronized) {// Allocate monitor and lock methodlock_method();  } else {// no synchronization necessary#ifdef ASSERT    {      Label L;      __ movl(rax, access_flags);      __ testl(rax, JVM_ACC_SYNCHRONIZED);      __ jcc(Assembler::zero, L);      __ stop("method needs synchronization");      __ bind(L);    }#endif  }// start execution#ifdef ASSERT  {    Label L;     const Address monitor_block_top (rbp,                 frame::interpreter_frame_monitor_block_top_offset * wordSize);    __ movptr(rax, monitor_block_top);    __ cmpptr(rax, rsp);    __ jcc(Assembler::equal, L);    __ stop("broken stack frame setup in interpreter");    __ bind(L);  }#endif// jvmti support  __ notify_method_entry();// 跳转到目标Java方法的第一条字节码指令，并执行其对应的机器指令,也就是指令分派了  __ dispatch_next(vtos);return entry_point;}
```

执行到这里，栈帧中会创建Java方法内部的会创建局部变量：

![](https://img-blog.csdnimg.cn/38ea8e079b1b4c54849cb67138f5eb95.png)

在上面会调用  generate_fixed_frame(false); 生成固定帧栈，会保存下列一些寄存器：

```
rbx：Method*ecx：invocation counterr13：bcp(byte code pointer)rdx：ConstantPool* 常量池的地址r14：本地变量表第1个参数的地址
```

得到最终的栈帧结构：

![](https://img-blog.csdnimg.cn/5db6916306f8468780ac0a300a2a05b5.png)

最终就进入 entry_point 中记录的 Java 方法第一个字节码指令并进行调用了。

其中从 generate_fixed_frame() 函数生成Java方法调用栈帧的时候，如果当前是第一次调用，那么r13指向的是字节码的首地址，即第一个字节码，此时的step参数为0。

r13指向字节码的首地址，当第1次调用时，参数step的值为0，

那么load_unsigned_byte()函数从r13指向的内存中取一个字节的值，取出来的是字节码指令的操作码。

增加r13的步长，这样下次执行时就会取出来下一个字节码指令的操作码。

```
void InterpreterMacroAssembler::dispatch_next(TosState state, int step) {  load_unsigned_byte(rbx, Address(r13, step));// r13指向字节码的首地址，当第1次调用时，参数step的值为0，// 那么load_unsigned_byte()函数从r13指向的内存中取一个字节的值，取出来的是字节码指令的操作码。// 增加r13的步长，这样下次执行时就会取出来下一个字节码指令的操作码。  increment(r13, step);// 返回当前栈顶状态的所有字节码入口点  dispatch_base(state, Interpreter::dispatch_table(state));}
```

接下来执行 dispatch_base()，获取当前栈顶状态字节码转发表的地址并跳转到字节码对应的入口执行机器码指令：

```
void InterpreterMacroAssembler::dispatch_base(TosState state, // 表示栈顶缓存状态address* table,                                              bool verifyoop) {// 省略// 获取当前栈顶状态字节码转发表的地址，保存到rscratch1  lea(rscratch1, ExternalAddress((address)table));// 跳转到字节码对应的入口执行机器码指令  jmp(Address(rscratch1, rbx, Address::times_8));}
```

而 Opcode 保存在对应栈顶状态 state (btos/ctos/stos/atos/itos/ftos/dtos/vtos共八种状态)的二位数组中 _table 中：

![](https://img-blog.csdnimg.cn/cc6b743541114efa82dd6935042ca2fc.png)
