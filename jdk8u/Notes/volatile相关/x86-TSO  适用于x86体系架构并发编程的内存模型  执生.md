---
source: https://www.lmlphp.com/user/13901/article/item/437069/
---
 Abstract :

　　如今大数据，云计算，分布式系统等对算力要求高的方向如火如荼。提升计算机算力的一个低成本方法是增加CPU核心，而不是提高单个硬件工作效率。

　　这就要求软件开发者们能准确，熟悉地运用高级语言编写出能够充分利用多核心CPU的软件，同时程序在高并发环境下要准确无误地工作，尤其是在商用环境下。

　　但是做为软件工程师，实际上不太可能花大量的时间精力去研究CPU硬件上的同步工作机制。

　　退而求其次的方法是总结出一套比较通用的内存模型，并且运用到并发编程中去。

　　本文结合对CPU的黑盒测试，介绍一个能够通用于 x86 系列CPU的并发编程的内存模型。

　　此内存模型 被测试在 AMD 与 x86 系列CPU上具有可行性，正确性。

___

本文章节：

　　1.关于各型号CPU的说明书规定或模型规定

　　2.官方发布的黑盒测试及它们可复现/不可复现的CPU型号

　　3.指令重排的发生

　　4.根据黑盒测试定义抽象内存模型 x86-TSO

　　5._**A Rigorous and Usable Programmer’s Model for x86 Multiprocessors**_ 的发布团队在AMD/Intel系列CPU上进行的一系列黑盒测试及它们与x86-TSO模型结构的关系

　　6.扩展

　　　　6.1 通过Hotspot源码分析 java volatile 关键字的语意及其与x86-TSO/普通TSO内存模型的关系

　　　　6.2 Linux内存屏障宏定义 与 x86-TSO 模型的关系

　　7.总结

　　8.参考文献

___

**1.各个型号CPU的规定**

CPU相关的模型或规定：

　　x86-CC 模型（出自 _The semantics of x86-CC multiprocessor machine code. In Proc. POPL 2009, Jan. 2009_）

　　IWP (_Intel White Paper_，英特尔在2007年8月发布的一篇CPU模型准则，内容给出了P1~P8共8条规则，并且用10个在CPU上的黑盒测试

　　来支持这8条规则)

　　Intel SDM (继承IWP的Intel 模型)

　　AMD3.14 说明手册对CPU的规定

___

**2.官方黑盒测试**　　

　　下面的可在...观察到，不可在...观察到，都是针对 最后的 State 而言，亦即 Proc 0: EAX = 0 ^ ... 语句表示的最终状态

　　其中倒着的V是且的意思

　　以下提到的StoreBuffer即CPU核心暂时缓存写入操作的物理部件，StoreBuffer中的写入操作会在任意时刻被刷入共享存储（主存/缓存），前提是总线没被锁/缓存行没被锁

　　以下提到的Store Fowarding 指的是CPU核心在读取内存单元时 会去 StoreBuffer中寻找该变量，如果找到了就读取，以便得到该内存单元在本核心上最新的版本

___

　  1.测试SB : 可在现代Intel CPU 和 AMD x86 中观察到

![x86-TSO : 适用于x86体系架构并发编程的内存模型-LMLPHP](https://c1.lmlphp.com/user/master/2020/09/20/637eff282830f6010eb6288b9acfb6e3.png "x86-TSO : 适用于x86体系架构并发编程的内存模型-LMLPHP")

　　核心0 和 核心1 各有自己的Store Buffer，会造成上述情况。

　　核心0将 x = 1 缓存在自己的StoreBuffer里，并且从共享内存中获取 y = 0，之所以见不到 y = 1 是因为 核心1 将 y = 1 的操作缓存在自己的StoreBuffer里

　　核心1将 y = 1 缓存在自己的StoreBuffer里，并且从共享内存中获取 x = 0，无法见到 x = 1 与上述同理

___

　  2.测试IRIW : 实际上不允许在任何CPU上观察到，但是有的CPU模型的描述可能让该测试发生

    ![x86-TSO : 适用于x86体系架构并发编程的内存模型-LMLPHP](https://c1.lmlphp.com/user/master/2020/09/20/87f9b5ca2e3ce43891afd4989f6790b7.png "x86-TSO : 适用于x86体系架构并发编程的内存模型-LMLPHP")

　　如果 核心0 和 核心2 共享 StoreBuffer，因为读取时会先去 StoreBuffer 读取修改，所以 核心0 执行的 x = 1 会被 CPU2 读取，故 EAX = 1 , 因为核心1 和 核心2 不共享StoreBuffer，核心1 的 y = 1 操作缓存在自己和核心3的共享StoreBuffer中

　　所以 EBX = 0 ， CPU3的寄存器状态同理 

___

　 3.测试n6 : 在 Intel Core 2 上可以观察到，但却是被X86-CC模型和IWP说明禁止的

![x86-TSO : 适用于x86体系架构并发编程的内存模型-LMLPHP](https://c1.lmlphp.com/user/master/2020/09/20/022cb0ffd0968696cd219a59c520a405.png "x86-TSO : 适用于x86体系架构并发编程的内存模型-LMLPHP")

　　核心0 和 核心1 有各自的Store Buffer

　　核心0将 x = 1 缓存在自己的Store Buffer 中，并且根据 Store Forwarding 原则 , 核心0 读取 x 到 EAX 的时候

　　会读取自己的Store Buffer，读取x 的值是 1，故EAX = x = 1

　　同理，核心1也会缓存自己的写操作， 即缓存 y = 2 和 x = 2 到自己的 StoreBuffer，因此 y = 2 这个操作不会被

　　核心0 观察到，核心0 从主内存中加载 y 。故 EBX = y = 0

___

　　4.测试n5 / n4b ：实际上不能在任何CPU上观察到

   ![x86-TSO : 适用于x86体系架构并发编程的内存模型-LMLPHP](https://c1.lmlphp.com/user/master/2020/09/20/289aa4b953aa8267a7464c5c5bf122f8.png "x86-TSO : 适用于x86体系架构并发编程的内存模型-LMLPHP")

![x86-TSO : 适用于x86体系架构并发编程的内存模型-LMLPHP](https://c1.lmlphp.com/user/master/2020/09/20/099d2f8589f89280076b5a04927e7e47.png "x86-TSO : 适用于x86体系架构并发编程的内存模型-LMLPHP")

　　n5和n4b是同样类型的测试，假如 核心0和 核心1都有自己的StoreBuffer

　　对于n5，核心0的EAX如果等于2，那么说明 核心1的StoreBuffer刷入到 共享的主存中, 那么 EAX = x 必然=执行于 x = 2

　　之后，一位 x = 1 对 EAX = x 来说是有影响的，EAX = x 和 x = 1 禁止重排，那么 x = 1 必然不能出现在 x = 2 之前，

　　更不可能出现在 ECX = x 之前 （x = 2 对 ECX = x 也是有影响的，语义上严禁重排）

　　对于n4b，如果EAX = 2 ，说明 核心1的 x = 2 操作已经刷入主存被 核心0 观察到，那么对于 核心0来说 x = 2 先于 EAX = x 执行

　　同上理，ECX = x 和 x = 2 也是严禁重排的，故 ECX = x 要先于 EAX = x 执行，更先于 x = 1 执行

　　n5 和 n4b 测试在AMD3.14 和Intel SDM 中好像是可以被允许的，也就是上面的 Forbidden Final State 被许可，但实际上不能

　　_A Rigorous and Usable Programmer’s Model for_ _x86_ _Multiprocessors_ 中作者原话 ： However, the principles stated in revisions 29–34 of the Intel SDM appear, presumably unintentionally, to allow them. The AMD3.14 Vol. 2, §7.2 text taken alone would allow them, but the implied coherence from elsewhere in the AMD manual would forbid them

___

　　实际上是概念模型如果从局部描述，没有办法说确切，但是从整体上看，整个模型的说明很多地方都禁止了n5和n4b的发生，可见想描述一个松散执行顺序的CPU模型是多么难的一件事。软件开发者也没办法花大量的时间精力去钻研硬件的结构组成和工作原理，只能依靠硬件厂商提供的概念模型去理解硬件的行为

　　在AMD3.15的模型说明中，语言清晰地禁止了IRIW，而不是模棱两可的否定。

　　以下表格总结了上述的黑盒测试在不同CPU模型中的观察结果（3.14 3.15是AMD不同版本的模型，29~34是Intel SDM在这个版本范围的模型）

    ![x86-TSO : 适用于x86体系架构并发编程的内存模型-LMLPHP](https://c1.lmlphp.com/user/master/2020/09/20/674d30fbe63de4ef042beec7e6244123.png "x86-TSO : 适用于x86体系架构并发编程的内存模型-LMLPHP")

___

 **3.指令重排的发生**　

　　上述的黑盒测试的解释中，提到了重排的概念，让我们看一下从软件层面的指令到硬件上，哪些地方可能出现 重排序：

　　CPU接收二进制指令流，流水线设计的CPU会依照流水线的方式串行地执行每条指令的各个阶段

　　举例描述：一个餐厅里 每个人的就餐需要三个阶段：盛饭，打汤，拿餐具。有三个员工，每人各负责一个阶段，显然每个员工只能同时处理一个客人，也就是同一时刻，同一阶段只能有一个客人，也就是任何时刻不可能出现两位客人同时打汤的情况，当然也不可能出现两个员工同时服务一个客人。能形成一条有条不紊的进餐流水线，不用等一个客人一口气完成3个阶段其他客人才能开始盛饭。

　　对于CPU也是，指令的执行分为 取指，译码，取操作数，写回内存 等阶段，每个阶段只能有一条指令在执行。

　　CPU应当有 取值工作模块，译码工作模块，等等模块来执行指令。每种模块同一时刻只能服务一条指令，对于CPU来说，流水线式地执行指令，是串行的，没有CPU聪明到给指令重排序一说，如果指令在CPU内部的执行顺序和高级语言的语义顺序不一样，那么很可能是编译器优化重排，导致CPU接受到的指令

本来就是编译器重排过的。

　　真正的指令重排出现在StoreBuffer的不可见上，缓存一致性已经保证了CPU间的缓存一致性。具体重拍的例子就是第一个黑盒测试SB: 

　　初值：x = 0, y = 0

     ![x86-TSO : 适用于x86体系架构并发编程的内存模型-LMLPHP](https://c1.lmlphp.com/user/master/2020/09/20/9654ebdb4eda39f3043e8ed7025ffdac.png "x86-TSO : 适用于x86体系架构并发编程的内存模型-LMLPHP")

　  在我们常规的并发编程思维中，会为这4条语句排列组合（按我们的认知，1一定在2之前，3一定在4之前），并且认为无论线程怎么切换，这四条语句的排列组合（1在2前，3在4前的组合）一定有一条符合最后的实际运行结果。假如按照 1 3 2 4 这样的组合来执行，那么最后 EAX = 1 , EBX = 1，或者 1 2 3 4 这样的顺序来执行，最后 EAX = 0 且 EBX = 1，怎么都不可能 EAX = 0, EBX = 0 

　  但实际上SB测试可以在Intel系列上观察到，从软件开发者的角度上看，就好像 按照 2 4 1 3 的顺序执行了一样，如同 2 被排在1 之前，3 被排在4 之前，是所谓 **指令重排** 的一种情况 

　  实际上，是因为StoreBuffer的存在，才导致了上述的指令重排。

　  试想一下，CPU0将 x = 1 指令的执行缓存在了自己的StoreBuffer里，CPU1也把 y = 1 的执行缓存在自己的StoreBuffer里，这样的话当两者执行各自的读取操作的时候，亦即CPU0执行 EAX = y

CPU1执行 EBX  = x , 都会直接去缓存或主存中读取，而缓存又MESI协议保证一致，但是不保证StoreBuffer一致，所以两者无法互相见到对方的StoreBuffer中对变量的修改，于是读到x, y都是初值 0

___

**4.根据黑盒测试定义抽象内存模型 x86-TSO**

    　从以上的试验无法总结出一套通用的内存模型，因为每个CPU的实现不同，但是我们可以总结出一个合理的关于x86的内存模型

　　并且这个模型适合软件开发者参考，并且符合CPU厂商的意图

　　首先是关于StoreBuffer的设计：

　　　　1.在Intel SDM 和 AMD3.15模型中，IRIW黑盒测试是明文禁止的，而IRIW测试意味着某些CPU可以共享StoreBuffer所以我们想创造的合理内存模型不能让CPU共享StoreBuffer

　　　　2.但是在上述黑盒测试中，比如n6和SB，都证明了，StoreBuffer确实存在

　　　　总结:StoreBuffer存在且每个CPU独占自己的StoreBuffer

　　上述的黑盒测试表明，除了StoreBuffer造成的CPU写不能马上被其他CPU观察到，各个CPU对主存的观察应该都是一致的，可以忽略掉缓存行，因为MESI协议会保证各个CPU的缓存行之间的一致性，但是无法保证StoreBuffer中的内容的一致性，因为MESI是缓存一致性协议，每个字母对应缓存（cache）的一种状态，保证的只是缓存行的一致性。

　 总结一下：我们想构造的x86模型的特点：

　　　1. 在硬件上必然是有StoreBuffer存在的，设计时需要考虑进去

　　   2. 缓存方面因为MESI协议，各个CPU的缓存之间不存在不一致问题，所以缓存和主存可以抽象为一个共享的内存

　　   3. 额外的一个特点是 总线锁，x86提供了 lock 前缀 ，lock前缀可以修饰一些指令来达到 read-modify-write 原子性的效果，比如最常见的 read-modify-write 指令 ADD，CPU需要从内存中取出变量，

加一后再写回内存，lock前缀可以让当前CPU锁住总线，让其他CPU无法访问内存，从而保证要修改的变量不会在修改中途被其他CPU访问，从而达到原子性 ADD 的效果。在x86中还有其他的指令自带

lock 前缀的效果，比如XCHG指令。带锁缓存行的指令在锁释放的时候会把StoreBuffer刷入共享存储

　最后可以得到如下模型：

　　![x86-TSO : 适用于x86体系架构并发编程的内存模型-LMLPHP](https://c1.lmlphp.com/user/master/2020/09/20/778bd8a0dfce9772c8d016c9d398fea1.png "x86-TSO : 适用于x86体系架构并发编程的内存模型-LMLPHP")

　　其中各颜色线段的含义：

　　红色：CPU的各个核心可以争夺总线锁，占有总线锁的CPU可以将自己的storeBuffer刷入到共享存储中（其实总线锁不是真的一定要锁总线，也可以锁缓存行，如果要锁的目标不只一个缓存行，则锁总线），占有期间其他CPU无法将自己的storeBuffer刷入共享存储

　　橙色：根据StoreFowarding原则，CPU核心读取内存单元时，首先去StoreBuffer读取最近的修改，并且x86的StoreBuffer是遵循FIFO的队列，x86不允许CPU直接修改缓存行，所以StoreStore内存屏障在x86上是空操作，因为对于一个核心来说，写操作都是FIFO的，写操作不会重排序。x86不允许直接修改缓存行也是缓存和主存能抽象成一体的原因。

　　紫色:  核心向StoreBuffer写入数据，在一些英文文献中会表示为：Buffered writes to 变量名，也就是对变量的写会被缓存在StoreBuffer，暂时不会被其他CPU观察到

　　绿色:  CPU核心将缓存的写操作刷入共享存储，除了有其他核心占有锁的情况 (因为其他核心占有锁的话，锁住了缓存行或总线，则当前核心不能修改这个缓存行或访问共享存储)，任何情况下，StoreBuffer都可以被刷入共享存储

　　蓝色：如果要读取的变量不在StoreBuffer中，则去共享存储读取（缓存或主存）

___

**5._A Rigorous and Usable Programmer’s Model for x86 Multiprocessors_ 的发布团队在AMD/Intel系列CPU上进行的一系列黑盒测试及它们与x86-TSO模型结构的关系**

　　在_A Rigorous and Usable Programmer’s Model for_ _x86_ _Multiprocessors_ 一文中，作者为了验证x86-TSO模型的正确性，在普遍流行的 AMD和 Intel 处理器上使用嵌入汇编的C程序进行测试。

并且使用memevents工具监视内存并且查看最终结果,并且用HOL4监控指令执行前后寄存器和内存的状态，最后进行验证，共进行了4600次试验。

　　结果如下：

       _1.写操作不允许重排序，无论是对其他核心来说还是自己来说_

　　_![x86-TSO : 适用于x86体系架构并发编程的内存模型-LMLPHP](https://c1.lmlphp.com/user/master/2020/09/20/df57e7c6630fcf2df17e636899b81269.png "x86-TSO : 适用于x86体系架构并发编程的内存模型-LMLPHP")_

　　解释：

　　从核心1 的角度看 核心0，x 和 y 的写入顺序不能颠倒　　

　　从x86-TSO模型的物理构件角度解释就是，写操作会按照 FIFO 的规则 进入 StoreBuffer，并且按照FIFO的顺序刷入共享存储，所以 写操作无法重排序。

　　所以 x = 1 写操作先入StoreBuffer队列，接着 y = 1 入，刷入共享存储的时候， x = 1 先刷入，y = 1 再刷入， 所以如果 y 读到 1 的话，x 一定不能是 0

___

　　_2.读操作不能延迟 ：对于其他核心来说，对于自己来说如果不是同一个内存单元，是否重排无关紧要，（因为读不能通知写，只有写改变了某些状态才能通知读去做些什么， 比如 x = 1; if (x == 1); ）_

　　![x86-TSO : 适用于x86体系架构并发编程的内存模型-LMLPHP](https://c1.lmlphp.com/user/master/2020/09/20/8541af9d5ab6e2a6c0e2c9bd4044bcee.png "x86-TSO : 适用于x86体系架构并发编程的内存模型-LMLPHP")

　　解释：

　　从核心1的角度观察，如果EBX = 1 , 那么说明 核心0的 y = 1 操作已经从 StoreBuffer中刷入到共享存储，之前说过，CPU流水线执行指令在物理上是不能对指令流进行重排的，所以 EAX = x 的操作 在 y = 1 之前执行

　　同理，EBX = y 这个读操作也不能延后到 x = 1 之后执行，所以 EAX = x 先于 y = 1 ，y = 1 先于 EBX = y , EBX = y 先于 x = 1 , 所以 EAX 不可能接受到 x = 1 的结果

___

　　_3.读操作可以提前：上述2是读操作不能延后，但是可以提前，并且是从其他核心的角度观察到的_

　　例子是 上面的 SB测试，

　　![x86-TSO : 适用于x86体系架构并发编程的内存模型-LMLPHP](https://c1.lmlphp.com/user/master/2020/09/20/637eff282830f6010eb6288b9acfb6e3.png "x86-TSO : 适用于x86体系架构并发编程的内存模型-LMLPHP")

　　如果忽视StoreBuffer，从核心1的角度看，EBX = 0，说明 x = 1 未执行，那么 EBX = x 应该在 x = 1 之前执行，又因为 y = 1 在 EBX = x 之前，那么 y = 1 应该在 x = 1 之前，EAX = y 在 x = 1 之后，理应在 y = 1 之后

　　那么EAX 理应 = 1。

　　![x86-TSO : 适用于x86体系架构并发编程的内存模型-LMLPHP](https://c1.lmlphp.com/user/master/2020/09/20/68d072f5dfa699c90b449189ea439a30.png "x86-TSO : 适用于x86体系架构并发编程的内存模型-LMLPHP")

　　但是最后状态可以两者均为0.。就好像 读取的指令被重新排到前面（下面是一种重排情况）

         ![x86-TSO : 适用于x86体系架构并发编程的内存模型-LMLPHP](https://c1.lmlphp.com/user/master/2020/09/20/c1f3d42f36a5464bb5f0f4bd1563c8ef.png "x86-TSO : 适用于x86体系架构并发编程的内存模型-LMLPHP")

 　在x86-TSO模型中，写操作是允许被提前的，从物理硬件的角度解释如下：

　 假设核心0是左边的核心，核心1是右边的核心，如果两者都把 写操作缓存在 StoreBuffer中，并且读操作执行之前，StoreBuffer没有同步回共享存储，那么两者读到的 x 和 y 都来自共享存储，并且都是 0。

    ![x86-TSO : 适用于x86体系架构并发编程的内存模型-LMLPHP](https://c1.lmlphp.com/user/master/2020/09/20/58c702d5fb01b2a90026d70d3096d367.png "x86-TSO : 适用于x86体系架构并发编程的内存模型-LMLPHP")

___

　　_4.对于单个核心来说，因为Store Forwarding 原则，同一个内存单元 之前的写操作必对之后的读操作可见_

 　　![x86-TSO : 适用于x86体系架构并发编程的内存模型-LMLPHP](https://c1.lmlphp.com/user/master/2020/09/20/d26709338bde9349851e6be0f9745651.png "x86-TSO : 适用于x86体系架构并发编程的内存模型-LMLPHP")

　　解释：对一个核心而言，自己的写入是能马上为自己所见的

___

 　　_5.写操作的可见性是传递的，这一点与 happens-before 原则的传递性类似，如果 A 能 看到 B 的动作，B能看到 C 的动作，那么 A一定能看到C 的动作_

　　![x86-TSO : 适用于x86体系架构并发编程的内存模型-LMLPHP](https://c1.lmlphp.com/user/master/2020/09/20/8b8c0934284ccc89aa330913e2676f43.png "x86-TSO : 适用于x86体系架构并发编程的内存模型-LMLPHP")

　　解释： 对于核心1，如果EAX = 1 ，那么说明核心1已经见到了 核心0的动作，对于核心2，EBX = 1，说明核心2已经见到了 核心1的动作，又根据之前的 读操作不能延后，EAX = x 不能延迟到 y = 1 之后，

　　所以 核心2 必能见到 核心0 的动作，所以 ECX = x 不能为 0

___

　　_6.共享存储的状态对所有核心来说都是一致的_

　　上面的IRIW测试就是违反共享内存一致性的例子：

　　![x86-TSO : 适用于x86体系架构并发编程的内存模型-LMLPHP](https://c1.lmlphp.com/user/master/2020/09/20/87f9b5ca2e3ce43891afd4989f6790b7.png "x86-TSO : 适用于x86体系架构并发编程的内存模型-LMLPHP")

　　核心2 和 核心3 观察到 核心0 和 核心1 的行为，那么他们的行为应该都是一样的，因为都刷入到共享存储中了

___

　　**7.带lock前缀的指令或者 XCHG指令，会清空StoreBuffer，使得之前的写操作马上被其他核心观察到**

　　![x86-TSO : 适用于x86体系架构并发编程的内存模型-LMLPHP](https://c1.lmlphp.com/user/master/2020/09/20/82d664baaf8e52b093c69a84053712aa.png "x86-TSO : 适用于x86体系架构并发编程的内存模型-LMLPHP")

　　解释：EAX一开始是1，将EAX的值写入 x ，核心0 的 XCHD会把StoreBuffer清空，亦即将 x = EAX (1) 刷入共享存储，核心1如果看到 x = 0 (EDX = 0 )，说明 x = EAX 在后面才执行。推得y = ECX 在 x = EAX 之前执行

　　y = ECX 也会马上刷入共享存储，必然对 核心 0 可见，所以 最后不可能 x = 0, y = 0

　　![x86-TSO : 适用于x86体系架构并发编程的内存模型-LMLPHP](https://c1.lmlphp.com/user/master/2020/09/20/9274723215927e86e603cceca0e35132.png "x86-TSO : 适用于x86体系架构并发编程的内存模型-LMLPHP")

　　MFENCE指令在x86-TSO模型上也能达到刷StoreBuffer的效果。

___

　　总结：在x86-TSO模型上，唯一可能重排序的清空是 读被提前了（从多个CPU的视角看），实际上是 StoreBuffer缓存了写操作，导致写操作没写出来。

___

**6.扩展：**

　　**6.1 通过Hotspot源码分析 java volatile 关键字的语意及其与x86-TSO/普通TSO内存模型的关系**

　　_**Java 的 volatile语意：Hotspot实现**_

　　![x86-TSO : 适用于x86体系架构并发编程的内存模型-LMLPHP](https://c1.lmlphp.com/user/master/2020/09/20/5a34753b1ae4e3d2d6ceff7c2ca619ee.png "x86-TSO : 适用于x86体系架构并发编程的内存模型-LMLPHP")

　　在JVM的字节码解释器中，如果putstatic字节码或putfield字节码的变量是java层面的volatile关键字修饰的，就会在指令执行的最后插入一道 storeLoad 屏障，前文已经说过，在x86中

　　唯一可能重排的是 读操作提前到 写操作之前，这里的storeLoad操作做的就是阻止重排

　　![x86-TSO : 适用于x86体系架构并发编程的内存模型-LMLPHP](https://c1.lmlphp.com/user/master/2020/09/20/10d608ca2036d241b0781ce719c59b13.png "x86-TSO : 适用于x86体系架构并发编程的内存模型-LMLPHP")

　　在os_cpu/linux_x86下的实现如下：

　　其实只是执行了一条带lock前缀的空操作嵌入汇编指令（栈顶指针+0是空操作），实现的效果就是把StoreBuffer中的内容刷入到 共享存储中

　　其实还有一层加强， __ asm __ 后面的volatile关键字 会阻止编译器对本条指令前后的指令重排序优化，这保证了CPU得到的指令流是符合我们程序的语意的

 　　![x86-TSO : 适用于x86体系架构并发编程的内存模型-LMLPHP](https://c1.lmlphp.com/user/master/2020/09/20/bccbf5a2fe50acb5dad74b155ac157f4.png "x86-TSO : 适用于x86体系架构并发编程的内存模型-LMLPHP")

 　　在模板解释器中： 

　　![x86-TSO : 适用于x86体系架构并发编程的内存模型-LMLPHP](https://c1.lmlphp.com/user/master/2020/09/20/6711eb5d118642cc32c5204b0d662fdd.png "x86-TSO : 适用于x86体系架构并发编程的内存模型-LMLPHP")

 　  ![x86-TSO : 适用于x86体系架构并发编程的内存模型-LMLPHP](https://c1.lmlphp.com/user/master/2020/09/20/7ce3a011f1bcbc6e139c06563f55e471.png "x86-TSO : 适用于x86体系架构并发编程的内存模型-LMLPHP")

　　一开始我惊讶于此，这句话没有为不同平台实现选择不同的实现方法，而是简单检查如果不是需要 storeLoad 屏障就跳过。

 　 ![x86-TSO : 适用于x86体系架构并发编程的内存模型-LMLPHP](https://c1.lmlphp.com/user/master/2020/09/20/52df6afa8c590a6f5faad67dde29bc97.png "x86-TSO : 适用于x86体系架构并发编程的内存模型-LMLPHP")

　 其实作者注解已经说的很明白，现在大部分RISC的SPARC架构的CPU的实现都满足TSO模型，所以只需要StoreLoad屏障而已

　 我在[新南威尔斯大学的网站](https://www.lmlphp.com/r?x=CtQ9yf-Vsi2i2jwGyMT62Ew_2jwr4tT6PLTV5xI_pFg9oNuOoNbm4EI92LhryjunoqS_zLXVzxbY4Fa6CtQdztnupFY)上找到了关于TSO的比较正宗的解释：

  ![x86-TSO : 适用于x86体系架构并发编程的内存模型-LMLPHP](https://c1.lmlphp.com/user/master/2020/09/20/0ef83c6ad7fe7f2d7ece0ff91b360028.png "x86-TSO : 适用于x86体系架构并发编程的内存模型-LMLPHP")

 　下面的讨论中，是否需要重排序是根据CPU最后的行为是否符合我们高级语言程序的语意顺序决定的，如果相同则不需要，不相同则需要。

 　也就是单个核心中，写操作之间是顺序的，会按二进制指令流的写入顺序刷到共享存储，不需要考虑重排的情况，所以不需要StoreStore屏障

　 遵循Store Forwarding，所以对于本核心，本核心的写操作对后续的读操作可见。这三点已经十分符合本文的x86-TSO模型。

　 有一点没有呈现，就是单个CPU核心中，是否禁止读延迟，也就是写操作不能跨越到读操作之前，根据上面的 只有StoreLoad 屏障有操作，其他屏障，包括LoadStore屏障无操作可以推断

　 普通的TSO模型也是遵循禁止读延迟原则的

___

 　　**6.2 Linux内存屏障宏定义 与 x86-TSO 模型的关系**

 　　**_Linux 的内存屏障宏定义_**　 

![x86-TSO : 适用于x86体系架构并发编程的内存模型-LMLPHP](https://c1.lmlphp.com/user/master/2020/09/20/21b553e729099672d8c20b23ec0ee905.png "x86-TSO : 适用于x86体系架构并发编程的内存模型-LMLPHP")

 　　在Linux下定义的具有全功能的内存屏障，是有实际操作的，和JVM的storeLoad如出一辙

　　 读屏障是空操作，写屏障只是简单的禁止编译器重排序，防止CPU接收的指令流被编译器重排序。

 　　只要编译器能编译出符合我们高级语言程序语意顺序的二进制流给CPU，根据TSO模型的，CPU执行这些指令流的中的写操作对外部呈现出来的（刷入共享存储被所有CPU观察到）就是FIFO顺序

　　 读操作不涉及任何状态变更，所以不需要内存屏障（也许只有在x86上才不需要，在其他有Invaild Queue的CPU结构中或许需要）

___

**7.总结**

本文总结：

　　x86-TSO模型的特点总结：

　　因为缓存有MESI协议保证一致性，所以缓存可以和主存合并抽象成共享存储

　　x86-TSO的写操作严格遵循FIFO

　　CPU流水线式地执行指令会使得CPU对接受到的指令流顺序执行

　　x86-TSO中唯一重排的地方在于StoreBuffer，因为StoreBuffer的存在，核心的写入操作被缓存，无法马上刷新到共享存储中被其他核心观察到，所以就有了 “ 写 ” 比 “读” 晚执行的直观感受，也可以说是读操作提前了，排到了写操作前

　　阻止这种重排的方法是 使用带 lock 前缀的指令或者XCHG指令，或MFENCE指令，将StoreBuffer中的内容刷入到共享存储，以便被其他核心观察到

___

**8.本文参考文献**　

　参考文献:

　　《Linux内核源代码情景分析》：毛德操

　　_**x86-TSO: A Rigorous and Usable Programmer’s Model for x86 Multiprocessors**_

　　_**Memory Barriers: a Hardware View for Software Hackers**_

### 请赞赏

朋友，创作不易；为犒赏小编的辛勤劳动，请她喝杯咖啡吧！

给赞赏的您，运气会变好！

支付宝

![](data:image/png;base64,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)

微信

![](data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAAGQAAABkCAYAAABw4pVUAAAAAXNSR0IArs4c6QAAB71JREFUeF7tne2W4ygMRDvv/9A9JzmTCcGIewVOujer/TfxB6BSlUq07b18fX19fy389/09vuxyudzuRsfvQ/bnRdf3v9//PZv6WXPox6I5m7lF875GrwDpkovAfwsgUTb1SO5m8H0cm3U9k2bZZwMVnReNRefb60JW/FWV6/F/DClAHuHqk+ZHAYkyMJqkzfTd6w1TIvZF9er+O2l+du4kebO1HBhSgByF5SMAIbe1YOxul5jso1rSM8NKkq0VZo6tE21J8DKGFCAP629VZ1jU7cWUVdZNrep4hmHWsJALsvf5UYZEAS1ATmIIZZ7tQ2z/Eum5dT6j+doMjdZKyURyTMej2Ly0DylA4u0jBQgxoz9OVvDdx0eu5d1zoD7IxHh7LyuSh3cH42MA+bbWwcDbnJOtNdbdUdc961Uog8k5RiqRDM309EsB8ojPrwYk2qOi3+3GXETMLFNWstPKadYBktjQvt+N3RFDKPC0jZCVLGoQSW4ywPxqQPo/UK3uA1FmUwNJgJDLa4+T9JwJbiYRzBoOLqsA2Qlx7tpRYhwki7rMVSmK9DhaAuntbJ40x1zYjkU/uxNASf6029vXkAIkhotqT3RlCpCohlBNsA0h6WZWz2cubpVVtJZ+DcSQnb2wsIYUIPwokw08OdIW8H81xGYJAUXePUPfkQTMGLUqKdSBn8WICJgCBKp6Nml2mfIESPSg3GrXSQ0l3TfrutrxbGBol8Dex2T89RwarwCRj8FScr0UkDPdznWi5HgsE/pFmx4i24dYRqyyO3Jpo7Xop06sfe2Lejag1jTMgPlPA2IbQ9swksU7I+AtA9uszUoM9Rd0nJzpijvTWycFSPyQHtltK4m3RLsz5Cyak0TROFanR/ehjLT9BrE8W/8ycl+ANNEiyaNkieQ4Bci9DyHakcUjr3323ygoOE/evnn/YtQXkCOkukdAZBxs6LIyumeanwLk2AoMbe9qDbG1gvoGAiq6fsYQKz225mSTk2rVrC9ZriEFCD+7SzEa9WwoWVlHka0lUSNJzBot1mq9neNZjrBf46ymFCADlxUZnCiwNnmMROqXPkmXI3pSN0sMoRozynZqYiljM66oBSOaa8oR9rbXShQVOrvoAuQ54umHrS3a2Swl2tsEuN7H1hIak1gfuSkjTRGzChCBCslmD9wWINFur5jn0ym206e+gmoOdcUZTScW01xIbiMG9b+3awp3ewsQ3t19CSD24zPk3SPakg7bmpSxnNYR0tzoONljMjaj++saUoA8NMNK2SmAkJuxBcsWwhWdvV4zmifNfabdRqKpP6E6avoU/eQiDUaBtcV4h4kFiPD8ryh8H82QfvudbKmlPRVraznJ44/ma+9NRZnMAV0fJeNMzg/b7wXIsXhTUtgEINd2PY6bi1ntpy1rM6k2KaiQzoqxNRbE5qzsUlLPYlqATJ69tfJsHFpU9w5jRDVkN2vILZFro/FnMkHabyWI2ExzXGFWWEN2BytA4lo0Y9Tye+oEGG2BR5PK1qCZHtsaYiUnYla2ZiiXRXppLR7R1AJJwTTFnu6RBeItgNgXdmj/hhiRtYZ9sLIJcbOQ3WfPaduHOn1ir9kaoSQINxfp5rQ4KpzEyALkbwSykkKO5qzibhJgN4lW5xolFznJEVvw0xp2sFVG0KTp+KiJ7OuYvcevAoSKJDGHtNEGKTIFlP3t+IZNo/la4KyrojmPapJ+pa0A4ZQj40Em42ZEsp9nsjXDuqozFkGhiuZMFr2X4awRIRmfMiQanOhPNN/VZZNVHwlINlNtsbfNGdUwyra276BzqZ+g5NxlzizW+mHrsyWI+oyVwrkrp1lpI/YvrcG+sHNWF2v/vrKShSSfFCCSPqohFKPo/m1M9As7NBjJgJWkiDkUjOvxjwBk9QNm1qGQNBFQdHzWT/RzJOuelWWTJKNEmSWdfgzIFjqifTaL/3eA2O/2kv5ayTrLnY2y3dYn20GTfJIltzEZ1hBLPytBVi5oUZE0fiwg0RtUq3qazQrbp1AikFS2x8mgUBJQElEzrWoI0ZikhhhBi8iahJXa0tfB1b6D1rIFCPUhFChbpCkrqakjPb8ep3sQiFm20v2ItaPrsQ8pQOKwvgUQk4ntOVl6rt7f2O7sVsbq+ZGbI4BIJW4sp4ets82UtZ5RgMntzWRlNcA0JtWe3ePtvPVDDlS0qeiTa4uEIQvw6D4WKHse1Yad4wVIE71fAcju/4Mq23dQP0E6bKSO2Jg9TmukOdF4TzW5ADkKDPUZkfMkhlE9vhV1+1o0ZTa5J1okZaHpdyy7oiIcjZGtj9Z5DvuQAoQfOf0RQAydbpTqnpclR0GZby0jMXDUG2WuoXW0a4/OJckya8U3qCg7aCEFCG/pPCVT9JlYqhmktwTE7vWUCO1xqi3ZPSyT6aP5mXHSTy5Sce4dSDSJAmT8Xa/TACFGWa9O582YQaykzKa9puxuMjnDqcuytN6xdNeAZAtfRpoKkEm0aEfUSpvN6pHLshlKxoWc5WqSvoQhESYFyDEylCA3a22/SkoZTcdp15akjCS1lUPSeuojstJHDOtjM62DBcgjPNb5kfGwgI6A2d7LiphBkmWz2Hj3lTozqjl0HwLCGpBZzSlAxMMRkaW3AJCktclZgPwyQP4AZfStcMZCCRQAAAAASUVORK5CYII=)

¥5¥10¥15¥20¥30
