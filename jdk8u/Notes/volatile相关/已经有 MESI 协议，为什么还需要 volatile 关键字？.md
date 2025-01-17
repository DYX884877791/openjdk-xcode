---
created: 2023-04-07T09:43:24 (UTC +08:00)
tags: [volatile,计算机组织与体系结构（书籍）,计算机组成原理（书籍）]
source: https://zhuanlan.zhihu.com/p/584787272
author: 
---

# 已经有 MESI 协议，为什么还需要 volatile 关键字？ - 知乎
---
大家好，我是小彭。

**[在上一篇文章里](https://link.zhihu.com/?target=https%3A//mp.weixin.qq.com/s/7WNCVvm7cGU9Fy9S3f1IFQ)**，我们聊到了 CPU 的缓存一致性问题，分为纵向的 Cache 与内存的一致性问题以及横向的多个核心 Cache 的一致性问题。我们也讨论了 MESI 协议通过写传播和事务串行化实现缓存一致性。

不知道你是不是跟我一样，在学习 MESI 协议的时候，自然地产生了一个疑问：在不考虑写缓冲区和失效队列的影响下，在硬件层面已经实现了缓存一致性，那么在 Java 语言层面为什么还需要定义 `volatile` 关键字呢？是多此一举吗？今天我们将围绕这些问题展开。

> **本文已收录到 [GitHub · AndroidFamily](https://link.zhihu.com/?target=https%3A//github.com/pengxurui/AndroidFamily)，有 Android 进阶知识体系，欢迎 Star。技术和职场问题，请关注公众号 \[彭旭锐\] 进 Android 面试交流群。**

___

**学习路线图：**

![](https://pic2.zhimg.com/v2-4a4da1f23b517ea96ceea81952f46325_b.jpg)

___

由于 CPU 和内存的速度差距太大，为了拉平两者的速度差，现代计算机会在两者之间插入一块速度比内存更快的高速缓存，CPU 缓存是分级的，有 L1 / L2 / L3 三级缓存。其中 L1 / L2 缓存是核心独占的，而 L3 缓存是多核心共享的。

在 CPU Cache 的三级缓存中，会存在 2 个缓存一致性问题：

-   纵向 - Cache 与内存的一致性问题：通过写直达或写回策略解决；
-   横向 - 多核心 Cache 的一致性问题：通过 MESI 等缓存一致性协议解决。

MESI 协议能够满足写传播和事务串行化 2 点特性，通过 “已修改、独占、共享、已失效” 4 个状态实现了 CPU Cache 的一致性；

现代 CPU 为了提高并行度，会在增加写缓冲区 & 失效队列将 MESI 协议的请求异步化，这其实是一种处理器级别的指令重排，会破坏了 CPU Cache 的一致性。

`Cache 不一致问题`

![](https://pic1.zhimg.com/v2-2524d1eb61aad58d9bab812a9c9a5790_b.jpg)

`MESI 协议在线模拟`

![](https://pic4.zhimg.com/v2-6436236894afd9158e5c1b212a7dd453_b.jpg)

网站地址：[https://www.scss.tcd.ie/Jeremy.Jones/VivioJS/caches/MESI.htm](https://link.zhihu.com/?target=https%3A//www.scss.tcd.ie/Jeremy.Jones/VivioJS/caches/MESI.htm)

现在，我们的问题是：既然 CPU 已经实现了 MESI 协议，为什么 Java 语言层面还需要定义 `volatile` 关键字呢？岂不是多此一举？你可能会说因为写缓冲区和失效队列破坏了 Cache 一致性。好，那不考虑这个因素的话，还需要定义 `volatile` 关键字吗？

**其实，MESI 解决数据一致性（Data Conherence）问题，而 volatile 解决顺序一致性（Sequential Consistency）问题。** WC，这两个不一样吗？

![动图封面](https://pic4.zhimg.com/v2-da7e5f7a10d502d2844e665bed967c0f_b.jpg)

___

## **2\. 数据一致性 vs 顺序一致性**

### **2.1 数据一致性**

**数据一致性讨论的是同一份数据在多个副本之间的一致性问题，** 你也可以理解为多个副本的状态一致性问题。例如内存与多核心 Cache 副本之间的一致性，或者数据在主从数据库之间的一致性。

当我们从 CPU 缓存一致性问题开始，逐渐讨论到 Cache 到内存的写直达和写回策略，再讨论到 MESI 等缓存一致性协议，从始至终我们讨论的都是 CPU 缓存的 “数据一致性” 问题，只是为了简便我们从没有刻意强调 “数据” 的概念。

数据一致性有强弱之分：

-   **强数据一致性：** 保证在任意时刻任意副本上的同一份数据都是相同的，或者允许不同，但是每次使用前都要刷新确保数据一致，所以最终还是一致。
-   **弱数据一致性：** 不保证在任意时刻任意副本上的同一份数据都是相同的，也不要求使用前刷新，但是随着时间的迁移，不同副本上的同一份数据总是向趋同的方向变化，最终还是趋向一致。

例如，MESI 协议就是强数据一致性的，但引入写缓冲区或失效队列后就变成弱数据一致性，随着缓冲区和失效队列被消费，各个核心 Cache 最终还是会趋向一致状态。

![](https://pic1.zhimg.com/v2-cdeb3901f965e53c8ff186ca7963a56c_b.jpg)

### **2.2 顺序一致性**

顺序一致性讨论的是对多个数据的多次操作顺序在整个系统上的一致性。在并发编程中，存在 3 种指令顺序：

-   **编码顺序（Progrom Order）：** 指源码中指令的编写顺序，是程序员视角看到的指令顺序，不一定是实际执行的顺序；
-   **执行顺序（Memory Order）：** 指单个线程或处理器上实际执行的指令顺序；
-   **全局执行顺序（Global Memory Order）：** 每个线程或处理器上看到的系统整体的指令顺序，在弱顺序一致性模型下，每个线程看到的全局执行顺序可能是不同的。

顺序一致性模型是计算机科学家提出的一种理想参考模型，为程序员描述了一个极强的全局执行顺序一致性，由 2 个特性组成：

-   **特性 1 - 执行顺序与编码顺序一致：** 保证每个线程中指令的执行顺序与编码顺序一致；
-   **特性 2 - 全局执行顺序一致：** 保证每个指令的结果会同步到主内存和各个线程的工作内存上，使得每个线程上看到的全局执行顺序一致。

举个例子，线程 A 和线程 B 并发执行，线程 A 执行 A1 → A2 → A3，线程 B 执行 B1 → B2 → B3。那么，在顺序一致性内存模型下，虽然程序整体执行顺序是不确定的，但是线程 A 和线程 B 总会按照 1 → 2 → 3 编码顺序执行，而且两个线程总能看到相同的全局执行顺序。

`顺序一致性内存模型`

![](https://pic4.zhimg.com/v2-ad010d1699466433a2e000d612992973_b.jpg)

### **2.3 弱顺序一致性（一定要理解）**

虽然顺序一致性模型对程序员非常友好，但是对编译器和处理器却不见得喜闻乐见。如果程序完全按照顺序一致性模型来实现，那么处理器和编译器的很多重排序优化都要被禁止，这对程序的 **“并行度”** 会有影响。例如：

-   **1、重排序问题：** 编译器和处理器不能重排列没有依赖关系的指令；
-   **2、内存可见性问题：** CPU 不能使用写回策略，也不能使用写缓冲区和失效队列机制。其实，从内存的视角看也是指令重排问题。

所以，在 Java 虚拟机和处理器实现中，实际上使用的是弱顺序一致性模型：

-   **特性 1 - 不要求执行顺序与编码顺序一致：** 不要求单线程的执行顺序与编码顺序一致，只要求执行结果与强顺序执行的结果一致，而指令是否真的按编码顺序执行并不关心。因为结果不变，从程序员的视角看程序就是按编码顺序执行的假象；
-   **特性 2 - 不要求全局执行顺序一致：** 允许每个线程看到的全局执行顺序不一致，甚至允许看不到其他线程已执行指令的结果。

**举个单线程的例子：** 在这段计算圆面积的代码中，在弱顺序一致性模型下，指令 A 和 指令 B 可以不按编码顺序执行。因为 A 和 B 没有数据依赖，所以对最终的结果也没有影响。但是 C 对 A 和 B 都有数据依赖，所以 C 不能重排列到 A 或 B 的前面，否则会改变程序结果。

`伪代码`

```
double pi = 3.14; // A
double r = 1.0； // B
double area = pi * r * r; // C（数据依赖于 A 和 B，不能重排列到前面执行）
```

`指令重排`

![](https://pic3.zhimg.com/v2-53842d983d01b16b6805ed28c94600ce_b.jpg)

**再举个多线程的例子：** 我们在 ChangeThread 线程修改变量，在主线程观察变量的值。在弱顺序一致性模型下，允许 ChangeThread 线程 A 指令的执行结果不及时同步到主线程，在主线程看来就像没执行过 A 指令。

**这个问题我们一般会理解为内存可见性问题，其实我们可以统一理解为顺序一致性问题。** 主线程看不到 ChangeThread 线程 A 指令的执行结果，就好像两个线程看到的全局执行顺序不一致：ChangeThread 线程看到的全局执行顺序是：\[B\]，而主线程看到的全局执行顺序是 \[\]。

`可见性示例程序`

```
public class VisibilityTest {
    public static void main(String[] args) {
        ChangeThread thread = new ChangeThread();
        thread.start();
        while (true) {
            if (thread.flag) { // B
                System.out.println("Finished");
                return;
            }
        }
    }

    public static class ChangeThread extends Thread {
        private boolean flag = false;

        @Override
        public void run() {
            try {
                Thread.sleep(1000);
            } catch (InterruptedException e) {
                e.printStackTrace();
            }
            flag = true; // A
            System.out.println("Change flag = " + flag);
        }
    }
}
```

`程序输出`

```
Change flag = true
// 无限等待
```

前面你说到编译器和处理器的重排序，为什么指令可以重排序，为什么重排序可以提升性能，重排序不会出错吗？

___

## **3\. 什么是指令重排序？**

### **3.1 重排序类型**

从源码到指令执行一共有 3 种级别重排序：

-   **1、编译器重排序：** 例如将循环内重复调用的操作提前到循环外执行；
-   **2、处理器系统重排序：** 例如指令并行技术将多条指令重叠执行，或者使用分支预测技术提前执行分支的指令，并把计算结果放到重排列缓冲区（Reorder Buffer）的硬件缓存中，当程序真的进入分支后直接使用缓存中的结算结果；
-   **3、存储器系统重排序：** 例如写缓冲区和失效队列机制，即是可见性问题，从内存的角度也是指令重排问题。

`指令重排序类型`

![](https://pic2.zhimg.com/v2-65df1e30cd72a237446c60f203bdbd6d_b.png)

### **3.2 什么是数据依赖性？**

编译器和处理器在重排序时，会遵循数据依赖性原则，不会试图改变存在数据依赖关系的指令顺序。如果两个操作都是访问同一个数据，并且其中一个是写操作，那么这两个操作就存在数据依赖性。此时一旦改变顺序，程序最终的执行结果一定会发生改变。

数据依赖性分为 3 种类型：：

| 数据依赖性 | 描述 | 示例 |
| --- | --- | --- |
| 写后读 | 写一个数据，再读这个数据 | a = 1; // 写  
b = a; // 读 |
| 写后写 | 写一个数据，再写这个数据 | a = 1; // 写  
a = 2; // 写 |
| 读后写 | 读一个数据，再写这个数据 | b = a; // 读  
a = 1; // 写 |

### **3.3 指令重排序安全吗？**

需要注意的是：数据依赖性原则只对单个处理器或单个线程有效，因此即使在单个线程或处理器上遵循数据依赖性原则，在多处理器或者多线程中依然有可能改变程序的执行结果。

举例说明吧。

**例子 1 - 写缓冲区和失效队列的重排序：** 如果是在一个处理器上执行 “写后读”，处理器不会重排这两个操作的顺序；但如果是在一个处理器上写，之后在另一个处理器上读，就有可能重排序。关于写缓冲区和失效队列引起的重排序问题，上一篇文章已经解释过，不再重复。

`写缓冲区造成指令重排`

![](https://pic1.zhimg.com/v2-974dc84b8b8d2fc3dd03eb67f77468d8_b.jpg)

**例子 2 - 未同步的多线程程序中的指令重排：** 在未同步的两个线程 A 和 线程 B 上分别执行这两段程序，程序的预期结果应该是 `4`，但实际的结果可能是 `0`。

`线程 A`

```
a = 2; // A1
flag = true; // A2
```

`线程 B`

```
while (flag) { // B1
    return a * a; // B2
}
```

情况 1：由于 A1 和 A2 没有数据依赖性，所以编译器或处理器可能会重排序 A1 和 A2 的顺序。在 A2 将 `flag` 改为 `true` 后，B1 读取到 `flag` 条件为真，并且进入分支计算 B2 结果，但 A1 还未写入，计算结果是 `0`。此时，程序的运行结果就被重排列破坏了。

情况 2：另一种可能，由于 B1 和 B2 没有数据依赖性，CPU 可能用分支预测技术提前执行 B2，但 A1 还未写入，计算结果还是 `0`。此时，程序的运行结果就被重排列破坏了。

`多线程的数据依赖性不被考虑`

![](https://pic3.zhimg.com/v2-9d88f8a51fc5ef70187c012b140edcea_b.jpg)

**小结一下：** 重排序在单线程程序下是安全的（与预期一致），但在多线程程序下是不安全的。

___

## **4\. 回答最初的问题**

到这里，虽然我们的讨论还未结束，但已经足够回答标题的问题：“已经有 MESI 协议，为什么还需要 volatile 关键字？”

即使不考虑写缓冲区或失效队列，MESI 也只是解决数据一致性问题，并不能解决顺序一致性问题。在实际的计算机系统中，为了提高程序的性能，Java 虚拟机和处理器会使用弱顺序一致性模型。

在单线程程序下，弱顺序一致性与强顺序一致性的执行结果完全相同。但在多线程程序下，重排序问题和可见性问题会导致各个线程看到的全局执行顺序不一致，使得程序的执行结果与预期不一致。

为了纠正弱顺序一致性的影响，编译器和处理器都提供了 **“内存屏障指令”** 来保证程序关键节点的执行顺序能够与程序员的预期一致。在高级语言中，我们不会直接使用内存屏障，而是使用更高级的语法，即 synchronized、volatile、final、CAS 等语法。

那么，什么是内存屏障？synchronized、volatile、final、CAS 等语法和内存屏障有什么关联，这个问题我们在下一篇文章展开讨论，请关注。

___

**参考资料**

-   **[Java 并发编程的艺术（第 1、2、3 章）](https://link.zhihu.com/?target=https%3A//weread.qq.com/web/bookDetail/247324e05a66a124750d9e9)** —— 方腾飞 魏鹏 程晓明 著
-   **[深入理解 Android：Java 虚拟机 ART（第 12.4 节）](https://link.zhihu.com/?target=https%3A//weread.qq.com/web/reader/3ee32e60717f5af83ee7b37k38d326a02f638db3aed9c35)** —— 邓凡平 著
-   **[深入理解 Java 虚拟机（第 5 部分）](https://link.zhihu.com/?target=https%3A//weread.qq.com/web/bookDetail/9b832f305933f09b86bd2a9)** —— 周志明 著
-   **[深入浅出计算机组成原理（第 55 讲）](https://link.zhihu.com/?target=https%3A//time.geekbang.org/column/intro/100026001)** —— 徐文浩 著，极客时间 出品
-   **[CPU有缓存一致性协议（MESI），为何还需要 volatile](https://link.zhihu.com/?target=https%3A//juejin.cn/post/6893792938824990734)** —— 一角钱技术 著
-   **[一文读懂 Java 内存模型（JMM）及 volatile 关键字](https://link.zhihu.com/?target=https%3A//juejin.cn/post/6893430262084927496)** —— 一角钱技术 著
-   **[MESI protocol](https://link.zhihu.com/?target=https%3A//en.wikipedia.org/wiki/MESI_protocol)** —— Wikipedia
-   **[Cache coherence](https://link.zhihu.com/?target=https%3A//en.wikipedia.org/wiki/Cache_coherence)** —— Wikipedia
-   **[Sequential consistency](https://link.zhihu.com/?target=https%3A//en.wikipedia.org/wiki/Sequential_consistency)** —— Wikipedia
-   **[Out-of-order execution](https://link.zhihu.com/?target=https%3A//en.wikipedia.org/wiki/Out-of-order_execution)** —— Wikipedia
-   **[std::memory\_order](https://link.zhihu.com/?target=https%3A//en.cppreference.com/w/cpp/atomic/memory_order)** —— [http://cppreference.com](https://link.zhihu.com/?target=http%3A//cppreference.com)

![](https://pic2.zhimg.com/v2-91d9a12409b408aa57f90f6ce1b50b59_b.jpg)
