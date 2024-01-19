---
source: https://zhuanlan.zhihu.com/p/508315407
---
总目录：

前文：

> [【Cache篇】初见Cache](https://link.zhihu.com/?target=https%3A//blog.csdn.net/qq_38131812/article/details/124512602%3Fspm%3D1001.2014.3001.5501)  
> [【Cache篇】Cache的映射方式](https://link.zhihu.com/?target=https%3A//blog.csdn.net/qq_38131812/article/details/124514857%3Fspm%3D1001.2014.3001.5501)  
> [【Cache篇】包容性和排他性的Cache](https://link.zhihu.com/?target=https%3A//blog.csdn.net/qq_38131812/article/details/124517207%3Fspm%3D1001.2014.3001.5501)  
> [【Cache篇】Cache的分类](https://link.zhihu.com/?target=https%3A//qmiller.blog.csdn.net/article/details/124523046%3Fspm%3D1001.2014.3001.5502)

对于单核CPU来说，不存在数据一致性问题；然而对于多核系统来说，不同CPU上的cache和ram可能具有同一个数据的多个副本。这就会导致数据观察者（CPU/GPU/DMA）能看到的数据不一致。

因此，维护cache一致性就非常有必要。维护cache一致性的关键是需要跟踪每个Cache Line的状态，并且根据读写操作和总线上相应的传输内容来更新Cache Line在不同CPU核心上的Cache Hit状态。

维护cache一致性有**软件**和**硬件**两种方式。现在大多数处理器都采用硬件来维护。在处理器中通过cache一致性协议来实现，这些协议维护了一个有限状态机，根据存储器读写指令/总线上的传输内容，进行状态迁移/相应的cache操作来维护cache一致性。

cache一致性协议分为主要有两大类：

-   监听协议，每个cache被监听/监听其他cache的总线活动；
-   目录协议，全局统一管理cache状态。

在这里我们介绍MESI协议（Write-Once总线监听协议），MESI这四个字母分别代表Modify、Exclusive、Shared和Invalid。Cache Line的状态必须是这四个中的一种。前三种状态均是数据有效下的状态。Cache Line有两个标志-脏（dirty）和有效（valid）。**脏代表该数据和内存不一致。**只有干净的数据才能被多个Cache Line共享。

| 状态 | 说明 |
| --- | --- |
| M | 数据已被修改，和内存的数据不一致，该数据只存在于此Cache Line中 |
| E | 数据和内存中一致，该数据只存在于此Cache Line中 |
| S | 数据和内存中一致，多个Cache Line持有这行数据的副本 |
| I | 这行数据无效 |

MESI在总线上的操作分为**本地读写**和**总线操作**。当操作类型是**本地**读写时，Cache Line的状态指的是本地CPU；而当操作类型是总线读写时，Cache Line的状态指的是远端CPU。

| 操作类型 | 描述 |
| --- | --- |
| 本地读 | 本地CPU读取Cache Line |
| 本地写 | 本地CPU更新Cache Line |
| 总线读 | 总线监听一个来自其他cpu的读cache信号。收到信号的cpu先检查cache是否存在该数据，然后广播应答 |
| 总线写 | 总线监听一个来自其他cpu的写cache信号。收到信号的cpu先检查cache是否存在该数据，然后广播应答 |
| 总线更新 | 总线收到更新请求，请求其他cpu干活。其他cpu收到请求后，若cpu有cache副本，则使其Cache Line无效 |
| 刷新 | 总线监听到刷新请求，收到请求的cpu把自己的Cache Line内容写回主内存 |
| 刷新到总线 | 收到该请求的cpu会将Cache Line内容发送到总线上，这也发送这个请求的CPU就可以获取到这个Cache Line的内容 |

下图中实线表示处理器请求响应，虚线表示总线监听响应。

解读下图得分两种情况：

-   如果CPU发现本地副本，并且这个Cache Line的状态为S，下图中的I->S后，然后在总线上回复FlushOpt信号（S->I）,Cache Line被发到总线上，其状态还是S。
-   如果CPU发现本地副本，并且这个Cache Line的状态为E，下图中的I->E后,则在总线上回复FlushOpt（E->I）,Cache Line被发到总线上，其状态变成了S（E->S）。

![](https://pic4.zhimg.com/v2-773f717ef7341273e5f935b04cd9c293_b.jpg)

> MOESI增加了一个Owned状态并重新定义了Shared状态。  
> O状态：表示当前Cache Line数据是当前CPU系统最新的数据副本，其他CPU可能拥有该Cache Line的副本，状态为S。  
> S状态：Cache Line的数据不一定于内存一致。如果其他CPU的Cache Line中**不存在状态O**的副本，则该Cache Line于内存中的**数据一致**；如果其他CPU的Cache Line中存在状态O的副本，则该Cache Line于内存中的数据**不一致**。

### **初始状态为I**

### **发起读操作**

假设CPU0发起了**本地读**请求，CPU0发出读PrRd请求，因为是本地cache line是无效状态，所以会在总线上产生一个BusRd信号，然后广播到其他CPU。其他CPU会监听到该请求（BusRd信号的请求）并且检查它们的缓存来判断是否拥有了该副本。

对于初始状态为I的cache来说，有四种可能的状态图。

-   全部为I  
    假设CPU1,CPU2, CPU3上的cache line都没有缓存数据，状态都是I，那么CPU0会从内存中读取数据到L1 cache，把cache line状态设置为E。
-   I->S  
    如果CPU1发现本地副本，并且这个cache line的状态为S，那么在总线上回复一个FlushOpt信号，即把当前的cache line的内容发送到总线上。刚才发出PrRd请求的CPU0，就能得到这个cache line的数据，然后CPU0状态变成S。这个时候的cache line的变化情况是：CPU0上的cache line从I->S，CPU1上的cache line保存S不变。
-   I->E  
    假设CPU2发现本地副本并且cache line的状态为E，则在总线上回应FlushOpt信号，把当前的cache line的内容发送到总线上，CPU2上的高速缓存行的状态变成S。这个时候 cache line的变化情况：CPU0的cache line变化是I->S，而CPU2上的cache line从E变成了S。
-   I->M  
    假设CPU3发现本地副本并且cache line的状态为M，将数据更新到内存，这时候两个cache line的状态都为S。cache line的变化情况：CPU0上cache line变化是I->S，CPU3上的cache line从M变成了S。

### **发起写操作**

假设CPU0发起了**本地写**请求，即CPU0发出读PrWr请求：

-   由于CPU0的本地cache line是无效的，所以，CPU0发送BusRdX信号到总线上。这里的本地写操作，就变成了总线写操作，于是我们要看其他CPU的情况。
-   其他CPU（例如CPU1等）收到BusRdX信号，先检查自己的高速缓存中是否有缓存副本，广播应答信号。
-   假设CPU1上有这份数据的副本，且状态为S，CPU1收到一个BusRdX信号指挥，会回复一个flushopt信号，把数据发送到总线上，把自己的cache line设置为无效，状态变成I，然后广播应答信号。
-   假设CPU2上有这份数据的副本，且状态为E，CPU2收到这个BusRdx信号之后，会回复一个flushopt信号，把数据发送到总线上，把自己的cache line设置为无效，然后广播应答信号。
-   若其他CPU上也没有这份数据的副本，也要广播一个应答信号。
-   CPU0会接收其他CPU的所有的应答信号，确认其他CPU上没有这个数据的缓存副本后。CPU0会从总线上或者从内存中读取这个数据：  
    a)如果其他CPU的状态是S或者E的时候，会把最新的数据通过flushopt信号发送到总线上。  
    b)如果总线上没有数据，那么直接从内存中读取数据。  
    最后才修改数据，并且CPU0本地cache line的状态变成M。  
    

![](https://pic3.zhimg.com/v2-70039dc7fcef76cf1dba646c388f7f9e_b.jpg)

### **初始状态为M**

对于M的本地读写操作均无效，因为M表示此cache line的数据是最新且dirty的。

### **收到总线读操作**

假设是CPU0的cache line的状态为M，而在其他CPU上没有这个数据的副本。当其他CPU（如CPU1）想读这份数据时，CPU1会发起一次总线读操作，所以，流程是这样的：

-   若CPU0上有这个数据的副本，那么CPU0收到信号后把cache line的内容发送到总线上，然后CPU1就获取这个cache line的内容。另外，CPU0会把相关内容发送到主内存中，把cache line的内容写入主内存中。这时候CPU0的状态从M->S
-   更改CPU1上的cache line状态为S。

### **收到总线写操作**

假设数据在本地CPU0上有副本并且状态为M，而其他CPU上没有这个数据的副本。若某个CPU（假设CPU1）想更新（写）这份数据，CPU1就会发起一个总线写操作。

1.  若CPU0上有这个数据的副本，CPU0收到总线写信号后，把自己的cache line的内容发送到内存控制器，并把该cache line的内容写入主内存中。CPU0上的cache line状态变成I。
2.  CPU1从总线或者内存中取回数据到本地cache line，然后修改自己本地cache line的内容。CPU1的状态变成M。

![](https://pic1.zhimg.com/v2-62581943cb0c9862bbdb7994a1af57f4_b.jpg)

### **初始状态为S**

当本地CPU的cache line状态为S时，

-   如果CPU发出本地读操作，S状态不变。
-   如果CPU收到总线读（BusRd），状态不变，回应一个FlushOpt信号，把数据发到总线上。

如果CPU发出本地写操作（PrWr）

-   发送BusRdX信号到总线上。
-   本地CPU修改本地高速缓存行的内容，状态变成M。
-   发送BusUpgr信号到总线上。
-   其他CPU收到BusUpgr信号后，检查自己的高速缓存中是否有副本，若有，将其状态改成I。

![](https://pic1.zhimg.com/v2-4d007fd6ebe0f91c47174af82999a6c0_b.jpg)

### **初始状态为E**

当本地CPU的cache line状态为E的时候。

-   本地读，从该cache line中取数据，状态不变。
-   本地写，修改该cache line的数据，状态变成M。
-   收到一个总线读信号，独占状态的cache line是干净的，因此状态变成S。  
    

-   cache line的状态先变成S。
-   发送FlushOpt信号，把cache line的内容发送到总线上。
-   发出总线读信号的CPU，从总线上获取了数据，状态变成S。

-   收到一个总线写，数据被修改，该cache line不再使用，状态变成I。  
    

-   cache line的状态先变成I。
-   发送FlushOpt信号，把cache line的内容发送到总线上。
-   发出总线写信号的CPU，从总线上获取了数据，然后修改，状态变成M。

![](https://pic1.zhimg.com/v2-9fab197e8dc4d0836c06a58496bd80a8_b.jpg)

## **四核CPU的MESI状态转换**

现在系统中有4个CPU，每个CPU都有各自的一级cache，它们都想访问相同地址的数据A，大小为64字节。假设：

> T0时刻：4个CPU的L1 cache都没有缓存数据A，cache line的状态为I (无效的)  
> T1时刻：CPU0率先发起访问数据A的操作  
> T2时刻：CPU1也发起读数据操作  
> T3时刻：CPU2的程序想修改数据A中的数据

那么在这四个时间点，MESI状态图是如何变化的呢？下面我会画出这四张MESI状态图。

### **T0**

首先对于T0时刻，所有的cache都是I。

### **T1**

CPU0率先发起访问数据A的操作。

1.  对于CPU0来说，这是一次本地读。由于CPU0本地的cache并没有缓存数据A，因此CPU0首先发送一个BusRd信号到总线上去查询它的其他几个兄弟有没有数据A。如果其他CPU有数据A，就会通过总线发出应答。如果现在CPU1有数据A，那么它就会回应CPU0，告诉CPU0有数据。这里的情况是四个CPU都没有数据A，**此时对于CPU0来说，需要去内存中读取数据A存到本地cache line中，然后设置此cache为独占状态E。**

![](https://pic4.zhimg.com/v2-453ae7abb866325a10af87c0260cacc7_b.jpg)

### **T2**

CPU1也发起读数据操作。

此时整个系统里只有CPU0中有缓存副本，CPU0会把缓存的数据发送到总线上并且应答CPU1，最后CPU0和CPU1都有缓存副本，状态都设置为S。

![](https://pic4.zhimg.com/v2-303558d5df5348a1847a614f8e338abb_b.jpg)

### **T3**

CPU2的程序企图修改数据A中的数据。

此时CPU2的本地cache line并没有缓存数据A，高速缓存行的状态为I，因此，这是一次**本地写**操作。首先CPU2会发送BusRdX信号到总线上，其他CPU收到BusRdX信号后，检查自己的cache中是否有该数据。若CPU0和CPU1发现自己都缓存了数据A，那么会使这些cache line无效，然后发送应答信号。虽然CPU3没有缓存数据A，但是它回复了一条应答信号，表明自己没有缓存数据A。CPU2收集完所有的应答信号之后，**把CPU2本地的cache line状态改成M**，M状态表明这个cache line已经被自己修改了，而且已经使其他CPU上相应的cache line无效。

![](https://pic2.zhimg.com/v2-7a0e5ed13e120f5385353fbba08dc9d5_b.jpg)

> 本文参考了笨叔叔的《奔跑吧Linux第二版》。
