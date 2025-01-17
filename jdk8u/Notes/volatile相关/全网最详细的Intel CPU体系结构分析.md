---
source: https://zhuanlan.zhihu.com/p/447736172
---
受前段meldown漏洞事件的影响，那段时间也正好在读[Paul的论文](https://link.zhihu.com/?target=https%3A//www.oschina.net/action/GoToLink%3Furl%3Dhttp%253A%252F%252Fwww.rdrop.com%252Fusers%252Fpaulmck%252Fscalability%252Fpaper%252Fwhymb.2010.06.07c.pdf)关于内存屏障的知识，其中有诸多细节想不通，便陷入无尽的煎熬和冥想中，看了《计算机系统结构》、《深入理解计算机系统》、《大话处理器》等经典书籍，也在google上搜了一大堆资料，前前后后、断断续续的折腾了一个多月，终于想通了，现在把自己的思想心得记录下来，希望对有这方面困惑的朋友有些帮助。

本文主要关注以下几个问题。

-   **什么是CPU的流水线？为什么需要流水线？**
-   **为什么需要内存屏障？在只有单个Core的CPU中是否还需要内存屏障？**
-   **什么是乱序执行？分为几种？**
-   **MOB和ROB是干什么的？**
-   **load buffer和store buffer的功能是什么？**
-   **x86和arm、power中的memory model有什么区别？**
-   **MESI主要是做什么的？**
-   **meldown漏洞的原理是什么？**

几乎所有的冯·诺伊曼型计算机的 CPU，其工作都可以分为 5 个阶段：取指令、指令译码、执行指令、访存取数、结果写回。

![](https://pic3.zhimg.com/v2-d409dc5a0adefabae12c1a1570ddbcae_b.jpg)

图1 CPU指令的执行阶段

## 1．取指令阶段

取指令（Instruction Fetch，IF）阶段是将一条指令从主存中取到指令寄存器的过程。 程序计数器 PC 中的数值，用来指示当前指令在主存中的位置。当一条指令被取出后，PC 中的数值将根据指令字长度而自动递增：若为单字长指令，则(PC)+1->PC；若为双字长指令，则(PC)+2->PC，依此类推。

## 2．指令译码阶段

取出指令后，计算机立即进入指令译码（Instruction Decode，ID）阶段。 在指令译码阶段，指令译码器按照预定的指令格式，对取回的指令进行拆分和解释，识别区分出不同的指令类别以及各种获取操作数的方法。在组合逻辑控制的计算机中，指令译码器对不同的指令操作码产生不同的控制电位，以形成不同的微操作序列；在微程序控制的计算机中，指令译码器用指令操作码来找到执行该指令的微程序的入口，并从此入口开始执行。 在传统的设计里，CPU中负责指令译码的部分是无法改变的。不过，在众多运用微程序控制技术的新型 CPU 中，微程序有时是可重写的。

## 3．执行指令阶段

在取指令和指令译码阶段之后，接着进入执行指令（Execute，EX）阶段。 此阶段的任务是完成指令所规定的各种操作，具体实现指令的功能。为此，CPU 的不同部分被连接起来，以执行所需的操作。 例如，如果要求完成一个加法运算，算术逻辑单元 ALU 将被连接到一组输入和一组输出，输入端提供需要相加的数值，输出端将含有最后的运算结果。

## 4．访存取数阶段

根据指令需要，有可能要访问主存，读取操作数，这样就进入了访存取数（Memory，MEM）阶段。 此阶段的任务是：根据指令地址码，得到操作数在主存中的地址，并从主存中读取该操作数用于运算。

## 5．结果写回阶段

作为最后一个阶段，结果写回（Writeback，WB）阶段把执行指令阶段的**运行结果**数据“写回”到**某种存储形式**：结果数据经常被写到 CPU 的**内部寄存器**中，以便**被后续的指令快速地存取**；在有些情况下， 结果数据**也可被写入相对较慢、但较廉价且容量较大的主存**。许多指令还会改变程序**状态字寄存器中标志位** 的状态，这些标志位标识着不同的操作结果，可被用来影响程序的动作。

在指令执行完毕、结果数据写回之后，若无意外事件（如结果溢出等）发生，计算机就接着从程序计数器 PC 中取得下一条指令地址，开始新一轮的循环，下一个指令周期将顺序取出下一条指令。

许多新型 CPU 可以同时取出、译码和执行多条指令，体现并行处理的特性。

【**文章福利**】小编推荐自己的Linux内核技术交流群:**【[1095678385](https://link.zhihu.com/?target=https%3A//jq.qq.com/%3F_wv%3D1027%26k%3DD4ntfALI)】**整理了一些个人觉得比较好的学习书籍、视频资料共享在群文件里面，有需要的可以自行添加哦！  

![](https://pic4.zhimg.com/v2-9205dc75054bebbd15868d25396e382b_b.jpg)

## 二、CPU指令流水线

在任一条指令的执行过程中，各个功能部件都会随着指令执行的进程而呈现出时忙时闲的现象。要加快计算机的工作速度，就应使各个功能部件并行工作，即以各自可能的高速度同时、不停地工作，使得各部件的操作在时间上重叠进行，实现流水式作业。 从原理上说，计算机的流水线（Pipeline）工作方式就是将**一个计算任务细分成若干个子任务**，**每个子任务都由专门的功能部件进行处理**，一个计算任务的各个子任务由流水线上各个功能部件轮流进行处理 （即各子任务在流水线的各个功能阶段并发执行），最终完成工作。这样，**不必等到上一个计算任务完成， 就可以开始下一个计算任务的执行**。 流水线的硬件基本结构如图2所示。流水线由一系列串联的功能部件（Si）组成，各个功能部件之间设有高速缓冲寄存器（L），以暂时保存上一功能部件对子任务处理的结果，同时又能够接受新的处理任务。在一个统一的时钟（C）控制下，计算任务从功能部件的一个功能段流向下一个功能段。在流水线中， 所有功能段同时对不同的数据进行不同的处理，各个处理步骤并行地操作。

![](https://pic1.zhimg.com/v2-45247a0eefeaa1badc5da6a21c422914_b.jpg)

图2 流水线的硬件基本结构

当任务连续不断地输入流水线时，在流水线的输出端便连续不断地输出执行结果，流水线达到不间断流水的稳定状态，从而实现了**子任务级的并行**。

当指令流不能顺序执行时，流水过程会中断（即断流）。为了保证流水过程的工作效率，流水过程不应经常断流。在一个流水过程中，实现各个**子过程的各个功能段所需要的时间应该尽可能保持相等**，以避免产生瓶颈，导致流水线断流。

流水线技术本质上是将一个**重复的时序过程分解成若干个子过程**，而每一个**子过程都可有效地在其专用功能段上与其他子过程同时执行**。采用流水线技术通过硬件实现并行操作后，就某一条指令而言，其执行速度并没有加快，但就程序执行过程的整体而言，程序执行速度大大加快。

流水线技术适合于大量的重复性的处理。

前面我提到过CPU 中一个指令周期的任务分解。假设指令周期包含取指令（IF）、指令译码（ID）、 指令执行（EX）、访存取数（MEM）、结果写回（WB）5 个子过程（过程段），流水线由这 5个串联的过程段 组成，各个过程段之间设有高速缓冲寄存器，以暂时保存上一过程段子任务处理的结果，在统一的时钟信号控制下，数据从一个过程段流向相邻的过程段。

非流水计算机的时空图如下:

![](https://pic4.zhimg.com/v2-96e4c94e437b7b08875cf6b428f4c1a3_b.jpg)

图3 非流水计算机时空图

对于非流水计算机而言，上一条指令的 5 个子过程全部执行完毕后才能开始下一条指令，每隔 **5 个时 钟周期才有一个输出结果**。因此，图3中用了 15 个时钟周期才完成 3 条指令，每条指令平均用时 5 个时钟周期。 非流水线工作方式的控制比较简单，但部件的利用率较低，系统工作速度较慢。

-   标量流水计算机工作方式

标量（Scalar）流水计算机是**只有一条指令流水线**的计算机。图 4表示标量流水计算机的时空图。

![](https://pic3.zhimg.com/v2-b4915c2ac0206b74e7c6817540e726f6_b.jpg)

图4 标量流水计算机时空图

对标量流水计算机而言，上一条指令与下一条指令的 5 个子过程在时间上可以重叠执行，**当流水线满 载时，每一个时钟周期就可以输出一个结果**。因此，图4中仅用了 9 个时钟周期就完成了 5 条指令，**每条指令平均用时 1.8 个时钟周期**。

采用标量流水线工作方式，**虽然每条指令的执行时间并未缩短，但 CPU 运行指令的总体速度却能成倍 提高**。当然，作为速度提高的代价，需要增加部分硬件才能实现标量流水。

-   超标量流水计算机工作方式

一般的流水计算机因**只有一条指令流水线，所以称为标量流水计算机**。所谓**超标量**（Superscalar）流 水计算机，是指它**具有两条以上的指令流水线**。图 5表示超标量流水计算机的时空图。

![](https://pic2.zhimg.com/v2-5cd99d9fb40f8fb8725c34cbbe4d5d75_b.jpg)

图5 超标量流水计算机时空图

当流水线满载时，**每一个时钟周期可以执行 2 条以上的指令**。因此，图5中仅用了 9 个时钟周期就完成了 10 条指令，**每条指令平均用时 0.9 个时钟周期**。 超标量流水计算机是时间并行技术和空间并行技术的综合应用。

## 三、指令的相关性

指令流水线的一个特点是流水线中的各条指令之间存在一些相关性，使得指令的执行受到影响。要使流水线发挥高效率，就要使流水线连续不断地流动，尽量不出现断流情况。然而，由于**流水过程中存在的相关性冲突，断流现象是不可避免的**。

## 1．数据相关

在流水计算机中，指令的处理是重叠进行的，前一条指令还没有结束，第二、三条指令就陆续开始工 作。由于多条指令的重叠处理，当**后继指令所需的操作数刚好是前一指令的运算结果时，便发生数据相关冲突**。由于这两条指令的执行顺序直接影响到操作数读取的内容，必须等前一条指令执行完毕后才能执行后一条指令。在这种情况下，这两条指令就是数据相关的。因此，**数据相关是由于指令之间存在数据依赖性而引起的**。根据指令间对同一寄存器读和写操作的先后次序关系，可将数据相关性分为**写后读（Read-AfterWrite，RAW）相关、读后写（Write-After-Read，WAR）相关、写后写（Write-After-Write，WAW）相关**三种类型。

解决数据相关冲突的办法如下：

**⑴采用编译的方法** 编译程序通过在两条相关指令之间插入其他不相关的指令（或空操作指令）而推迟指令的执行，使数据相关消失，从而产生没有相关性的程序代码。这种方式简单，但降低了运行效率。

**⑵由硬件监测相关性的存在**，采用数据旁路技术设法解决数据相关 当前一条指令要写入寄存器而下一条指令要读取同一个寄存器时，在前一条指令执行完毕、结果数据还未写入寄存器前，由内部数据通路把该结果数据直接传递给下一条指令，也就是说，下一条指令所需的 操作数不再通过读取寄存器获得，而是直接获取。这种方式效率较高，但控制较为复杂。

## 2．资源相关

所谓资源相关，是指**多条指令进入流水线后在同一机器周期内争用同一个功能部件**所发生的冲突。 例如，在图 4所示的标量流水计算机中，**在第 4 个时钟周期时，第 1 条指令处于访存取数（MEM） 阶段，而第 4 条指令处于取指令（IF）阶段**。如果**数据和指令存放在同一存储器中**，**且存储器只有一个端口**，这样便会发生这两条指令争用存储器的资源相关冲突。 因为每一条指令都可能需要 2 次访问存储器（读指令和读写数据），在指令流水过程中，可能会有 2 条指令同时需要访问存储器，导致资源相关冲突解决资源相关冲突的一般办法是增加资源，例如增设一个存储器，将指令和数据分别放在两个存储器中。

## 3．控制相关

控制相关冲突是由**转移指令**引起的。当执行转移指令时，依据转移条件的产生结果，可能顺序取下一 条指令，也可能转移到新的目标地址取指令。**若转移到新的目标地址取指令，则指令流水线将被排空**，并等待转移指令形成下一条指令的地址，以便读取新的指令，这就使得流水线发生断流。 为了减小转移指令对流水线性能的影响，通常采用以下两种转移处理技术：

**⑴延迟转移法** 由**编译程序重排指令序列来实现**。其基本思想是“**先执行再转移**”，即发生转移时并不排空指令流水线，而是**继续完成下几条指令**。如果这些后继指令是与该转移指令结果无关的有用指令，那么延迟损失时间片正好得到了有效的利用。

**⑵转移预测法** 用硬件方法来实现。**依据指令过去的行为来预测将来的行为**，即**选择出现概率较高的分支进行预取**。通过使用转移取和顺序取两路指令预取队列以及目标指令 Cache，可将转移预测提前到取指令阶段进行，以获得良好的效果。

## 四、指令的动态执行技术

## 1．指令调度

为了减少指令相关性对执行速度的影响，可以在保证程序正确性的前提下，调整指令的顺序，即进行指令调度。 指令调度**可以由编译程序进行**，**也可以由硬件在执行的时候进行**，分别称为**静态指令调度**和**动态指令调度**。**静态指令调度是指编译程序通过调整指令的顺序来减少流水线的停顿**，提高程序的执行速度；**动态 指令调度用硬件方法调度指令的执行以减少流水线停顿**。

流水线中一直采用的**有序（in-order）指令启动是限制流水线性能的主要因素之一**。如果有**一条指令在流水线中停顿了，则其后的指令就都不能向前流动了**，这样，如果相邻的两条指令存在相关性，流水线就将发生停顿，如果有多个功能部件，这些部件就有可能被闲置。消除这种限制流水线性能的因素从而提高指令执行速度，其**基本思想就是允许指令的执行是无序的（out-of-order，也称乱序）**，也就是说，**在保持指令间、数据间的依赖关系的前提下，允许不相关的指令的执行顺序与程序的原有顺序有所不同**，这一思想是实行动态指令调度的前提。

## 2．乱序执行技术

乱序执行（Out-of-order Execution）是以乱序方式执行指令，即 CPU 允许将多条指令不按程序规定的顺序而分开发送给各相应电路单元进行处理。这样，根据各个电路单元的状态和各指令能否提前执行的具体情况分析，将能够提前执行的指令立即发送给相应电路单元予以执行，在这期间不按规定顺序执行指令；然后由**重新排列单元将各执行单元结果按指令顺序重新排列**。乱序执行的目的，就是为了**使 CPU 内部电路满负荷运转，并相应提高 CPU 运行程序的速度**。

实现乱序执行的关键在于**取消传统的“取指”和“执行”两个阶段之间指令需要线性排列的限制**，而使用一个**指令缓冲池**来开辟一个较长的指令窗口，**允许执行单元在一个较大的范围内调遣和执行已译码的程序指令流**。

## 3．分支预测

分支预测（Branch Prediction）是对程序的流程进行预测，然后读取其中一个分支的指令。采用分支预测的主要目的是为了提高 CPU的运算速度。 **分支预测的方法有静态预测和动态预测两类**：静态预测方法比较简单，如预测永远不转移、预测永远转移、预测后向转移等等，它并不根据执行时的条件和历史信息来进行预测，因此预测的准确性不可能很高；**动态预测**方法则**根据同一条转移指令过去的转移情况来预测未来的转移情况**。 由于程序中的条件分支是根据程序指令在流水线处理后的结果来执行的，所以当 CPU 等待指令结果时， 流水线的前级电路也处于等待分支指令的空闲状态，这样必然出现时钟周期的浪费。如果 CPU 能在前条指令结果出来之前就预测到分支是否转移，那么就可以提前执行相应的指令，这样就避免了流水线的空闲等待，也就相应提高了 CPU 的运算速度。但另一方面，**一旦前条指令结果出来后证明分支预测是错误的，那么就必须将已经装入流水线执行的指令和结果全部清除，然后再装入正确的指令重新处理，这样就比不进行分支预测而是等待结果再执行新指令还要慢了**。

因此，**分支预测的错误并不会导致结果的错误，而只是导致流水线的停顿**，如果能够保持较高的预测 准确率，分支预测就能提高流水线的性能。

## 五、实例分析

前面的知识只是一个理论基础铺垫，下面我们就结合一款真实的CPU架构进行对应分析，图6和图7分别是x86和ARM体系结构的内核架构图（都是具有OoOE特性的CPU架构），可以看到他们基本的组成都是一样的（虽然x86是CISC而ARM是RISC，但是现代x86内部也是先把CISC翻译成RISC的），因此我在这里就只分析x86结构。

![](https://pic2.zhimg.com/v2-056ae8955fe0730e920756c92a06c0ed_b.jpg)

图6 intel Nehalem内核架构图

![](https://pic2.zhimg.com/v2-c43013dfc378c06fd6db572da1ffeda5_b.jpg)

图7 ARM Cortex-A57内核架构图

## 1．取指令阶段（IF）

处理器在执行指令之前，必须先装载指令。指令会先保存在 L1 缓存的 I-cache (Instruction-cache)指令缓存当中，Nehalem 的指令拾取单元使用 128bit 带宽的通道从 I-cache 中读取指令。这个 I-cache 的大小为 32KB，采用了 [4 路组相连](https://link.zhihu.com/?target=https%3A//my.oschina.net/fileoptions/blog/1630855)，在后面的存取单元介绍中我们可以得知这种比 Core 更少的集合关联数量是为了降低延迟。

为了适应超线程技术，RIP(Relative Instruction Point，相对指令指针)的数量也从一个增加到了两个，每个线程单独使用一个。

![](https://pic1.zhimg.com/v2-ad981f8afc1116e019a644fe9c9df96c_b.jpg)

指令拾取单元**包含了分支预测器**(Branch Predictor)，**分支预测是在 Pentium Pro 处理器开始加入的功能，预测如 if then 这样的语句的将来走向**，**提前读取相关的指令并执行的技术**，可以明显地提升性能。指令拾取单元**也包含了 Hardware Prefetcher**，**根据历史操作预先加载以后会用到的指令来提高性能**，这会在后面得到详细的介绍。

当分支预测器决定了走向一个分支之后，**它使用 BTB(Branch Target Buffer，分支目标缓冲区)来保存预测指令的地址**。Nehalem 从以前的一级 BTB 升级到了两个级别，这是为了适应很大体积的程序(数据库以及 ERP 等应用，跳转分支将会跨过很大的区域并具有很多的分支)。Intel 并没有提及 BTB 详细的结构。与BTB 相对的 RSB(Return Stack Buffer，返回堆栈缓冲区)也得到了提升，RSB 用来保存一个函数或功能调用结束之后的返回地址，通过**重命名**的 RSB 来避免多次推测路径导致的入口/出口破坏。**RSB 每个线程都有一个，一个核心就拥有两个，以适应超线程技术的存在**。

指令拾取单元**使用预测指令的地址来拾取指令**，它通过访问 L1 ITLB 里的索引来继续访问 L1 ICache，128 条目的小页面 L1 ITLB 按照两个线程静态分区，每个线程可以获得 64 个条目，这个数目比 Core 2 的少。当关闭超线程时，单独的线程将可以获得全部的 TLB 资 源。除了小页面 TLB 之外，Nehalem 还每个线程拥有 7 个条目的全关联(Full Associativity) 大页面 ITLB，这些 TLB 用于访问 2M/4M 的大容量页面，每个线程独立，因此关闭超线程不会让你得到 14 个大页面 ITLB 条目。

指令拾取单元通过 128bit 的总线将指令从 L1 ICache 拾取到一个 16Bytes(刚好就是 128bit)的预解码拾取缓冲区。128 位的带宽让人有些迷惑不解，Opteron 一早就已经使用 了 256bit 的指令拾取带宽。最重要的是，L1D 和 L1I 都是通过 256bit 的带宽连接到 L2 Cache 的。

由于一般的CISC x86指令都小于4Bytes（32位x86指令;x86指令的特点就是不等长)， 因此一次可以拾取 4 条以上的指令，而预解码拾取缓冲区的输出带宽是 6 指令每时钟周期， 因此可以看出指令拾取带宽确实有些不协调，特别是考虑到 64 位应用下指令会长一些的情 况下(解码器的输入输出能力是 4 指令每时钟周期，因此 32 位下问题不大)。

**指令拾取结束后会送到 18 个条目的指令队列**，在 Core 架构，送到的是 LSD 循环流缓冲区，在后面可以看到，Nehalem 通过将 LSD 移动后更靠后的位置来提高性能。

## 2．指令译码阶段（ID）

在将指令充填到可容纳 18 条目的指令队列之后，就可以进行解码工作了。解码是类 RISC (精简指令集或简单指令集)处理器导致的一项设计，从 Pentium Pro 开始在 IA 架构出现。 处理器接受的是 x86 指令(CISC 指令，复杂指令集)，而在执行引擎内部执行的却不是x86 指令，而是一条一条的类 RISC 指令，Intel 称之为 Micro Operation——micro-op，或者写 为 μ-op，一般用比较方便的写法来替代掉希腊字母:u-op 或者 uop。相对地，一条一条的 x86 指令就称之为 Macro Operation或 macro-op。

**RISC 架构的特点就是指令长度相等，执行时间恒定(通常为一个时钟周期)**，因此处理器设计起来就很简单，可以通过深长的流水线达到很高的频率，IBM 的 Power6 就可以轻松地达到 4.7GHz 的起步频率。和 RISC 相反，**CISC 指令的长度不固定，执行时间也不固定**，因此 Intel 的 RISC/CISC 混合处理器架构就要通过解码器 将 x86 指令翻译为 uop，从而**获得 RISC 架构的长处，提升内部执行效率**。

和 Core 一样，Nehalem 的解码器也是 4 个（3 个简单解码器加 1 个复杂解码器）。简单解码器可以将一条 x86 指令(包括大部分 SSE 指令在内)翻译为一条 uop，而复杂解码器则将一些特别的(单条)x86 指令翻译为 1~4 条 uops——在极少数的情况下，某些指令需要通过 额外的可编程 microcode 解码器解码为更多的 uops(有些时候甚至可以达到几百个，因为 一些 IA 指令很复杂，并且可以带有很多的前缀/修改量，当然这种情况很少见)，下图 Complex Decoder 左方的 ucode 方块就是这个解码器，这个解码器可以通过一些途径进行升级或者扩展，实际上就是通过主板 Firmware 里面的 Microcode ROM 部分。

之所以具有两种解码器，是因为仍然是关于 RISC/CISC 的一个事实: 大部分情况下(90%) 的时间内处理器都在运行少数的指令，其余的时间则运行各式各样的复杂指令(不幸的是， 复杂就意味着较长的运行时间)，RISC 就是将这些复杂的指令剔除掉，只留下最经常运行的指令(所谓的精简指令集)，然而被剔除掉的那些指令虽然实现起来比较麻烦，却在某些领域确实有其价值，RISC 的做法就是将这些麻烦都交给软件，CISC 的做法则是像现在这样: 由硬件设计完成。因此 RISC 指令集对编译器要求很高，而 CISC 则很简单。对编程人员的要求也类似。

![](https://pic3.zhimg.com/v2-7ed99236c45213420771e6cd6d5579be_b.jpg)

## 3、循环流检测

在解码为 uop 之后 Nehalem 会将它们都存放在一个叫做 uop LSD Buffer 的缓存区。在Core 2 上，这个 LSD Buffer 是出现在解码器前方的，Nehalem 将其移动到解码器后方，并相对加大了缓冲区的条目。Core 2 的 LSD 缓存区可以保存 18 个 x86 指令而 Nehalem 可以保 存 28 个 uop，从前文可以知道，**大部分 x86 指令都可以解码为一个 uop，少部分可以解码 为 1~4 个 uop**，因此 Nehalem 的 LSD 缓冲区基本上可以相当于保存 21~23 条x86 指令，比 Core 2 要大上一些。

![](https://pic2.zhimg.com/v2-55e9450cd352a7b431b6fd192eca0161_b.jpg)

LSD 循环流监测器也算包含在解码部分，它的作用是: **假如程序使用的循环段(如 for..do/do..while 等)少于 28 个 uops，那么 Nehalem 就可以将这个循环保存起来，不再需要重新通过取指单元、分支预测操作，以及解码器**，Core 2 的 LSD 放在解码器前方，因此无法省下解码的工作。

Nehalem LSD 的工作比较像 NetBurst 架构的 Trace Cache，其也是保存 uops，作用也是部分地去掉一些严重的循环，不过由于 Trace Cache 还同时担当着类似于 Core/Nehalem 架构的 Reorder Buffer 乱序缓冲区的作用，容量比较大(可以保存 12k uops，准确的大小 是 20KB)，因此在 cache miss 的时候后果严重(特别是在 SMT 同步多线程之后，miss 率加 倍的情况下)，LSD 的小数目设计显然会好得多。不过笔者认为 28 个 uop 条目有些少，特 别是考虑到 SMT 技术带来的两条线程都同时使用这个 LSD 的时候。

在 LSD 之后，Nehalem 将会进行 Micro-ops Fusion，这也是前端(The Front-End)的最后一个功能，在这些工作都做完之后，uops 就可以准备进入执行引擎了。

## 4．乱序执行指令阶段（OoOE）

**OoOE— Out-of-Order Execution 乱序执行也是在 Pentium Pro 开始引入的**，它有些类似于多线程的概念。**乱序执行是为了直接提升 ILP(Instruction Level Parallelism)指令级并行化的设计**，在多个执行单元的超标量设计当中，一系列的执行单元可以**同时运行**一些**没有数据关联性的若干指令**，**只有需要等待其他指令运算结果的数据会按照顺序执行**，从而总体提升了运行效率。乱序执行引擎是一个很重要的部分，需要进行复杂的调度管理。

首先，在乱序执行架构中，**不同的指令可能都会需要用到相同的通用寄存器(GPR，General Purpose Registers)**，特别是在指令需要改写该通用寄存器的情况下——为了让这些指令们能并行工作，处理器需要准备解决方法。一般的 **RISC 架构准备了大量的GPR**， 而**x86 架构天生就缺乏 GPR**(x86具有8个GPR，x86-64 具有 16 个，一般 RISC 具有 32 个，IA64 则具有 128 个)，为此 Intel 开始引入**重命名寄存器(Rename Register)**，不同的指令可以通过具有名字相同但实际不同的寄存器来解决。

此外，为了 SMT 同步多线程，这些寄存器还要准备双份，每个线程具有独立的一份。

![](https://pic1.zhimg.com/v2-9ce3e60df5f59d43f4320677df1b699c_b.jpg)

乱序执行从Allocator定位器开始，**Allocator 管理着RAT(Register Alias Table，寄存器别名表)**、**ROB(Re-Order Buffer，重排序缓冲区)**和 **RRF(Retirement Register File，退回寄存器文件)**。在 **Allocator 之前，流水线都是顺序执行的**，在 **Allocator 之后，就可以进入乱序执行阶段了**。在每一个线程方面，Nehalem 和 Core 2 架构相似，**RAT 将重命名的、虚拟的寄存器(称为 Architectural Register 或 Logical Register)指向ROB 或者RRF**。**RAT 是一式两份,每个线程独立**，**每个 RAT 包含了 128 个重命名寄存器**。**RAT 指向在 ROB 里面的最近的执行寄存器状态，或者指向RRF保存的最终的提交状态。**

**ROB(Re-Order Buffer，重排序缓冲区)**是一个非常重要的部件，**它是将乱序执行完毕的指令们按照程序编程的原始顺序重新排序的一个队列**，以**保证所有的指令都能够逻辑上实现正确的因果关系**。**打乱了次序的指令们(分支预测、硬件预取)依次插入这个队列**，当一条指令通过 RAT 发往下一个阶段确实执行的时候这条指令(包括寄存器状态在内)将被加入 ROB 队列的一端，**执行完毕的指令(包括寄存器状态)将从 ROB 队列的另一端移除(期间这些指令的数据可以被一些中间计算结果刷新)**，因为**调度器是 In-Order 顺序的，这个队列（ROB）也就是顺序的**。**从 ROB 中移出一条指令就意味着指令执行完毕了，这个阶段叫做 Retire 回退**，相应地 ROB 往往也叫做 Retirement Unit(回退单元)，并将其画为**流水线的最后一部分**。

在一些超标量设计中，**Retire 阶段会将 ROB 的数据写入 L1D 缓存（这是将MOB集成到ROB的情况）**，而在另一些设计里， **写入 L1D 缓存由另外的队列完成**。例如，Core/Nehalem 的这个操作就由 **MOB(Memory Order Buffer，内存重排序缓冲区)**来完成。

ROB 是乱序执行引擎架构中都存在的一个缓冲区，**重新排序指令的目的是将指令们的寄存器状态依次提交到RRF退回寄存器文件当中**，**以确保具有因果关系的指令们在乱序执行中可以得到正确的数据**。**从执行单元返回的数据会将先前由调度器加入ROB 的指令刷新数据部分**并**标志为结束(Finished)**，再经过其他检查通过后才能**标志为完毕(Complete)**，**一旦标志为完毕，它就可以提交数据并删除重命名项目并退出ROB** **了**。提交状态的工作由 Retirement Unit(回退单元)完成，**它将确实完毕的指令包含的数据写入RRF(“确实” 的意思是，非猜测执性、具备正确因果关系，程序可以见到的最终的寄存器状态)**。和 RAT 一样，**RRF 也同时具有两个，每个线程独立。**Core/Nehalem 的 Retirement Unit 回退单元每时钟周期可以执行 4 个 uops 的寄存器文件写入，和 RAT 每时钟 4 个 uops 的重命名一致。

由于 ROB 里面保存的指令数目是如此之大(128 条目)，因此一些人认为它的作用是用来从中挑选出不相关的指令来进入执行单元，这多少是受到一些文档中的 Out-of-Order Window 乱序窗口这个词的影响(后面会看到ROB 会和 MOB 一起被计入乱序窗口资源中)。

ROB 确实具有 RS 的一部分相似的作用，不过，**ROB 里面的指令是调度器（dispacher）通过 RAT发往 RS 的同时发往ROB的（里面包含着正常顺序的指令和猜测执行的指令，但是乱序执行并不是从ROB中乱序挑选的），也就是说，在“乱序”之前，ROB 的指令就已经确定了**。**指令并不是在 ROB 当中乱序挑选的(这是在RS当中进行)，ROB 担当的是流水线的最终阶段: 一个指令的 Retire回退单元;以及担当中间计算结果的缓冲区。**  
**RS(Reservation Station，中继站):** **等待源数据**到来以进行OoOE乱序执行(**没有数据的指令将在 RS 等待**)， **ROB(ReOrder Buffer，重排序缓冲区):** **等待结果到达**以进行 Retire 指令回退 (**没有结果的指令将在 ROB等待**)。

Nehalem 的 128 条目的 **ROB 担当中间计算结果的缓冲区**，**它保存着猜测执行的指令及其数据**，**猜测执行允许预先执行方向未定的分支指令**。在大部分情况下，猜测执行工作良好——**分支猜对了，因此其在 ROB 里产生的结果被标志为已结束，可以立即地被后继指令使用而不需要进行 L1 Data Cache 的 Load 操作**(这也是 ROB 的另一个重要用处，典型的 x86 应用中 **Load 操作是如此频繁**，达到了几乎占 1/3 的地步，**因此 ROB 可以避免大量的Cache Load 操作，作用巨大**)。在剩下的不幸的情况下，分支**未能按照如期的情况进行**，这时猜测的**分支指令段将被清除**，相应指令们的**流水线阶段清空**，对应的**寄存器状态也就全都无效**了，这种**无效的寄存器状态不会也不能出现在 RRF 里面。**

重命名技术并不是没有代价的，在获得前面所说的众多的优点之后，它令指令在发射的时候需要扫描额外的地方来寻找到正确的寄存器状态，不过总体来说这种代价是非常值得的。RAT可以在每一个时钟周期重命名 4 个 uops 的寄存器，**经过重命名的指令在读取到正确的操作数并发射到统一的RS(Reservation Station，中继站，Intel 文档翻译为保留站点) 上。RS 中继站保存了所有等待执行的指令。**

和 Core 2 相比，Nehalem 的 ROB 大小和 RS 大小都得到了提升，ROB 重排序缓冲区从 96 条目提升到 128 条目(鼻祖 Pentium Pro 具有 40 条)，RS 中继站从 32 提升到 36(Pentium Pro 为 20)，它们都在两个线程(超线程中的线程)内共享，不过采用了不同的策略:ROB 是采用了静态的分区方法，而 RS 则采用了动态共享，因为有时候会有一条线程内的指令因 等待数据而停滞，这时另一个线程就可以获得更多的 RS 资源。停滞的指令不会发往 RS，但是仍然会占用 ROB 条目。由于 ROB 是静态分区，因此在开启 HTT 的情况下，每一个线程只能 分到 64 条，不算多，在一些极少数的应用上，我们应该可以观察到一些应用开启 HTT 后会 速度降低，尽管可能非常微小。

## 5、执行单元

在为 SMT 做好准备工作并打乱指令的执行顺序之后（指的是分支预测、硬件预取），uops 通过每时钟周期 4 条的速度进入 Reservation Station 中继站(保留站)，总共 36 条目的中继站 uops 就开始等待超标量(Superscaler)执行引擎乱序执行了。自从 Pentium 开始，Intel 就开始在处理器里面采用了超标量设计(Pentium 是两路超标量处理器)，**超标量的意思就是多个执行单元**，它可以**同时执行多条没有相互依赖性的指令**，从而达到提升 **ILP 指令级并行化**的目的。Nehalem 具备 6 个执行端口，每个执行端口具有多个不同的单元以执行不同的任务，然而同一时间只能有一条指令(uop)进入执行端口，因此也可以认为 Nehalem 有 6 个“执行单元”，在每个时钟周期内可以执行最多 6 个操作(或者说，6 条指令)，和 Core 一样。

![](https://pic4.zhimg.com/v2-8cab5ec920c446fcf166229da9df1ce7_b.jpg)

36 条目的中继站指令在分发器的管理下，**挑选出尽量多的可以同时执行的指令(也就是乱序执行的意思)**——最多 6 条——发送到执行端口。 这些执行端口并**不都是用于计算**，实际上，**有三个执行端口是专门用来执行内存相关的操作的，只有剩下的三个是计算端口**，因此，在这一点上 Nehalem 实际上是跟 Core 架构一 样的，这也可以解释为什么有些情况下，Nehalem 和 Core 相比没有什么性能提升。

**计算操作分为两种**: 使用 ALU(Arithmetic Logic Unit，算术逻辑单元)的**整数(Integer) 运算**和使用 FPU(Floating Point Unit，浮点运算单元)的**浮点(Floating Point)运算**。SSE 指令(包括 SSE1 到 SSE4)是一种特例，它虽然有整数也有浮点，然而它们使用的都是 128bit 浮点寄存器，使用的也大部分是 FPU 电路。在 Nehalem 中，三个计算端口都可以做整数运算(包括 MMX)或者SSE 运算(浮点运算不太一样，只有两个端口可以进行浮点 ADD 和 MUL/DIV 运算，因此每时钟周期最多进行 2 个浮点计算，这也是目前 Intel 处理器浮点性能不如整数性能突出的原因)，不过每一个执行端口都不是完全一致:只有端口 0 有浮点乘和除功能，**只有端口 5 有分支能力(这个执行单元将会与分支预测单元连接)**，其他 FP/SSE 能力也不尽相同，这些不对称之处都由统一的分发器来理解，并进行指令的调度管理。**没有采用完全对称的设计可能是基于统计学上的考虑**。和 Core 一样，Nehalem 的也没有采用 Pentium 4 那样的 2 倍频的 ALU 设计(在 Pentium 4，ALU 的运算频率是 CPU 主频的两倍， 因此整数性能明显要比浮点性能突出)。

不幸的是，虽然可以同时执行的指令很多，然而在流水线架构当中运行速度并不是由最 “宽”的单元来决定的，而是由最“窄”的单元来决定的。这就是木桶原理，Opteron的解码器后端只能每时钟周期输出 3 条 uops，而 Nehalem/Core2 则能输出 4 条，因此它们的实际最大每时钟运行指令数是 3/4，而不是 6。同样地，多少路超标量在这些乱序架构处理器中也不再按照运算单元来划分，Core Duo 及之前(到 Pentium Pro 为止)均为三路超标量处理器，Core 2/Nehalem 则为四路超标量处理器。可见在微架构上，Nehalem/Core 显然是 要比其他处理器快一些。顺便说一下，这也是 Intel 在超线程示意图中，使用 4 个宽度的方 块来表示而不是 6 个方块的原因。

## 6、存取单元

运算需要用到数据，也会生成数据，**这些数据存取操作就是存取单元所做的事情**，实际 上，Nehalem 和 Core 的存取单元没什么变化，仍然是 3 个。

这**三个存取单元**中，**一个用于所有的 Load 操作(地址和数据)，一个用于 Store 地址，一个用于 Store 数据**，前两个数据相关的单元带有 AGU(Address Generation Unit，地址生成单元)功能(NetBurst架构使用快速 ALU 来进行地址生成)。

![](https://pic1.zhimg.com/v2-b923df77486af9be4727ce3c6e82f7a4_b.jpg)

在乱序架构中，**存取操作也可以打乱进行**。**类似于指令预取一样**，**Load/Store 操作也可以提前进行以降低延迟的影响，提高性能**。然而，**由于Store操作会修改数据影响后继的Load 操作，而指令却不会有这种问题(寄存器依赖性问题通过ROB解决)，因此数据的乱序操作更为复杂。**

![](https://pic2.zhimg.com/v2-b98fd8c565a3989b1bcb3c81374f6dc5_b.jpg)

如上图所示，第一条 ALU 指令的运算结果要 Store 在地址 Y(第二条指令)，而第九条 指令是从地址 Y Load 数据，显然在第二条指令执行完毕之前，无法移动第九条指令，否则将会产生错误的结果。同样，如果CPU也不知道第五条指令会使用什么地址，所以它也无法确定是否可以把第九条指令移动到第五条指令附近。

![](https://pic1.zhimg.com/v2-ef25d18987342336eef9d9a5db7b0e18_b.jpg)

**内存数据相依性预测功能(Memory Disambiguation)可以预测哪些指令是具有依赖性的或者使用相关的地址(地址混淆，Alias)**，**从而决定哪些 Load/Store 指令是可以提前的**， 哪些是不可以提前的。**可以提前的指令在其后继指令需要数据之前就开始执行、读取数据到ROB当中**，**这样后继指令就可以直接从中使用数据，从而避免访问了无法提前 Load/Store 时访问 L1 缓存带来的延迟(3~4 个时钟周期)。**

不过，为了**要判断一个 Load 指令所操作的地址没有问题，缓存系统需要检查处于 in-flight 状态(处理器流水线中所有未执行的指令)的 Store 操作**，这是一个颇耗费资源的过程。在 NetBurst 微架构中，通过**把一条 Store 指令分解为两个 uops——一个用于计算地址、一个用于真正的存储数据，这种方式可以提前预知 Store 指令所操作的地址**，初步的**解决了数据相依性问题**。在 NetBurst 微架构中，Load/Store 乱序操作的算法遵循以下几条 原则:

-   如果一个对于**未知地址**进行操作的 Store 指令处于 in-flight 状态，那么所有的 Load 指令都要被延迟
-   在操作**相同地址**的 Store 指令之前 Load 指令不能继续执行
-   一个 **Store 指令不能移动到另外一个 Store 指令之前（指的是在RS中不能先挑选执行后面的一条store指令，注意这只是说某一种架构不允许重排store，其实还是有很多架构如Alpha等是松散内存模型，允许不相关的store重排序的，这一块就牵扯到memory models相关知识了，建议参考[这里](https://link.zhihu.com/?target=https%3A//www.oschina.net/action/GoToLink%3Furl%3Dhttp%253A%252F%252Fwww.cl.cam.ac.uk%252F%257Epes20%252Fweakmemory%252F%2523x86)）**

这种原则下的问题也很明显，比如第一条原则会在一条处于等待状态的 Store 指令所操作的地址未确定之前，就延迟所有的 Load 操作，显然**过于保守了**。实际上，**地址冲突问题是极少发生的**。根据某些机构的研究，在一个Alpha EV6 处理器中最多可以允许 512 条指令处于 in-flight 状态，但是其中的 97%以上的 Load 和 Store 指令都不会存在地址冲突问题。  
基于这种理念，**Core 微架构采用了大胆的做法**，**它令 Load 指令总是提前进行**，除非新加入的动态混淆预测器(Dynamic Alias Predictor)预测到了该 Load 指令不能被移动到 Store 指令附近。这个预测是根据历史行为来进行的，据说准确率超过 90%。  
**在执行了预 Load 之后，一个冲突监测器会扫描 MOB 的 Store 队列**，检查该**是否有Store操作与该 Load 冲突**。在很不幸的情况下(1%~2%)，**发现了冲突，那么该 Load 操作作废**、 **流水线清除并重新进行 Load 操作**。这样大约会损失 20 个时钟周期的时间，然而从整体上看， Core 微架构的激进 Load/Store 乱序策略确实很有效地提升了性能，因为**Load 操作占据了通常程序的 1/3 左右，并且 Load 操作可能会导致巨大的延迟**(在命中的情况下，Core 的 L1D Cache 延迟为 3 个时钟周期，Nehalem 则为 4 个。L1 未命中时则会访问 L2 缓存，一般为 10~12 个时钟周期。访问 L3 通常需要 30~40 个时钟周期，访问主内存则可以达到最多约 100 个时钟周期)。**Store 操作并不重要，什么时候写入到 L1 乃至主内存并不会影响到执行性能。**

![](https://pic2.zhimg.com/v2-2396063e4d5d7a53a8ed963d857b1a71_b.jpg)

如上图所示，我们需要载入地址 X 的数据，加 1 之后保存结果;载入地址 Y 的数据，加1 之后保存结果;载入地址 Z 的数据，加 1 之后保存结果。如果根据 Netburst 的基本准则， 在第三条指令未决定要存储在什么地址之前，处理器是不能移动第四条指令和第七条指令的。实际上，它们之间并没有依赖性。因此，Core 微架构中则**“大胆”的将第四条指令和第七条指令分别移动到第二和第三指令的并行位置**，这种行为是**基于一定的猜测的基础上的“投机”行为，如果猜测的对的话(几率在 90%以上)，完成所有的运算只要5个周期，相比之前的9个周期几乎快了一倍。**

和为了顺序提交到寄存器而需要 ROB 重排序缓冲区的存在一样，在乱序架构中，多个**打乱了顺序的 Load 操作和Store操作也需要按顺序提交到内存**，**MOB(Memory Reorder Buffer， 内存重排序缓冲区)就是起到这样一个作用的重排序缓冲区**(介于 Load/Store 单元 与 L1D Cache 之间的部件，有时候也称之为LSQ)，MOB 通过一个 128bit 位宽的 Load 通道与一个 128bit 位宽的 Store 通道与双口 L1D Cache 通信。**和 ROB 一样，MOB的内容按照 Load/Store 指令实际的顺序加入队列的一端**，**按照提交到 L1 DCache 的顺序从队列的另一端移除**。ROB 和 MOB 一起实际上形成了一个分布式的 Order Buffer 结构，有些处理器上只存在 ROB，兼备了 MOB 的功能（把MOB看做ROB的一部分可能更好理解）。

和ROB 一样，**Load/Store 单元的乱序存取操作会在 MOB 中按照原始程序顺序排列**，以提供正确的数据，**内存数据依赖性检测功能也在里面实现(内存数据依赖性的检测比指令寄存器间的依赖性检测要复杂的多)**。**MOB 的 Load/Store 操作结果也会直接反映到 ROB当中（中间结果）**。

**MOB还附带了数据预取(Data Prefetch)功能**，它会**猜测未来指令会使用到的数据**，并**预先从L1D Cache 缓存 Load入MOB 中**(Data Prefetcher 也会对 L2 至系统内存的数据进行这样的操作)， **这样 MOB 当中的数据有些在 ROB 中是不存在的**(这有些像 ROB 当中的 Speculative Execution 猜测执行，MOB 当中也存在着“Speculative Load Execution 猜测载入”，**只不过失败的猜测执行会导致管线停顿，而失败的猜测载入仅仅会影响到性能，然而前端时间发生的Meltdown漏洞却造成了严重的安全问题**)。**MOB包括了Load Buffers和Store Buffers。**

乱序执行中我们可以看到很多缓冲区性质的东西: **RAT 寄存器别名表、ROB 重排序缓冲 区、RS 中继站、MOB 内存重排序缓冲区(包括 load buffer 载入缓冲和 store buffer 存储缓冲)**。在**超线程**的作 用下，**RAT是一式两份**，包含了 128 个重命名寄存器; 128 条目的 ROB、48 条目的 LB 和 32 条目的 SB 都 每个线程 64 个 ROB、24 个 LB 和 16 个 SB; **RS 则是在两个线程中动态共享**。可见，虽然整体数量增加了，然而就单个线程而言，获得的资源并没有 提升。这会影响到 HTT 下单线程下的性能。

## 六、缓存（cache）

通常缓存具有两种设计:非独占和独占，Nehalem 处理器的 L3 采用了非独占高速缓存 设计(或者说“包含式”，L3 包含了 L1/L2 的内容)，这种方式在 Cache Miss 的时候比独 占式具有更好的性能，而在缓存命中的时候需要检查不同的核心的缓存一致性。Nehalem 并 采用了“内核有效”数据位的额外设计，降低了这种检查带来的性能影响。随着核心数目的 逐渐增多(多线程的加入也会增加 Cache Miss 率)，对缓存的压力也会继续增大，因此这 种方式会比较符合未来的趋势。在后面可以看到，这种设计也是考虑到了多处理器协作的情况(此时 Miss 率会很容易地增加)。这可以看作是 Nehalem 与以往架构的基础不同:之前的架构都是来源于移动处理设计，而 Nehalem 则同时为企业、桌面和移动考虑而设计。

在 L3 缓存命中的时候(单处理器上是最通常的情况，多处理器下则不然)，处理器检查内核有效位看看是否其他内核也有请求的缓存页面内容，决定是否需要对内核进行侦听。

在NUMA架构中，多个处理器中的同一个缓存页面必定在其中一个处理器中属于 F 状态(可以修改的状态)，这个页面在这个处理器中没有理由不可以多核心共享(可以多核心共享就意味着这个能进入修改状态的页面的多个有效位被设置为一)。MESIF协议应该是工作在核心(L1+L2)层面而不是处理器(L3)层面，这样同一处理器里多个核心共享的页面，只有其中一个是出于 F 状态(可以修改的状态)。见后面对 NUMA 和 MESIF 的解析。(L1/L2/L3 的同步应该是不需要 MESIF 的同步机制)

在 L3 缓存未命中的时候(多处理器下会频繁发生)，处理器决定进行内存存取，按照 页面的物理位置，它分为近端内存存取(本地内存空间)和远端内存存取(地址在其他处理 器的内存的空间):

关于缓存[Cache架构原理](https://link.zhihu.com/?target=https%3A//my.oschina.net/fileoptions/blog/1630855)和[Cache一致性MESI](https://link.zhihu.com/?target=https%3A//my.oschina.net/fileoptions/blog/1593952)的原理不是本文的重点，此处不再赘述。

## 七、总结

之前一直存在疑惑是因为看了[Paul的论文](https://link.zhihu.com/?target=https%3A//my.oschina.net/fileoptions/blog/1593952)，有两点，一个是CPU在猜测执行时，如果猜测乱序执行的是一条store指令，并且store指令要操作的地址和之前正处于in-flight状态的store指令没有相关关系，那么意味着这条猜测执行的store会直接更新DCache，如果后期发现猜测执行错误，那么此时是没有办法回滚的（因为数据已经写入DCache），而对于猜测执行的load就不会造成这么严重的问题，顶多就是提前把主内存的内容读到DCache中（但是前段时间发生的**[Meltdown漏洞](https://link.zhihu.com/?target=https%3A//www.oschina.net/action/GoToLink%3Furl%3Dhttps%253A%252F%252Fmeltdownattack.com%252F)**就是利用了该特性）；第二个疑问就是，类似于x86手册中说了store指令（参见[memory model](https://link.zhihu.com/?target=https%3A//www.oschina.net/action/GoToLink%3Furl%3Dhttp%253A%252F%252Fpreshing.com%252F20120930%252Fweak-vs-strong-memory-models%252F)）是不允许重排执行的，那还会还会存在[Paul的论文](https://link.zhihu.com/?target=https%3A//www.oschina.net/action/GoToLink%3Furl%3Dhttp%253A%252F%252Fwww.rdrop.com%252Fusers%252Fpaulmck%252Fscalability%252Fpaper%252Fwhymb.2010.06.07c.pdf)所描述的问题现象（讲store store barrier那里）吗，理论上store指令都是顺序提交结果到内存的。

对于第一个疑惑，其答案就是，处于猜测执行阶段的store指令是**不允许提交**的（commit）,因为猜测执行之前的代码还没有提交（时刻记住 **乱序执行、按顺序提交** 贯穿全文），而一旦猜测执行之前的代码提交，也就可以验证猜测执行是否成功，此时如果猜测成功就执行commit，store数据到DCache（但是还是允许提前load数据到DCache），否则就直接丢弃猜测执行的结果（直接丢弃load buffer里面的数据）。也有些CPU架构中，对于store类型的指令是不允许猜测执行的，因此也不会有问题。

对于第二个疑惑，对于允许store重排的CPU架构来说，虽然内存读写指令在RS、MOB中都是按照编程顺序存放的，但是对于前后两条不相关的store指令而言，store指令就会乱序执行，但是虽然可以乱序执行（执行的结果反映在MOB中），但是依然必须**顺序提交MOB中的结果**。而对于x86这种不允许store重排的CPU而言，store只能按顺序执行，并且在MOB中被标记，并等待顺序提交。进入提交阶段后，MOB中store buffer中的store直连会按照编程顺序一条一条进行提交（即写数据到DCache），但是如果前一条store指令操作的数据不在本地cahce中，此时该store指令就无法被立即写入DCache，需要等待cache层MESI协议把数据同步过来，这是相当耗时的。而如果恰巧后一条store指令（前提是与前一条store不存在地址冲突）操作的数据就在本地cache中，此时如果允许后一条store指令先提交（这样可以大大的降低CPU的等待时间），则就会出现store乱序的问题（[Paul的论文](https://link.zhihu.com/?target=https%3A//www.oschina.net/action/GoToLink%3Furl%3Dhttp%253A%252F%252Fwww.rdrop.com%252Fusers%252Fpaulmck%252Fscalability%252Fpaper%252Fwhymb.2010.06.07c.pdf)描述的现象，注意，这并不是真正的store乱序，store结果依然还是按照编程顺序进行提交的）。因此，如果这个结果正好违背业务逻辑，解决方式就是显示的在两条store之间添加store store内存屏障，这样后一条store在作用到cache之前，会先等待store buffer被排空，这样就不会存在store“乱序”执行的现象，说白了，这个问题导致的原因，就是这个**store buffer在提交时是否是FIFO的（即队列必须严格的先进先出）**。如果store buffer是FIFO类型的（如x86 CPU），那么就不会存在该现象。x86体系结构中store指令的确是不会被乱序执行的，所以的确不会发生[Paul的论文](https://link.zhihu.com/?target=https%3A//my.oschina.net/fileoptions/blog/1593952)描述的问题。

由于load指令一般是允许乱序执行的，也就是load指令会在RS中被乱序dispatch到执行引擎（计算地址等），因此Memory Subsystem中的load buffer就用来暂存这些提前执行的load指令结果，如果乱序执行出现错误、或者分支预测错误，直接丢弃load buffer中的内容即可，但是load操作也是带有副作用的，就是它会导致数据被load到cache上，很容易被**[Meltdown漏洞](https://link.zhihu.com/?target=https%3A//www.oschina.net/action/GoToLink%3Furl%3Dhttps%253A%252F%252Fmeltdownattack.com%252F) 利用。**

最后概括下乱序执行的含义

-   现代处理器采用指令并行技术,在不存在数据依赖性的前提下,处理器可以改变语句对应的机器指令的执行顺序来提高处理器执行速度
-   现代处理器采用内部缓存技术,导致数据的变化不能及时反映在主存所带来的乱序.
-   现代编译器为优化而重新安排语句的执行顺序

整个乱序执行的过程为：经过取值、译码、寄存器重命名之后的指令，被**顺序**送到保留站（如果是一般的指令还会**顺序发到ROB**，即重排序缓冲区，如果是内存store/load指令，会按**顺序放入MOB**，即内存重排序缓冲区，有时候MOB就包含在ROB中），在保留站中的指令，一旦源操作数已经ready，并且当前有空闲的硬件单元，那么这个指令就可以执行，而**不需要等待前面的指令完成**，这就造成了乱序执行的效果。乱序执行中的指令，产生的寄存器修改不会修改最终的物理寄存器，而只会修改自己重命名的私有寄存器（中间检结果反应在ROB中）。同样，如果是内存存取操作（store/load）乱序执行，也只会把结果暂存到MOB中（MOB包含store buffer和load buffer）。 因此乱序执行的结果还是未生效的，乱序执行后，需要按照顺序提交ROB和MOB中的结果（其实就是对这些缓冲区中的entry做个commit的标记，表示这些entry可以被应用到寄存区或者被写入内存）。ROB和MOB除了可以暂存中间结果、保证按序提交，还会应用于指令间的依赖分析、分支预测、硬件预取等功能。

![](https://pic3.zhimg.com/v2-f0ed65f43e9a418db7d95bb9f0d6e536_b.jpg)

## 八、附录

## 常见CPU架构的内存模型

![](https://pic1.zhimg.com/v2-7deae530ee7dd04b3309bcf85216a4f4_b.jpg)

以 [Paul论文](https://link.zhihu.com/?target=https%3A//www.oschina.net/action/GoToLink%3Furl%3Dhttp%253A%252F%252Fwww.rdrop.com%252Fusers%252Fpaulmck%252Fscalability%252Fpaper%252Fwhymb.2010.06.07c.pdf)描述的图为例，该图可以作为CPU的一种通用架构模型。而所谓的内存模型（memory model），其实就是个CPU在实现上图架构时的一些差异，如CPU是否允许乱序执行执行指令、store buffer是否是FIFO的、是否存在分支预测、是否存在预读、是否存在invalidate queue组件等。

对于x86架构来说，**store buffer是FIFO**，因此不会存在store store乱序，写入顺序就是刷入cache的顺序。但是对于ARM/Power架构来说，store buffer并未保证FIFO，因此先写入store buffer的数据，是有可能比后写入store buffer的数据晚刷入cache的（前文已解释）。从这点上来说，store buffer的存在会让ARM/Power架构出现乱序的可能。store barrier存在的意义就是将store buffer中的数据，刷入cache。在某些cpu中，存在invalid queue。invalid queue用于缓存cache line的失效消息，也就是说，当cpu0写入W0(x, 1)，并从store buffer将修改刷入cache，此时cpu1读取R1(x, 0)仍是允许的。因为使cache line失效的消息被缓冲在了invalid queue中，还未被应用到cache line上。这也是一种会使得指令乱序的可能。load barrier存在的意义就是将invalid queue缓冲刷新。

对于x86架构的cpu来说，在单核上来看，其保证了Sequential consistency，因此对于开发者，我们可以完全不用担心单核上的乱序优化会给我们的程序带来正确性问题。在多核上来看，其保证了x86-tso模型，使用mfence就可以将store buffer中的数据，写入到cache中。而且，由于x86架构下，store buffer是FIFO的和不存在invalid queue，mfence能够保证多核间的数据可见性，以及顺序性。对于arm和power架构的cpu来说，编程就变得危险多了。除了存在数据依赖，控制依赖以及地址依赖等的前后指令不能被乱序之外，其余指令间都有可能存在乱序。而且，它们的store buffer并不是FIFO的，而且还可能存在invalid queue，这些也同样让并发编程变得困难重重。因此需要引入不同类型的barrier来完成不同的需求。

![](https://pic2.zhimg.com/v2-281942fd85e65cf31cd66bf06ed6bf41_b.jpg)

```
原创作者：黑客画家
原出处：开源博客
原创地址：https://my.oschina.net/fileoptions/blog/1633021
```
