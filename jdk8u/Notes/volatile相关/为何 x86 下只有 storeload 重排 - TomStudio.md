---
source: https://www.cnblogs.com/sunddenly/articles/14829087.html
---
① 如果，有invalidatequeue 组件的话比较明显的是会出现loadload重排序重排序，因为后面的load可能拿到的坑是旧值，断言失败]([https://zhuanlan.zhihu.com/p/125549632](https://zhuanlan.zhihu.com/p/125549632))  
② 如果，有invalidatequeue 组件的话，在NUCA下，[因为cpu可能会划分为多个cache组，我认为会出现 load store 重排序](https://zhuanlan.zhihu.com/p/71589870)，另一组cpu会认为 c = 1和while发生了重排  
③ 在 x86下，基于 FIFO 的storebuffer 可以保证storestore的顺序性

首先x86无 invalidatequeue， 可以保证无 loadload 和 load store排序，基于 FIFO 的storebuffer，且x86 的架构，强制 CPU 必须先写入 StoreBuffer，再写入 cache 可以保证storestore的顺序性  
[https://xie.infoq.cn/article/680fd531df57856ddcb532914](https://xie.infoq.cn/article/680fd531df57856ddcb532914)

CPU在cache line状态的转化期间是阻塞的，经过长时间的优化，在寄存器和L1缓存之间添加了LoadBuffer、StoreBuffer来降低阻塞时间，LoadBuffer、StoreBuffer，合称排序缓冲(Memoryordering Buffers (MOB))，Load缓冲64长度，store缓冲36长度，Buffer与L1进行数据传输时，CPU无须等待。

CPU执行load读数据时，把读请求放到LoadBuffer，这样就不用等待其它CPU响应，先进行下面操作，稍后再处理这个读请求的结果。  
CPU执行store写数据时，把数据写到StoreBuffer中，待到某个适合的时间点，把StoreBuffer的数据刷到主存中。  
[http://web.cecs.pdx.edu/~alaa/ece587/notes/memory-ordering.pdf](http://web.cecs.pdx.edu/~alaa/ece587/notes/memory-ordering.pdf)  
[http://yizhanggou.top/volatile/](http://yizhanggou.top/volatile/)  
[https://github.com/luohaha/MyBlog/issues/4](https://github.com/luohaha/MyBlog/issues/4)  
[https://stackoverflow.com/questions/11105827/what-is-a-store-buffer](https://stackoverflow.com/questions/11105827/what-is-a-store-buffer)

**================ 在intel手册中关于重排序的描述 =========================**

[从Intel手册里能看到](https://www.intel.co.kr/content/www/kr/ko/architecture-and-technology/64-ia-32-architectures-software-developer-vol-3a-part-1-manual.html):

1、Reads are not reordered with other reads.  
不需要特殊fence指令就能保证LoadLoad  
2.Writes are not reordered with older reads.  
不需要特殊fence指令就能保证LoadStore  
3.Writes to memory are not reordered with other writes.  
不需要特殊fence指令就能保证StoreStore  
4.Reads may be reordered with older writes to different locations but not with older writes to the same location.  
需要特殊fence指令才能保证StoreLoad, 有名的Peterson algorithm算法就是需要StoreLoad的典型场景

需要注意的是，前三点并没有强调 `different locations` or `same location` 情况，最后一点却强调了，read-read 和write-write 之间，不管是否同地址不能重排，且前面的read不能和write重排  
[https://www.intel.co.kr/content/www/kr/ko/architecture-and-technology/64-ia-32-architectures-software-developer-vol-3a-part-1-manual.html](https://www.intel.co.kr/content/www/kr/ko/architecture-and-technology/64-ia-32-architectures-software-developer-vol-3a-part-1-manual.html)

**================ 在某博客中关于重排序的描述 =========================**

为什么这一堆 Barrier 里 StoreLoad 最重？  
所谓的重实际就是跟内存交互次数，交互越多延迟越大，也就是越重。StoreStore， LoadLoad 两个都不提了，因为它俩要么只限制读，要么只限制写，也即只有一次内存交互。只有 LoadStore 和 StoreLoad 看上去有可能对读写都有限制。但 LoadStore 里实际限制的更多的是读，即 Load 数据进来，它并不对最后的 Store 存出去数据的可见性有要求，只是说 Store 不能重排到 Load 之前。而反观 StoreLoad，它是说不能让 Load 重排到 Store 之前，这么一来得要求在 Load 操作前刷写 Store Buffer 到内存。不去刷 Store Buffer 的话，就可能导致先执行了读取操作，之后再刷 Store Buffer 导致写操作实际被重排到了读之后。而数据一旦刷写出去，别的 CPU 就能看到，看到之后可能就会修改下一步 Load 操作的内存导致 Load 操作的内存所在 Cache Line 无效。如果允许 Load 操作从一个可能被 Invalidate 的 Cache Line 里读数据，则表示 Load 从实际意义上来说被重排到了 Store 之前，因为这个数据可能是 Store 前就在 Cache 中的，相当于读操作提前了。为了避免这种事发生，Store 完成后一定要去处理 Invalidate Queue，去判断自己 Load 操作的内存所在 Cache Line 是否被设置为无效。这么一来为了满足 StoreLoad 的要求，一方面要刷 Store Buffer，一方面要处理 Invalidate Queue，则最差情况下会有两次内存操作，读写分别一次，所以它最重。  
StoreLoad 为什么能实现其它 Barrier 的功能？  
这个也是从前一个问题结果能看出来的。StoreLoad 因为对读写操作均有要求，所以它能实现其它 Barrier 的功能。其它 Barrier 都是只对读写之中的一个方面有要求。  
不过这四个 Barrier 只是 Java 为了跨平台而设计出来的，实际上根据 CPU 的不同，对应 CPU 平台上的 JVM 可能可以优化掉一些 Barrier。比如很多 CPU 在读写同一个变量的时候能保证它连续操作的顺序性，那就不用加 Barrier 了。比如 Load x; Load x.field 读 x 再读 x 下面某个 field，如果访问同一个内存 CPU 能保证顺序性，两次读取之间的 Barrier 就不再需要了，根据字节码编译得到的汇编指令中，本来应该插入 Barrier 的地方会被替换为 nop，即空操作。在 x86 上，实际只有 StoreLoad 这一个 Barrier 是有效的，x86 上没有 Invalidate Queue，每次 Store 数据又都会去 Store Buffer 排队，所以 StoreStore， LoadLoad 都不需要。x86 又能保证 Store 操作都会走 Store Buffer 异步刷写，Store 不会被重排到 Load 之前，LoadStore 也是不需要的。只剩下一个 StoreLoad Barrier 在 x86 平台的 JVM 上被使用。  
[https://juejin.cn/post/6844904144273145863](https://juejin.cn/post/6844904144273145863)

由于 x86 是遵循 TSO 的最终一致性模型，如若出现 data race 的情况还是需要考虑同步的问题，尤其是在 StoreLoad 的场景。而其余场景由于其 store buffer 的特殊性以及不存在 invalidate queue 的因素，可以不需要考虑重排序的问题，因此在 x86 平台下，除了 StoreLoad Barrier 以外，其余的 Barrier 均为空操作。

[https://www.zhihu.com/question/274310265/answer/1271612645](https://www.zhihu.com/question/274310265/answer/1271612645)  
[https://zhuanlan.zhihu.com/p/81555436](https://zhuanlan.zhihu.com/p/81555436)

**================ 在jdk官方文档的描述 =========================**

Total Store Order (TSO) machines can be seen as machines issuing a release store for each store and a load acquire for each load.

Therefore there is an inherent resemblence between TSO and acquire/release semantics.

TSO can be seen as an abstract machine where loads are executed immediately when encountered (hence loadload reordering not happening) but enqueues stores in a FIFO queue for asynchronous serialization (neither storestore or loadstore reordering happening).

The only reordering happening is storeload due to the queue asynchronously serializing stores (yet in order).  
[http://hg.openjdk.java.net/jdk10/jdk10/hotspot/file/5ab7a67bc155/src/share/vm/runtime/orderAccess.hpp](http://hg.openjdk.java.net/jdk10/jdk10/hotspot/file/5ab7a67bc155/src/share/vm/runtime/orderAccess.hpp)

**================ FQA =========================**  
因此，MESI协议最多只是保证了对于一个变量，在多个核上的读写顺序，对于多个变量而言是没有任何保证的。很遗憾，还是需要volatile～～

答案仍然是需要的。因为 MESI只是保证了多核cpu的独占cache之间的一致性，但是cpu的并不是直接把数据写入L1 cache的，中间还可能有store buffer。有些arm和power架构的cpu还可能有load buffer或者invalid queue等等。因此，有MESI协议远远不够。  
[https://blog.csdn.net/org_hjh/article/details/109626607](https://blog.csdn.net/org_hjh/article/details/109626607)
