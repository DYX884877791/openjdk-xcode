---
source: https://blog.csdn.net/fancyerii/article/details/5610360
---
以前我招聘过高级 java 工程师，其中一个面试题目是“你对 weak reference 了解多少？”。这个话题比较偏，不指望每个人都能清楚它的细节。如果面试的人说“ Umm... 好像和 gc （垃圾回收）有点关系？”，那我就相当满意了。实际情况却是 20 多个 5 年 java 开发经验的工程师只有 2 个知道有 weak reference 这么回事，其中 1 个是真正清楚的。我试图给他们一些提示，期望有人会恍然大悟，可惜没有。不知道为什么这个特性 uncommon ，确切地说，是相当 uncommon ，要知道这是在 java1.2 中推出的，那是 7 年前的事了。

没必要成为 weak reference 专家，装成资深 java 工程师（就像茴香豆的茴字有四种写法）。但是至少要了解一点点，知道是怎么回事。下面告诉你什么是 weak references ，怎么用及何时用它们。

l          Strong references  
       从强引用 (Strong references) 开始。你每天用的就是 strong reference ，比如下面的代码： `StringBuffer buffer = new StringBuffer()` `；` 创建了一个 StringBuffer 对象，变量 buffer 保存对它的引用。这太小儿科了！是的，请保持点耐心。 Strong reference ，是什么使它们‘ strong ’？——是 gc 处理它们的方式：如果一个对象通过一串强引用链可达，那么它们不会被垃圾回收。你总不会喜欢 gc 把你正在用的对象回收掉吧。

l          When strong references are too strong  
       我们有时候用到一些不能修改也不能扩展的类，比如 final class ，再比如，通过 Factory 创建的对象，只有接口，连是什么实现都不知道。想象一下，你正在用 widget 类，需要知道每个实例的扩展信息，比如它是第几个被创建的 widget 实例（即序列号），假设条件不允许在类中添加方法， widget 类自己也没有这样的序列号，你准备怎么办？用 HashMap ！ `serialNumberMap.put(widget, widgetSerialNumber)` ，用变量记录新实例的序列号，创建实例时把实例和它的序列号放到 HashMap 中。很显然，这个 Map 会不断变大，从而造成内存泄漏。你要说，不要紧，在不用某个实例时就从 map 中删除它。是的，这可行，但是“ put —— remove ”，你不觉得你在做与内存管理“ new —— delete ”类似的事吗？像所有自己管理内存的语言一样，你不能有遗漏。这不是 java 风格。       另 一个很普遍的问题是缓存，特别是很耗内存的那种，比如图片缓存。想象一下，有个项目要管理用户自己提供的图片，比如像我正在做的网站编辑器。自然地你会把 这些图片缓存起来，因为每次从磁盘读取会很耗时，而且可以避免在内存中一张图片出现多份。你应该能够很快地意识到这有内存危机：由于图片占用的内存没法被 回收，内存迟早要用完。把一部分图片从缓存中删除放到磁盘上去！——这涉及到什么时候删除、哪些图片要删除的问题。和 widget 类一样，不是吗，你在做内存管理的工作。

l          Weak reference    Weak reference ，简单地说就是这个引用不会强到迫使对象必须保持在内存中。 Gc 不会碰 Strong reference 可达的对象，但可以碰 weak reference 可达的对象。下面创建一个 weak reference ： `WeakReference weakWidget = new WeakReference(widget)` ，使用 `weakWidget.get()` 来取到 widget 对象。注意， get() 可能返回 null 。什么？ null ？什么时候变成 null 了？——当内存不足垃圾回收器把 widget 回收了时（如果是 Strong reference ，这是不可能发生的）。你会问，变成 null 之后要想再得到 widget 怎么办？答案是没有办法，你得重新创建 widget 对象，对 cache 系统这很容易做到，比如图片缓存，从磁盘载入图片即可（内存中的每份图片要在磁盘上保存一份）。       像上面的“ widget 序列号”问题，最简单的是用 jdk 内含的 WeakHashMap 类。 WeakHashMap 和 HashMap 的工作方式类似，不过它的 keys （注意不是 values ）都是 weak reference 。如果 WeakHashMap 中的一个 key 被垃圾回收了，那么这个 entry 会被自动删除。如果使用的是 Map 接口，那么实例化时只需把 HashMap 改成 WeakHashMap ，其它代码都不用变，就这么简单。

l          Reference queque    一旦 `WeakReference.get()` 返回 null ，它指向的对象被垃圾回收， WeakReference 对象就一点用都没有了，如果要对这些没有的 WeakReference 做些清理工作怎么办？比如在 WeakHashMap 中要把回收过的 key 从 Map 中删除掉。 jdk 中的 ReferenceQueue 类使你可以很容易地跟踪 dead references 。 WeakReference 类的构造函数有一个 ReferenceQueue 参数，当指向的对象被垃圾回收时，会把 WeakReference 对象放到 ReferenceQueue 中。这样，遍历 ReferenceQueue 可以得到所有回收过的 WeakReference 。 WeakHashMap 的做法是在每次调用 size() 、 get() 等操作时都先遍历 ReferenceQueue ，处理那些回收过的 key ，见 jdk 的源码 WeakHashMap# expungeStaleEntries() 。

l          Different degrees of weakness    上面我们仅仅提到“ weak reference ”，实际上根据弱的层度不同有四种引用：强（ strong ）、软（ soft ）、弱（ weak ）、虚（ phantom ）。我们已经讨论过 strong 和 weak ，下面看下 soft 和 phantom 。

n          Soft reference      Soft reference 和 weak reference 的区别是：一旦 gc 发现对象是 weak reference 可达就会把它放到 ReferenceQueue 中，然后等下次 gc 时回收它；当对象是 Soft reference 可达时， gc 可能会向操作系统申请更多内存，而不是直接回收它，当实在没辙了才回收它。像 cache 系统，最适合用 Soft reference 。

n          Phantom reference      虚引用 Phantom reference 与 Soft reference 和 WeakReference 的使用有很大的不同：它的 get() 方法总是返回 null （不信可以看 jdk 的 PhantomReference 源码）。这意味着你只能用 PhantomReference 本身，而得不到它指向的对象。它的唯一用处是你能够在 ReferenceQueue 中知道它被回收了。为何要有这种“不同”？       何时进入 ReferenceQueue 产生了这种“不同”。 WeakReference 是在它指向的对象变得弱可达 (weakly reachable ）时立即被放到 ReferenceQueue 中，这在 finalization 、 garbage collection 之前发生。理论上，你可以在 finalize() 方法中使对象“复活”（使一个强引用指向它就行了， gc 不会回收它），但 WeakReference 已经死了（死了？不太明白作者的确切意思。在 finalize 中复活对象不太能够说明问题。理论上你可以复活 ReferenceQueue 中的 WeakReference 指向的对象，但没法复活 PhantomReference 指向的对象，我想这才是它们的“不同”）。而 PhantomReference 不同，它是在 garbage collection 之后被放到 ReferenceQueue 中的，没法复活。       PhantomReferences 的价值在哪里？我只说两点： 1 、你能知道一个对象已经从内存中删除掉了，事实上，这是唯一的途径。这可能不是很有用，只能用在某些特别的场景中，比如维护巨大的图片：只有图片对象被回收之后才有必要再载入，这在很大程度上可以避免 OutOfMemoryError 。 2 、可以避免 finalize() 方法的缺点。在 finalize 方法中可以通过新建强引用来使对象复活。你可能要说，那又怎么样？—— finalize 的问题是对那些重载了 finalize 方法的对象垃圾回收器必须判断两遍才能决定回收它。第一遍，判断对象是否可达，如果不可达，看是否有 finalization ，如果有则调用，否则回收；第二遍判断对象是否可达，如果不可达，则回收。由于 finalize 是在内存回收之前调用的，那么在 finalize 中可能出现 OutOfMemoryError ，即使很多对象可以被回收。用 PhantomReference 就不会出现这种情况，当 PhantomReference 进入 ReferenceQueue 之后就没法再获得所指向的对象（它已经从内存中删除了）。由于 PhantomReference 不能使对象复活，所以它指向的对象可以在第一遍时回收，有 finalize 方法的对象就不行。可以证明， finalize 方法不是首选。 PhantomReference 更安全更有效，可以简化 VM 的工作。虽然好处多，但要写的代码也多。所以我坦白承认，大部分情况我还是用 finalize 。不管怎么样，你多了个选择，不用在 finalize 这棵树上吊死。

l          总结    我打赌有人在嘟囔，说我在讲老黄历，没什么鲜货。你说得没错，不过，以我的经验仍有很多 java 工程师对 weak reference 没甚了解，这样一堂入门课对他们很有必要。真心希望你能从这篇文章中得到一点收获。

原文地址：[http://weblogs.java.net/blog/enicholas/archive/2006/05/understanding_w.html](http://weblogs.java.net/blog/enicholas/archive/2006/05/understanding_w.html)

作者：[Ethan Nicholas's Blog](http://weblogs.java.net/blog/enicholas/)
