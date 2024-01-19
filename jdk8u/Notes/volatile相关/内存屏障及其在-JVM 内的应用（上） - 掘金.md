---
source: https://juejin.cn/post/6844904143207792648
---
作者：[LeanCloud 后端高级工程师 郭瑞](https://link.juejin.cn/?target=www.leancloud.cn "www.leancloud.cn")

内容分享视频版本: [内存屏障及其在-JVM-内的应用](https://link.juejin.cn/?target=https%3A%2F%2Fwww.bilibili.com%2Fvideo%2FBV1X54y1Q75J "https://www.bilibili.com/video/BV1X54y1Q75J")

## MESI

MESI 的词条在这里：[MESI protocol - Wikipedia](https://link.juejin.cn/?target=https%3A%2F%2Fen.wikipedia.org%2Fwiki%2FMESI_protocol "https://en.wikipedia.org/wiki/MESI_protocol")，它是一种缓存一致性维护协议。MESI 表示 Cache Line 的四种状态，`modified`, `exclusive`, `shared`, `invalid`。

-   `modified`：CPU 拥有该 Cache Line 且将其做了修改，CPU 需要保证在重用该 Cache Line 存其它数据前，将修改的数据写入主存，或者将 Cache Line 转交给其它 CPU 所有；
-   `exclusive`：跟 `modified` 类似，也表示 CPU 拥有某个 Cache Line 但还未来得及对它做出修改。CPU 可以直接将里面数据丢弃或者转交给其它 CPU
-   `shared`：Cache Line 的数据是最新的，可以丢弃或转交给其它 CPU，但当前 CPU 不能对其进行修改。要修改的话需要转为 `exclusive` 状态后再进行；
-   `invalid`：Cache Line 内的数据为无效，也相当于没存数据。CPU 在找空 Cache Line 缓存数据的时候就是找 `invalid` 状态的 Cache Line；

有个超级棒的可视化工具，能看到 Cache 是怎么在这四个状态之间流转的：[VivioJS MESI animation help](https://link.juejin.cn/?target=https%3A%2F%2Fwww.scss.tcd.ie%2FJeremy.Jones%2FVivioJS%2Fcaches%2FMESIHelp.htm "https://www.scss.tcd.ie/Jeremy.Jones/VivioJS/caches/MESIHelp.htm")。Address Bus 和 Data Bus 都是所有 CPU 都能监听到变化。比如 CPU 0 要读数据会把请求先发去 Address Bus，Memory 和其它 CPU 都会收到这次请求。Memory 通过 Data Bus 发数据时候也是所有 CPU 都会收到数据。我理解这就是能实现出来 [Bus snooping](https://link.juejin.cn/?target=https%3A%2F%2Fen.wikipedia.org%2Fwiki%2FBus_snooping "https://en.wikipedia.org/wiki/Bus_snooping")的原因。另外这个工具上可以使用鼠标滚轮上下滚，看每个时钟下数据流转过程。

![](https://p1-jj.byteimg.com/tos-cn-i-t2oaga2asx/gold-user-assets/2020/4/28/171bfcaac148c3d4~tplv-t2oaga2asx-zoom-in-crop-mark:4536:0:0:0.image)

后续内容以及图片大多来自 [Is Parallel Programming Hard, And, If So, What Can You Do About It?](https://link.juejin.cn/?target=https%3A%2F%2Fmirrors.edge.kernel.org%2Fpub%2Flinux%2Fkernel%2Fpeople%2Fpaulmck%2Fperfbook%2Fperfbook.html "https://mirrors.edge.kernel.org/pub/linux/kernel/people/paulmck/perfbook/perfbook.html") 这本书的附录 C。因为 MESI 协议本身非常复杂，各种状态流转很麻烦，所以这本书里对协议做了一些精简，用比较直观的方式来介绍这个协议，好处是让理解更容易。如果想知道协议的真实样貌需要去看上面提到的 WIKI。

### 协议

-   Read: 读取一条物理内存地址上的数据；
-   Read Response： 包含 `Read` 命令请求的数据结果，可以是主存发来的，也可以是其它 CPU 发来的。比如被读的数据在别的 CPU 的 Cache Line 上处于 `modified` 状态，这个 CPU 就会响应 `Read` 命令
-   Invalidate：包含一个物理内存地址，用于告知其它所有 CPU 在自己的 Cache 中将这条地址对应的 Cache Line 清理；
-   Invalidate Acknowledge：收到 `Invalidate`，在清理完自己的 Cache 后，CPU 需要回应 `Invalidate Acknowledge`；
-   Read Invalidate：相当于将 `Read` 和 `Invalidate` 合起来发送，一方面收到请求的 CPU 要考虑构造 `Read Response` 还要清理自己的 Cache，完成后回复 `Invalidate Acknowledge`，即要回复两次；
-   Writeback：包含要写的数据地址，和要写的数据，用于让对应数据写回主存或写到某个别的地方。

发起 `Writeback` 一般是因为某个 CPU 的 Cache 不够了，比如需要新 Load 数据进来，但是 Cache 已经满了。就需要找一个 Cache Line 丢弃掉，如果这个被丢弃的 Cache Line 处于 `modified` 状态，就需要触发一次 `WriteBack`，可能是把数据写去主存，也可能写入同一个 CPU 的更高级缓存，还有可能直接写去别的 CPU。比如之前这个 CPU 读过这个数据，可能对这个数据有兴趣，为了保证数据还在缓存中，可能就触发一次 `Writeback` 把数据发到读过该数据的 CPU 上。

举例：

![](https://p1-jj.byteimg.com/tos-cn-i-t2oaga2asx/gold-user-assets/2020/4/28/171bfcadcf8f5191~tplv-t2oaga2asx-zoom-in-crop-mark:4536:0:0:0.image)

左边是操作执行顺序，CPU 是执行操作的 CPU 编号。Operation 是执行的操作。RMW 表示读、修改、写。Memory 那里 V 表示内存数据是 Valid。

0.  一开始所有缓存都是 Invalid;
1.  CPU 0 通过 `Read` 消息读 0 地址数据，0 地址所在 Cache Line 变成 `Shared` 状态;
2.  CPU 3 再执行 `Read` 读 0 地址，它的 0 地址所在 Cache Line 也变成 `Shared`;
3.  CPU 0 又从 Memory 读取 8 地址，替换了之前存 0 地址的 Cache Line。8 地址所在 Cache Line 也标记为 `Shared`。
4.  CPU 2 因为要读取并修改 0 地址数据，所以发送 `Read Invalidate` 请求，首先 Load 0 地址数据到 Cache Line，再让当前 Cache 了 0 地址数据的 CPU 3 的 Cache 变成 `Invalidate`;
5.  CPU 2 修改了 0 地址数据，0 地址数据在 Cache Line 上进入 `Modified` 状态，并且 Memory 上数据也变成 Invalid 的；
6.  CPU 1 发送 `Read Invalidate` 请求，从 CPU 2 获取 0 地址的最新修改，并设置 CPU 2 上 Cache Line 为 `Invalidate`。CPU 1 在读取到 0 地址最新数据后对其进行修改，Cache Line 进入 `Modified` 状态。**注意**这里 CPU 2 没有 `Writeback` 0 地址数据到 Memory
7.  CPU 1 读取 8 地址数据，因为自己的 Cache Line 满了，所以 `Writeback` 修改后的 0 地址数据到 Memory，读 8 地址数据到 Cache Line 设置为 `Shared` 状态。此时 Memory 上 0 地址数据进入 Valid 状态

真实的 MESI 协议非常复杂，MESI 因为是缓存之间维护数据一致性的协议，所以它所有请求都分为两端，请求来自 CPU 还是来自 Bus。请求来源不同在不同状态下也有不同结果。下面图片来自 wiki [MESI protocol - Wikipedia](https://link.juejin.cn/?target=https%3A%2F%2Fen.wikipedia.org%2Fwiki%2FMESI_protocol "https://en.wikipedia.org/wiki/MESI_protocol")，只是贴一下大概瞧瞧就好。

![](https://p1-jj.byteimg.com/tos-cn-i-t2oaga2asx/gold-user-assets/2020/4/28/171bfcb359251f1c~tplv-t2oaga2asx-zoom-in-crop-mark:4536:0:0:0.image)

## Memory Barrier

### Store Buffer

假设 CPU 0 要写数据到某个地址，有两种情况：

1.  CPU 0 已经读取了目标数据所在 Cache Line，处于 `Shared` 状态；
2.  CPU 0 的 Cache 中还没有目标数据所在 Cache Line；

第一种情况下，CPU 0 只要发送 `Invalidate` 给其它 CPU 即可。收到所有 CPU 的 `Invalidate Ack` 后，这块 Cache Line 可以转换为 `Exclusive` 状态。第二种情况下，CPU 0 需要发送 `Read Invalidate` 到所有 CPU，拥有最新目标数据的 CPU 会把最新数据发给 CPU 0，并且会标记自己的这块 Cache Line 为无效。

无论是 `Invalidate` 还是 `Read Invalidate`，CPU 0 都得等其他所有 CPU 返回 `Invalidate Ack` 后才能安全操作数据，这个等待时间可能会很长。因为 CPU 0 这里只是想写数据到目标内存地址，它根本不关心目标数据在别的 CPU 上当前值是什么，所以这个等待是可以优化的，办法就是用 Store Buffer:

![](https://p1-jj.byteimg.com/tos-cn-i-t2oaga2asx/gold-user-assets/2020/4/28/171bfcb7c66238de~tplv-t2oaga2asx-zoom-in-crop-mark:4536:0:0:0.image)

每次写数据时一方面发送 `Invalidate` 去其它 CPU，另一方面是将新写的数据内容放入 Store Buffer。等到所有 CPU 都回复 `Invalidate Ack` 后，再将对应 Cache Line 数据从 Store Buffer 移除，写入 CPU 实际 Cache Line。

除了避免等待 `Invalidate Ack` 外，Store Buffer 还能优化 `Write Miss` 的情况。比如即使只用一个 CPU，如果目标待写内存不在 Cache，正常来说需要等待数据从 Memory 加载到 Cache 后 CPU 才能开始写，那有了 Store Buffer 的存在，如果待写内存现在不在 Cache 里可以不用等待数据从 Memory 加载，而是把新写数据放入 Store Buffer，接着去执行别的操作，等数据加载到 Cache 后再把 Store Buffer 内的新写数据写入 Cache。

另外对于 `Invalidate` 操作，有没有可能两个 CPU 并发的去 `Invalidate` 某个相同的 Cache Line？

这种冲突主要靠 Bus 解决，可以从前面 MESI 的可视化工具看到，所有操作都得先访问 Address Bus，访问时会锁住 Address Bus，所以一段时间内只有一个 CPU 会操作 Bus，会操作某个 Cache Line。但是两个 CPU 可以不断相互修改同一个内存数据，导致同一个 Cache Line 在两个 CPU 上来回切换。

### Store Forwarding

前面图上画的 Store Buffer 结构还有问题，主要是读数据的时候还需要读 Store Buffer 里的数据，而不是写完 Store Buffer 就结束了。

比如现在有这个代码，a 一开始不在 CPU 0 内，在 CPU 1 内，值为 0。b 在 CPU 0 内：

```
a = 1;
b = a + 1;
assert(b == 2); 
复制代码
```

CPU 0 因为没缓存 a，写 a 为 1 的操作要放入 Store Buffer，之后需要发送 `Read Invalidate` 去 CPU 1。等 CPU 1 发来 a 的数据后 a 的值为 0，如果 CPU 0 在执行 `a + 1` 的时候不去读取 Store Buffer，则执行完 b 的值会是 1，而不是 2，导致 assert 出错。

所以更正常的结构是下图：

![](https://p1-jj.byteimg.com/tos-cn-i-t2oaga2asx/gold-user-assets/2020/4/28/171bfcbecff97819~tplv-t2oaga2asx-zoom-in-crop-mark:4536:0:0:0.image)

另外一开始 CPU 0 虽然就是想写 a 的值 为 1，根本不关心它现在值是什么，但也不能直接发送 `Invalidate` 给其它 CPU。因为 a 所在 Cache Line 上可能不只 a 在，可能还有别的数据在，如果直接发送 `Invalidate` 会导致 Cache Line 上不属于 a 的数据丢失。所以 `Invalidate` 只有在 Cache Line 处于 `Shared` 状态，准备向 `Exclusive` 转变时才会使用。

### Write Barrier

```
// CPU 0 执行 foo(), 拥有 b 的 Cache Line
void foo(void) 
{ 
    a = 1; 
    b = 1; 
} 
// CPU 1 执行 bar()，拥有 a 的 Cache Line
void bar(void)
{
    while (b == 0) continue; 
    assert(a == 1);
} 
复制代码
```

对 CPU 0 来说，一开始 Cache 内没有 a，于是发送 `Read Invalidate` 去获取 a 所在 Cache Line 的修改权。a 写入的新值存在 Store Buffer。之后 CPU 0 就可以立即写 `b = 1` 因为 b 的 Cache Line 就在 CPU 0 上，处于 `Exclusive` 状态。

对 CPU 1 来说，它没有 b 的 Cache Line 所以需要先发送 `Read` 读 b 的值，如果此时 CPU 0 刚好写完了 `b = 1`，CPU 1 读到的 b 的值就是 1，就能跳出循环，此时如果还未收到 CPU 0 发来的 `Read Invalidate`，或者说收到了 CPU 0 的 `Read Invalidate` 但是只处理完 `Read` 部分给 CPU 0 发回去 a 的值即 `Read Response` 但还未处理完 `Invalidate`，也即 CPU 1 还拥有 a 的 Cache Line，CPU 0 还是不能将 a 的写入从 Store Buffer 写到 CPU 0 的 Cache Line 上。这样 CPU 1 上 a 读到的值就是 0，从而触发 assert 失败。

上述问题原因就是 Store Buffer 的存在，如果没有 Write Barrier，写入操作可能会乱序，导致后一个写入提前被其它 CPU 看到。

这里可能的一个疑问是，上述问题能出现意味着 CPU 1 在收到 `Read Invalidate` 后还未处理完就能发 `Read` 请求给 CPU 0 读 b 变量的 Cache Line，感觉上似乎不合理，因为似乎 Cache 应该是收到一个请求处理一个请求才对。这里可能有理解的盲区，我猜测是因为 `Read Invalidate` 实际分为两个操作，一个 `Read` 一个 `Invalidate`，`Read` 可以快速返回，但是 `Invalidate` 操作可能比较重，比如需要写回主存，那 Cache 可能有什么优化能允许等待执行完 `Invalidate` 返回 `Invalidate Ack` 前再收到 CPU 发来的轻量级的 `Read` 操作时可以把 `Read` 先丢出去，毕竟 CPU 读操作对 Cache 来说只需要转发，`Invalidate` 则是真的要 Cache 去操作自己的标志之类的，做的事情更多。

上面问题解决办法就是 Write Barrier，其作用是将 Write Barrier 之前所有操作的 Cache Line 都打上标记，Barrier 之后的写入操作不能直接操作 Cache Line 而也要先写 Store Buffer 去，只是这种拥有 Cache Line 但因为 Barrier 关系也写入 Store Buffer 的 Cache Line 不用打特殊标记。等 Store Buffer 内带着标记的写入因为收到 `Invalidate Ack` 而能写 Cache Line 后，这些没有打标记的写入操作才能写入 Cache Line。

相同代码带着 Write Barrier：

```
// CPU 0 执行 foo(), 拥有 b 的 Cache Line
void foo(void) 
{ 
    a = 1; 
    smp_wmb();
    b = 1; 
} 
// CPU 1 执行 bar()，拥有 a 的 Cache Line
void bar(void)
{
    while (b == 0) continue; 
    assert(a == 1);
} 
复制代码
```

此时对 CPU 0 来说，a 写入 Store Buffer 后带着特殊标记，b 的写入也得放入 Store Buffer。这样如果 CPU 1 还未返回 `Invalidate Ack`，CPU 0 对 b 的写入在 CPU 1 上就不可见。CPU 1 发来的 `Read` 读取 b 拿到的一直是 0。等 CPU 1 回复 `Invalidate Ack` 后，Ack 的是 a 所在 Cache Line，于是 CPU 0 将 Store Buffer 内 `a = 1` 的写入写到自己的 Cache Line，在从 Store Buffer 内找到所有排在 a 后面不带特殊标记的写入，即 `b = 1` 写入自己的 Cache Line。这样 CPU 1 再读 b 就会拿到新值 1，而此时 a 在 CPU 1 上因为回复过 `Invalidate Ack`，所以 a 会是 `Invalidate` 状态，重新读 a 后得到 a 值为 1。assert 成功。

### Invalidate Queue

每个 CPU 上 Store Buffer 都是有限的，当 Store Buffer 被写满之后，后续写入就必须等 Store Buffer 有位置后才能再写。就导致了性能问题。特别是 Write Barrier 的存在，一旦有 Write Barrier，后续所有写入都得放入 Store Buffer 会让 Store Buffer 排队写入数量大幅度增加。所以需要缩短写入请求在 Store Buffer 的排队时间。

之前提到 Store Buffer 存在原因就是等待 `Invalidate Ack` 可能较长，那缩短 Store Buffer 排队时间办法就是尽快回复 `Invalidate Ack`。`Invalidate` 操作时间长来自两方面：

1.  如果 Cache 特别繁忙，比如 CPU 有大量的在 Cache 上的读取、写入操作，可能导致 Cache 错过 `Invalidate` 消息，导致 `Invalidate` 延迟 (我认为是收到总线上信号后如果来不及处理可以丢掉，等信号发送方待会重试)
2.  可能短时间到来大量的 `Invalidate`，导致 Cache 来不及处理这么多 `Invalidate` 请求。每个还得回复 `Invalidate Ack`，也会占用总线通信时间

于是解决办法就是为每个 CPU 再增加一个 Invalidate Queue。收到 `Invalidate` 请求后将请求放入队列，并立即回复 `Ack`。

![](https://p1-jj.byteimg.com/tos-cn-i-t2oaga2asx/gold-user-assets/2020/4/28/171bfcc5cd68dcb2~tplv-t2oaga2asx-zoom-in-crop-mark:4536:0:0:0.image)

这么做导致的问题也是显而易见的。一个被 Invalidate 的 Cache Line 本来应该处于 `Invalidate` 状态，CPU 不该读、写里面数据的，但因为 `Invalidate` 请求被放入队列，CPU 还认为自己可以读写这个 Cache Line 而在操作老旧数据。从上图能看到 CPU 和 Invalidate Queue 在 Cache 两端，所以跟 Store Buffer 不同，CPU 不能去 Invalidate Queue 里查一个 Cache Line 是否被 Invalidate，这也是为什么 CPU 会读到无效数据的原因。

另一方面，Invalidate Queue 的存在导致如果要 Invalidate 一个 Cache Line，得先把 CPU 自己的 Invalidate Queue 清理干净，或者至少有办法让 Cache 确认一个 Cache Line 在自己这里状态是非 Invalidate 的。

### Read Barrier

因为 Invalidate Queue 的存在，CPU 可能读到旧值，场景如下：

```
// CPU 0 执行 foo(), a 处于 Shared，b 处于 Exclusive
void foo(void) 
{ 
    a = 1; 
    smp_wmb();
    b = 1; 
} 
// CPU 1 执行 bar()，a 处于 Shared 状态
void bar(void)
{
    while (b == 0) continue; 
    assert(a == 1);
} 
复制代码
```

CPU 0 将 `a = 1`写入 Store Buffer，发送 `Invalidate` (不是 `Read Invalidate`，因为 a 是 `Shared` 状态) 给 CPU 1。CPU 1 将 `Invalidate` 请求放入队列后立即返回了，所以 CPU 0 很快能将 1 写入 a、b 所在 Cache Line。CPU 1 再去读 b 的时候拿到 b 的新值 0，读 a 的时候认为 a 处于 `Shared` 状态于是直接读 a，拿到 a 的旧值比如 0，导致 assert 失败。最后，即使程序运行失败了，CPU 1 还需要继续处理 Invalidate Queue，把 a 的 Cache Line 设置为无效。

解决办法是加 Read Barrier。Read Barrier 起作用不是说 CPU 看到 Read Barrier 后就立即去处理 Invalidate Queue，把它处理完了再接着执行剩下东西，而只是标记 Invalidate Queue 上的 Cache Line，之后继续执行别的指令，直到看到下一个 Load 操作要从 Cache Line 里读数据了，CPU 才会等待 Invalidate Queue 内所有刚才被标记的 Cache Line 都处理完才继续执行下一个 Load。比如标记完 Cache Line 后，又有新的 `Invalidate` 请求进来，因为这些请求没有标记，所以下一次 Load 操作是不会等他们的。

```
// CPU 0 执行 foo(), a 处于 Shared，b 处于 Exclusive
void foo(void) 
{ 
    a = 1; 
    smp_wmb();
    b = 1; 
} 
// CPU 1 执行 bar()，a 处于 Shared 状态
void bar(void)
{
    while (b == 0) continue; 
    smp_rmb();
    assert(a == 1);
} 
复制代码
```

有了 Read Barrier 后，CPU 1 读到 b 为 0后，标记所有 Invalidate Queue 上的 Cache Line 继续运行。下一个操作是读 a 当前值，于是开始等所有被标记的 Cache Line 真的被 Invalidate 掉，此时再读 a 发现 a 是 `Invalidate` 状态，于是发送 `Read` 到 CPU 0，拿到 a 所在 Cache Line 最新值，assert 成功。

除了 Read Barrier 和 Write Barrier 外还有合二为一的 Barrier。作用是让后续写操作全部先去 Store Buffer 排队，让后续读操作都得先等 Invalidate Queue 处理完。

## 其它参考

-   [www.rdrop.com/users/paulm…](https://link.juejin.cn/?target=http%3A%2F%2Fwww.rdrop.com%2Fusers%2Fpaulmck%2Fscalability%2Fpaper%2Fwhymb.2010.07.23a.pdf "http://www.rdrop.com/users/paulmck/scalability/paper/whymb.2010.07.23a.pdf")
-   [Intel® 64 and IA-32 Architectures Software Developer Manuals | Intel® Software](https://link.juejin.cn/?target=https%3A%2F%2Fsoftware.intel.com%2Fen-us%2Farticles%2Fintel-sdm "https://software.intel.com/en-us/articles/intel-sdm")
-   [Memory Barriers Are Like Source Control Operations](https://link.juejin.cn/?target=https%3A%2F%2Fpreshing.com%2F20120710%2Fmemory-barriers-are-like-source-control-operations%2F "https://preshing.com/20120710/memory-barriers-are-like-source-control-operations/")
