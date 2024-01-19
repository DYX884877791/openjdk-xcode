---
created: 2023-04-07T09:43:40 (UTC +08:00)
tags: [Java,缓存,Java 虚拟机（JVM）,Java 编程,多线程]
source: https://www.zhihu.com/question/296949412/answer/747494794
author: 罗一鑫追求真理，无视对错 关注
---

# (24 封私信 / 80 条消息) 既然CPU有缓存一致性协议（MESI），为什么JMM还需要volatile关键字？ - 知乎
---
通过提问的句式可以看出题主对于MESI和java的volatile的理解因果倒置。volatile是Java这种高级语言中的一个关键字，要实现这个volatile的功能，需要借助MESI！（**请注意，这里只是说Java的volatile，而不涵盖C和C++的volatile**）

CPU有缓存一致性协议：MESI，这不错。但MESI并非是无条件生效的！

不是说CPU支持MESI，那么你的变量就默认能做到缓存一致了。

根据MESI，CPU某核（假设CPU0）的缓存行（包含变量x）是M、S、或E的时候，如果总线嗅探到了变量x被其其他核（比如CPU1）执行了写操作（remote write）那么CPU0中的该缓存行会置为I（无效），在CPU0后续对该变量执行读操作的时候，发现是I状态，就会去主存中同步最新的值（其实由于L3缓存的存在，这里也可能是直接从L3同步到CPU0的L1和L2缓存，而不直接访问主存）

但实际可能不太理想，因为在CPU1执行写操作，要等到其他CPU（比如CPU0、CPU2……）将对应缓存行置为I状态，然后再将数据同步到主存，这个写操作才能完成。由于这样性能较差所以引入了Store Buffer，CPU1只需要将数据写入到Store Buffer，而不等待其他CPU把缓存行状态置为I，就开始忙别的去了。等到其他CPU通知CPU1我们都知道那个缓存失效啦，然后这个数据才同步到主存。

java虚拟机在实现volatile关键字的时候，是写入了一条lock 前缀的汇编指令。

**lock 前缀的汇编指令会强制写入主存，也可避免前后指令的CPU重排序，并及时让其他核中的相应缓存行失效，从而利用MESI达到符合预期的效果。**

非lock前缀的汇编指令在执行写操作的时候，可能是是不生效的。比如前面所说的Store Buffer的存在，lock前缀的指令在功能上可以等价于内存屏障，可以让其立即刷入主存。

再来介绍一下何谓lock前缀指令。lock指令在汇编中不是单独出现的，而是作为前缀来修饰其他指令的。可以和lock前缀修饰的指令有：

```
ADD, ADC, AND, BTC, BTR, BTS, CMPXCHG,DEC, INC, NEG, NOT, OR, SBB, SUB, XOR, XADD, XCHG
```

比如：

addl指令执行的时候会触发一个LOCK#信号，锁缓存(独占变量所在缓存行），指令执行完毕之后锁定解除。注意addl $0×0, (%esp)是一个空操作，不会有任何影响，这里只是为了配合触发lock的效果

是volatile的底层实现，满足了MESI的触发条件，才让变量有了缓存一致性。所以孰因孰果?

引申一下，如果是二十年前或更早，CPU不支持MESI的时候，要如何实现volatile呢？答案貌似是”锁总线“，不过这个代价比较大。
