---
source: https://zhuanlan.zhihu.com/p/184912992?utm_source=zhihu
---
memory barrier对于程序员来讲应该都比较熟悉，具体是什么不再赘述。这篇文章主要参考Paul的论文从硬件的层面来深入理解memory barrier的作用原理，涉及cpu cache一致性、store buffer、invalidate queue和一点内存模型的相关知识。

### 1. cache架构

现代cpu的速度远超内存的速度。为了应对这种超过两个数量级的速度悬殊，引入了多级cache。如图1所示，数据以"cache lines"的形式在cpus的cache和内存之间传输，cache line是定长的block，大小通常在16～256bytes。

![](https://pic1.zhimg.com/v2-c18fe4a57b4cdd324c2670b3aa3dafc8_b.jpg)

对于给定的cpu，第一次访问某个数据item时会经历一次cache miss，需要从内存中读取放入cpu的cache中，再次访问时将从cache中找到。当cpu cache满了之后，新的数据将会淘汰cache中的数据，然而大多数caches即使未满，也可能被强制淘汰旧的数据，原因在于cache的硬件实现，如图2所示的例子中，这个cache由定长hash桶(也叫"sets")组成的hashtable实现，十六个"sets"和两个"ways"共组成32个"lines"，每个包含一个256字节的"cache line"。

![](https://pic1.zhimg.com/v2-cc031866468f6935a72fc022d5c47e18_b.jpg)

因为cache是硬件实现，hash函数非常简单，只使用4位内存地址。如图2所示，每个单元是一个包含256字节的cache line，因为cache line是256字节对齐的，地址的低8位都是0，因此硬件的hash函数选择更高的4位来匹配左边的hash数值。

举例描述，假设程序访问图中的0x12345F00，hash的位置为0xF，两个way都是空的，相应的cache line可以直接放入。如果程序访问0x1233000，hash的位置为0x0，相应的cache line可以放入way 1中。然而如果程序访问0x1233E00，0xE的两个way都已被占据，其中的一个会被淘汰，下次访问它时将会有一次cache miss。

以上分析了cpu读数据的例子，对于写，需要保证所有cpu之间的一致性，在某个cpu写数据之前需要将其它cpu cache中的相应数据移除或者"invalidated"，这些操作完成后它才可以安全地修改这个数据。而其它cpu再次访问这个数据时将会经历cache miss。因此，为确保所有cpu能维护数据的一致性视图，需要非常细致的处理，fetching、invalidating、writing这些对数据的操作很容易造成数据丢失或者不同cpu的cache中数据不一致，下一节将介绍"高速缓存一致性协议"如何防止这些问题。

### 2. cache一致性协议

cache一致性协议通过管理cache-line的状态来防止不一致或者数据丢失。这些协议十分复杂，可以多达数十种状态，这里只关注四种状态的MESI缓存一致性协议。

### MESI状态

使用MESI协议，每个cache line上用2位来维护状态值"tag"，四种状态分别为：

-   **modified**  
    处于modified状态的cache line可以认为是被这个cpu所拥有，相应的内存确保不会出现在其它cpu的cache中。这个cache拥有这份数据的最新副本，因此也负责将它写回内存或者移交给其它cache。
-   **exclusive**  
    与modified相似，区别是这种状态下相应cpu还未修改cache line的这份数据，内存中的这份数据是最新的，在cache丢弃该数据前，缓存不用负责将数据写回或者移交给其它cache。
-   **shared**  
    处于shared状态的cache line可能被复制到至少一个其它的cpu cache中，因此在该cpu存储该line时，需要先询问其它cpu，与exclusive一样，相应的内存中的值是最新的，在cache丢弃该数据前，不用负责将数据写回或者移交给其它cache。
-   **invalid**  
    处于invalid状态的line是空的，没有数据，当新的数据进入cache时，将会优先被放入invalid状态的cache line中，替代其它状态的line可能引起将来的cache miss。

所有cpu必须维护cache中数据的一致性视图，因此cache一致性协议提供messages用于cpu间的通信以协调cache line的状态转换。

### MESI协议Messages

如果cpu在单个共享总线上，则会显示以下消息：

-   **Read**  
    包含要读的cache line的物理地址。
-   **Read Response**  
    包含之前"read"消息所请求的数据，“read response”消息可能是内存或者某个cache所回应。例如如果有相应数据的cache的状态是"modified"，那么这个"read response"消息就由cache来提供。
-   **Invalidate**  
    包含需要被失效的cache line的物理地址。所有其它cache必须移除相应的数据。
-   **Invalidate Acknowledge**  
    当cpu接收"invalidate"消息将相应数据移除后需要回应"invalidate acknowledge"消息。
-   **Read Invalidate**  
    "read"和"invalidate"消息的结合，包含要读的cache line的物理地址并且同时让其它cache移除相应的数据。因此它需要一个包含数据的"read response"消息和所有其它cpu的“invalidate acknowledge”消息。
-   **Writeback**  
    包含要被写回内存的数据地址和相应数据。

可以看出，共享内存的多处理器系统实则是一个消息传递的计算机，这意味着使用分布式共享内存的SMP集群是通过两个不同级别间的消息传递来实现共享内存的。

### MESI状态图

随着协议消息的发送和接收，给定的cache line的状态随之改变，如图3所示。

![](https://pic2.zhimg.com/v2-b26df79b2b01e1ae0e25ae0d4f03c3b1_b.jpg)

-   (a)：cache line被写回内存，但cpu将其保留在cache中并保留进一步修改它的权利。这个转变需要一条"write back"消息。
-   (b)：cpu写入已独占的cache line。这个转变不需要发送和接收任何消息。
-   (c)：cpu接收到对于它修改的cache line的"read invalidate"消息，它必须  
    失效它的本地副本，并回应数据和确认消息给相应的cpu。
-   (d)：cpu要对一个不在它cache中的数据做原子的read-modify-write操作，它发送"read invalidate"消息，接收到"read response"消息，但它接收到完整的回应后可以完成这个状态转变。
-   (e)：与d类似，只是数据以只读的状态在它的cache中，因此只需要发送"invalidate"消息并等待所有回应。
-   (f)：其它cpu读了cache line，“modified”状态的cpu回应了请求的数据，并变为只读状态。
-   (g)：与f类似，只是数据回应可以由这个cpu的cache或者内存来完成，无论哪种情况，这个cpu变为只读状态。
-   (h)：这个cpu需要写一些数据到cache line中，因此发送"invalidate"消息，直到接收到所有回应后才可以完成转变。
-   (i)：其它cpu需要对这个cache line中的数据做原子的read-modify-write操作，这个cpu失效自己cache中的数据。接收"read invalidate"消息并回应数据和确认消息。
-   (j)：cpu要对一个不在cache中的数据做store操作，发送"read invalidate"消息并等待数据和所有确认消息。当实际store操作完成可变为"modified"状态，转变(b)的过程。
-   (k)：cpu load一个不在它cache中的数据。通过"read"消息。
-   (l)：一些其它cpu对该cache line中的数据做了store操作，该cache line也在其它cpu的cache中，保持为只读状态。通过接收“invalidate”消息。

这里描述一个MESI的例子

![](https://pic3.zhimg.com/v2-d0884b127247950218e6c55e1be0a356_b.jpg)

如table1所示，一个cache line的数据初始位于地址为0的内存中，在包含四个cpu的系统中传输。第一列显示了操作顺序，第二列是执行操作的cpu，第三列是执行的操作，接着显示了每个cpu的cache line的状态。最后两列显示内存数据是最新的("V")还是过期的("I")。

初始数据在内存中，相应cpu cache lines都是“invalid”状态。依次执行以下步骤：

1.  cpu0 load了地址0的数据，cpu0对应的cache line进入"shared"状态。
2.  cpu3 load了地址0的数据，cpu3对应的cache line也进入“shared”状态。
3.  cpu0 load了地址8的数据，地址8的数据将地址0的数据淘汰。
4.  cpu2 意识到它很快将对其做store操作，因此它使用"read invalidate"消息来独占数据副本，将其从cpu3的cache中失效。
5.  cpu2 做了预期中的store操作，此时内存中的数据过期，变为"invalid"状态。
6.  cpu1 做了原子的增加操作，使用"read invalidate"来获取cpu2的数据并使其无效，cpu1的cache line进入"modified"状态。
7.  cpu1 读取了地址8的数据，使用"write back"消息将地址0的数据写回内存。

### 3. Store Buffers

虽然图1所示的cache结构对于给定cpu和给定数据集的重复读取和写入提供了良好的性能，但是对于给定cache line的第一次写入却十分差。如图4所示，展示了cpu0写一个cpu1 cache所持有的cache line的时间线，cpu0必须等待这段比较长的时间。

![](https://pic4.zhimg.com/v2-de1da4a7f56f21762cf3ed5fda5f009f_b.jpg)

如下图所示，一种方式是使用在CPU和它的cache之间增加"store buffers"的方式来避免write stall，有了store buffer之后，cpu可以简单地将它的write记录在buffer中，继续执行其它操作。

![](https://pic4.zhimg.com/v2-9f2b7390ea4ffd7138fecdfeffe85b23_b.jpg)

但是这也带来了复杂性，第一个问题，cache和store buffer中有两份数据，需要保证cpu从其自身视角观察自己行为时看到的是符合program order的。针对这个问题硬件设计者实现了"store forwarding"，当cpu执行load操作时，不仅需要看cache还要看store buffer。即使store操作还未被写到cache中，也会被后续的load操作所看到。

第二个问题，全局内存有序不能保证。如下例子中，初始变量a,b都为0，假设CPU0执行foo()，CPU1执行bar()，包含a的cache line只存在与CPU1的cache中，包含b的cache line存在于CPU0的cache中。

```
void foo(void) {  
  a = 1;   
  b = 1; 
} 
void bar(void) {   
  while(b==0) continue;   
  assert(a == 1); 
}
```

如下顺序的操作可能会发生：

1.  CPU0执行**a=1**，相应的cache line并不在其cache中，因此CPU0将修改后的值放到它的store buffer中，并发送给其它CPU一条"read invalidate"的消息。
2.  CPU1执行**while(b==0) continue**，对应的cache line并不在其cache中，因此发送一条"read"消息。
3.  CPU0执行**b=1**，相应的cache line存在其cache中，也就是cache line处于"modified"或者"exclusive"状态，因此将新的值保存在cache line中。
4.  CPU0接收到"read"消息，将"b"的最新值返回给CPU1，并将cache line的状态标记为"shared"。
5.  CPU1接收到包含"b"的cache line，并将其放入自己cache中。
6.  CPU1继续执行**while(b==0) continue**，发现b=1跳出循环，执行下一个操作。
7.  CPU1执行**assert(a == 1)**，而CPU1仍然看到的是a的旧值，因此assert失败。
8.  CPU1接收到"read invalidate"消息，将包含"a"的cache line返回给CPU0并将其从自己的cache中失效。
9.  CPU0接收到包含"a"的cache line，得到ack的消息，将store buffer中相应cache line应用到cache中。

这里可能有一个疑惑为什么第一步不直接发送"invalidate"而是"read invalidate"消息，CPU0执行的是修改操作，的确不需要其它CPU cache中"a"的旧值，但是这条cache line中包含的不仅仅只有变量"a"，而cache line是传输和状态更改的最小单位。

硬件不清楚哪些变量是可能相关的，硬件设计者无法提供直接的解决方式。因此他们提供了**memory-barrier**指令给软件开发者，由他们告诉CPU变量的关系。上述程序代码使用memory-barrier更改后如下：

```
void foo(void)
{
  a = 1;
  smp_mb();
  b = 1;
}
void bar(void)
{
  while(b==0) continue;
  assert(a == 1);
}
```

CPU执行smp_mb()时，后续的**store**必须等待store buffer刷到cache中才能存储到cache line中。CPU可能简单stall，直到store buffer清空，或者用store buffer保存后续的store直到之前的entries被应用。

使用memory barrier方式后，上述步骤2之后CPU0执行smp_mp()，标记所有当前的store-buffer的entries(包含a=1)，CPU0执行b=1，发现store buffer中有被标记的entries，相比之前直接将新的值存储到cache line中，而是将它暂存在store buffer中(unmarked entry)。 CPU0接收到"read"消息，将包含"b"的cache line返回给CPU1，此时的b=0，while继续执行。直到CPU1 invalidate了自己的包含"a"的cache line并回复给CPU0，CPU0将store buffer应用到cache。后续操作按cache line的状态正常执行，不再详述，最终assert执行成功。

### 4. Invalidate Queues and Memory Barriers

不幸的是，store buffer相对比较小，当CPU执行适度序列的store操作时就可能将其填满，CPU必须再次等待直到invalidate完成。相同的问题当一次memory barrier执行时也会出现，所有后续的store序列都要等待invalidation完成。

这种场景可以通过加速invalidate确认的速度来优化，一种方式是给每个cpu增加一个invalidate消息队列，或者叫作"invalidate queues"。

invalidate确认花费较长时间的一个原因是必须要确保相应cache line真正被失效，当cache繁忙时可能会被delay，比如cpu集中loading和storing cache中的数据时，另外如果短时间内有大量Invalidate消息，CPU可能会处理不及时，导致其它CPU等待。

而在发送确认其实不需要真正失效cache line，可以先放入invalidate queue中等待处理，但要在CPU发送任何有关该cache line的消息之前进行处理。

如图6所示，invalidate消息可以在放入invalidate queue中后立刻回复确认，而不用等待真正失效完成。当然，CPU在发送invalidate消息之前需要先查看自己的invalidate queue，如果invalidate queue存在相应的cache line，则CPU必须等待invalidate queue中的entry被处理。

![](https://pic1.zhimg.com/v2-63e65cf7be3b7d7637ee84b337cdd04c_b.jpg)

然而，invalidate消息缓存在invalidate queue中的方式再次引入额外的memory-misordering问题。

假设变量"a"和"b"的值初始都是0，"a"是只读副本(share 状态)，"b"被CPU0所owner(modified或者exclusive状态)。如下程序，CPU0执行foo函数，CPU1执行bar函数。

void foo(void) { a = 1; smp_mb(); b = 1; } void bar(void) { while(b==0) continue; assert(a == 1); }

可能的操作序列如下：

1.  cpu0执行**a=1**，在cpu0的cache中是只读状态，因此将a的新值放入store buffer中，并发送"invalidate"消息失效cpu1 cache中包含"a"的cache line。
2.  cpu1执行**while(b == 0) continue**，但b不在其cache中，因此发送"read"消息。
3.  cpu1接收到cpu0的"invalidate"消息，放入invalidate队列并立刻回复确认。此时cpu1的cache中仍然是a的旧值。
4.  cpu0接收到cpu1的回应，因此不需要通过smp_mb来处理，将a的值从store buffer应用到cache中。
5.  cpu0执行b=1，将新的值存储在cache line中。
6.  cpu0接收到"read"消息，将b的最新值返回给cpu1，并将其cache line标记为shared。
7.  cpu1接收到包含b的cache line，将其应用到自己的cache中。
8.  cpu1结束while循环，继续执行下一步。
9.  cpu1执行**assert(a==1)**，因为cpu1的cache中是a的旧值0，assert失败。
10.  cpu1处理队列中的invalidate消息，将a的cache line从自己的cache中失效。但是已经晚了。

可以看到第一步中仅仅发送"invalidate"消息，是因为只读副本中已经包含了cache line中的其它变量。

硬件设计者依旧无法直接解决，而memory-barrier可以于invalidate queue交互，当cpu执行memory barrier时，标记当前invalidate queue中的所有条目，强制后续的**load**等待，直到所有标记的条目成功应用到cache中。因此加入一条memory barrier的程序代码如下：

```
void foo(void)
{
  a = 1;
  smp_mb();
  b = 1;
}
void bar(void)
{
  while(b==0) continue;
  assert(a == 1);
}
```

cpu1执行完while循环后执行smp_mb()，标记invalidate queue中的条目，当执行**assert(a==1)**时，必须等待队列中的条目被应用才能load，"a"的cache line失效，cpu1继续执行load发现"a"的cache line不在cache中，因此发送"read"消息，获取到"a"的最新值，assert成功。

### Read and Write Memory Barriers

在上一部分中，memory barrier标记了store buffer和invalidate queue中的entries，但实际上foo函数不需要标记invalidate queue，bar函数不需要对store buffer做任何事情。

因此许多CPU架构都提供了较弱的memory-barrier指令，它们可以只执行两种指令中的一种，粗略地讲，"read memory barrier"仅标记invalidate queue，"write memory barrier"仅标记store buffer。这样的效果是，**读屏障仅对执行的cpu上的load做排序，保证所有在读屏障之前的load操作先于读屏障之后的load操作之前完成。写屏障仅仅排序该cpu上的store，所有写屏障之前的store操作先于之后的store操作完成**。完整的内存屏障对load和store都排序，但仅仅针对执行屏障的CPU。

使用读写屏障修改的程序如下：

```
void foo(void)
{
  a = 1;
  smp_mb();
  b = 1;
}
void bar(void)
{
  while(b==0) continue;
  smp_mb();
  assert(a == 1);
}
```

一些计算机具有更多种类的memory barrier，但是了解这三种可以对memory barrier有一个比较好的理解。

### 5. Types of Memory Barrier

[Doug Lea](https://link.zhihu.com/?target=http%3A//gee.cs.oswego.edu/dl)提出了四种类型的memory barrier，这四种barrier都是以防止特定类型的内存排序来命名的。不同的cpu有特定的指令，这四种可以比较好的匹配真实cpu的指令，虽然也不是完全匹配。大多数时候，真实的cpu指令是多种类型的组合，以达到特定的效果。

[这篇文章](https://link.zhihu.com/?target=https%3A//preshing.com/20120710/memory-barriers-are-like-source-control-operations/)将多处理器间的交互类比为多个程序员工作于同一远程仓库项目，程序员A和程序员B都在本地有一份完整的远程仓库中的副本。A本地一系列的修改就类似store，提交到远程仓库的过程中是reordered，B从远程仓库拉取新的提交就类似load，拉取的过程中也是reordered。比较形象描述了这四种类型的memory barrier。

![](https://pic1.zhimg.com/v2-c20ecf8d62271398dd9589bd84bc12b8_b.jpg)

### 特定cpu的Memory-Barrier指令

![](https://pic2.zhimg.com/v2-50da7b7ae1b83d662b9569ec47229659_b.jpg)

### 6. Memory Model

memory model是个比较大的话题，这里简单介绍一下。上述了解到内存乱序有多种类型，内存模型表明了给定处理器会出现什么类型的乱序。

按照所支持的内存顺序可分为弱一致性模型和强一致性模型，下图列举了各种模型的例子，物理设备代表了硬件内存模型，也列出了软件内存模型像c++11或者java。

![](https://pic1.zhimg.com/v2-d871fd016197ea691a73e1e110f9b4a8_b.jpg)

### Weak Memory Models

最弱的内存模型，上述四种memory reorder都可能出现。由特定cpu的barrier指令那张图中也可以看出，Alpha是weak memoy model的典型代表，主流处理器一般都不是这种模型。

C/C++11编程语言当使用low-level原子操作时暴露出一种软件层面的弱一致性模型，但如果是在像x86/64这种强一致性模型平台上使用可以不用关心，但从语言方面考虑需要涉及跨平台的支持。

### Weak With Data Dependency Ordering

这种内存模型大多数时候与Alpha一样弱，只有一种特定的情况：他们支持Data dependency ordering。这又个什么序？

简单看一下data dependency barriers，它是read barrier的一种较弱的形式，在大多数处理器中，data dependency barriers会比read barrier更轻量。Linux的[RCU机制](https://link.zhihu.com/?target=https%3A//lwn.net/Articles/262464/)会比较严重地依赖它。

Q = &P;

D = *Q;

这两个load中，第二个load依赖前一个的结果，data dependency barriers要求第二个load的结果值与Q的值一样新。

Alpha已经很少在使用了，但仍然有几种现代cpu家族有几乎相同的weak hardware ordering：

-   ARM 广泛使用在智能手机和平板电脑中，并且在多核配置中越来越受欢迎。
-   PowerPC 多核配置的Xbox360
-   Itanium 微软在Windows中不再支持它，但在Linux中仍然支持，在HP服务器中也可以找到。

### Strong Memory Models

不会出现LoadLoad、StoreStore、LoadStore乱序，但是StoreLoad乱序仍然存在。x86/64系列处理器通常是强序的，对于SPARC处理器，是以[TSO(Total Store Order)](https://link.zhihu.com/?target=https%3A//ljalphabeta.gitbooks.io/a-primer-on-memory-consistency-and-cache-coherenc/content/%25E7%25AC%25AC%25E5%259B%259B%25E7%25AB%25A0-total-store-order%25E5%2592%258Cx.html)模式运行的，是强硬件有序的另一个例子，这是比较常见的一种内存模型。

### Sequential Consistency

顺序一致性内存模型(SC)，相比TSO其实SC并不常见。在SC中，上述四种乱序都不会出现，就好像整个程序的执行被简化为每个线程按照顺序交互地执行有序指令。现在很难找到一个现代的多核设备来保证硬件级别的SC。而有一台SC的双处理器机器早在1989年就存在了：基于386的Compaq SystemPro。根据英特尔的文档，386不够先进，无法在运行时执行任何内存重新排序。

SC在高级编程语言中作为软件内存模型才更有意义。在Java5和更高版本中，可以将共享变量声明为volatile。在C++11中，当执行原子类型的操作时，可以使用默认排序约束，memory_order_seq_cst。执行这些操作时，工具链将限制编译器重新排序，并发出特定于CPU的指令，这些指令转化为合适的memory barrier。因此即使在weak order的多核设备上也可以实现SC内存模型。

参考：

[Memory Barriers: a Hardware View for Software Hackers](https://link.zhihu.com/?target=http%3A//www.puppetmastertrading.com/images/hwViewForSwHackers.pdf)

[https://preshing.com/20120710/memory-barriers-are-like-source-control-operations/](https://link.zhihu.com/?target=https%3A//preshing.com/20120710/memory-barriers-are-like-source-control-operations/)

[https://preshing.com/20120930/weak-vs-strong-memory-models/?spm=ata.13261165.0.0.f4ac2702O2pIga](https://link.zhihu.com/?target=https%3A//preshing.com/20120930/weak-vs-strong-memory-models/%3Fspm%3Data.13261165.0.0.f4ac2702O2pIga)

[https://www.kernel.org/doc/Documentation/memory-barriers.txt](https://link.zhihu.com/?target=https%3A//www.kernel.org/doc/Documentation/memory-barriers.txt)

[https://ljalphabeta.gitbooks.io/a-primer-on-memory-consistency-and-cache-coherenc/content/%E7%AC%AC%E5%9B%9B%E7%AB%A0-total-store-order%E5%92%8Cx.html](https://link.zhihu.com/?target=https%3A//ljalphabeta.gitbooks.io/a-primer-on-memory-consistency-and-cache-coherenc/content/%25E7%25AC%25AC%25E5%259B%259B%25E7%25AB%25A0-total-store-order%25E5%2592%258Cx.html)

[http://gee.cs.oswego.edu/dl/jmm/cookbook.html](https://link.zhihu.com/?target=http%3A//gee.cs.oswego.edu/dl/jmm/cookbook.html)
