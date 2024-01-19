---
source: http://ifeve.com/java-reference%E6%A0%B8%E5%BF%83%E5%8E%9F%E7%90%86%E5%88%86%E6%9E%90/
---
带着问题，看源码针对性会更强一点、印象会更深刻、并且效果也会更好。所以我先卖个关子，提两个问题(没准下次跳槽时就被问到)。

-   我们可以用ByteBuffer的allocateDirect方法，申请一块堆外内存创建一个DirectByteBuffer对象，然后利用它去操作堆外内存。这些申请完的堆外内存，我们可以回收吗？可以的话是通过什么样的机制回收的？
-   大家应该都知道WeakHashMap可以用来实现内存相对敏感的本地缓存，为什么WeakHashMap合适这种业务场景，其内部实现会做什么特殊处理呢？

## GC可到达性与JDK中Reference类型

上面提到的两个问题，其答案都在JDK的Reference里面。JDK早期版本中并没有Reference相关的类，这导致对象被GC回收后如果想做一些额外的清理工作(比如socket、堆外内存等)是无法实现的，同样如果想要根据堆内存的实际使用情况决定要不要去清理一些内存敏感的对象也是法实现的。为此JDK1.2中引入的Reference相关的类，即今天要介绍的Reference、SoftReference、WeakReference、PhantomReference，还有与之相关的Cleaner、ReferenceQueue、ReferenceHandler等。与Reference相关核心类基本都在java.lang.ref包下面。其类关系如下

-   ![](http://ifeve.com/wp-content/uploads/2020/01/p.png)
    

其中，SoftReference代表软引用对象，垃圾回收器会根据内存需求酌情回收软引用指向的对象。普通的GC并不会回收软引用，只有在即将OOM的时候(也就是最后一次Full GC)如果被引用的对象只有SoftReference指向的引用，才会回收。WeakReference代表弱引用对象，当发生GC时，如果被引用的对象只有WeakReference指向的引用，就会被回收。PhantomReference代表虚引用对象(也有叫幻象引用的，个人认为还是虚引用更加贴切)，其是一种特殊的引用类型，不能通过虚引用获取到其关联的对象，但当GC时如果其引用的对象被回收，这个事件程序可以感知，这样我们可以做相应的处理。最后就是最常见强引用对象，也就是通常我们new出来的对象。在继续介绍Reference相关类的源码前，先来简单的看一下GC如何决定一个对象是否可被回收。其基本思路是从GC Root开始向下搜索，如果对象与GC Root之间存在引用链，则对象是可达的，GC会根据是否可到达与可到达性决定对象是否可以被回收。而对象的可达性与引用类型密切相关，对象的可到达性可分为5种。

-   强可到达，如果从GC Root搜索后，发现对象与GC Root之间存在强引用链则为强可到达。强引用链即有强引用对象，引用了该对象。
-   软可到达，如果从GC Root搜索后，发现对象与GC Root之间不存在强引用链，但存在软引用链，则为软可到达。软引用链即有软引用对象，引用了该对象。
-   弱可到达，如果从GC Root搜索后，发现对象与GC Root之间不存在强引用链与软引用链，但有弱引用链，则为弱可到达。弱引用链即有弱引用对象，引用了该对象。
-   虚可到达，如果从GC Root搜索后，发现对象与GC Root之间只存在虚引用链则为虚可到达。虚引用链即有虚引用对象，引用了该对象。
-   不可达，如果从GC Root搜索后，找不到对象与GC Root之间的引用链，则为不可到达。

看一个简单的列子：

-   ![](http://ifeve.com/wp-content/uploads/2020/01/p-1024x463.jpeg)
    

ObjectA为强可到达，ObjectB也为强可到达，虽然ObjectB对象被SoftReference ObjcetE 引用但由于其还被ObjectA引用所以为强可到达;而ObjectC和ObjectD为弱引用达到，虽然ObjectD对象被PhantomReference ObjcetG引用但由于其还被ObjectC引用，而ObjectC又为弱引用达到，所以ObjectD为弱引用达到;而ObjectH与ObjectI是不可到达。引用链的强弱有关系依次是 强引用 > 软引用 > 弱引用 > 虚引用，如果有更强的引用关系存在，那么引用链到达性，将由更强的引用有关系决定。

JVM在GC时如果当前对象只被Reference对象引用，JVM会根据Reference具体类型与堆内存的使用情况决定是否把对应的Reference对象加入到一个由Reference构成的pending链表上，如果能加入pending链表JVM同时会通知ReferenceHandler线程进行处理。ReferenceHandler线程是在Reference类被初始化时调用的，其是一个守护进程并且拥有最高的优先级。Reference类静态初始化块代码如下:

```
static {
```

而ReferenceHandler线程内部的run方法会不断地从Reference构成的pending链表上获取Reference对象，如果能获取则根据Reference的具体类型进行不同的处理，不能则调用wait方法等待GC回收对象处理pending链表的通知。ReferenceHandler线程run方法源码:

```
public void run() {
```

run内部调用的tryHandlePending源码:

```
static boolean tryHandlePending(boolean waitForNotify) {
```

上面tryHandlePending方法中比较重要的点是c.clean()与q.enqueue(r)，这个是文章最开始提到的两个问题答案的入口。Cleaner的clean方法用于完成清理工作，而ReferenceQueue是将被回收对象加入到对应的Reference列队中，等待其他线程的后继处理。更具体地关于Cleaner与ReferenceQueue后面会再详细说明。Reference的核心处理流程可总结如下：

-   ![](http://ifeve.com/wp-content/uploads/2020/01/p-2.jpeg)
    

对Reference的核心处理流程有整体了解后，再来回过头细看一下Reference类的源码。

```
/* Reference实例有四种内部的状态
```

上面解释了Reference中的主要成员的作用，其中比较重要是Reference内部维护的不同状态，其状态不同成员变量queue、pending、discovered、next的取值都会发生变化。Reference的主要方法如下:

```
//构造函数，指定引用的对象referent
```

## ReferenecQueue与Cleaner源码分析

先来看下ReferenceQueue的主要成员变量的含义。

```
//代表Reference的queue为null。Null为ReferenceQueue子类
```

ReferenceQueue中比较重要的方法为enqueue、poll、remove方法。

```
//入列队enqueue方法，只被Reference类调用，也就是上面分析中ReferenceHandler线程为调用
```

poll方法源码相对简单，其就是从ReferenceQueue的头节点获取Reference。

```
public Reference<? extends T> poll() {
```

remove方法的源码如下：

```
public Reference<? extends T> remove(long timeout) throws IllegalArgumentException, InterruptedException {
```

简单的分析完ReferenceQueue的源码后，再来整体回顾一下Reference的核心处理流程。JVM在GC时如果当前对象只被Reference对象引用，JVM会根据Reference具体类型与堆内存的使用情况决定是否把对应的Reference对象加入到一个由Reference构成的pending链表上，如果能加入pending链表JVM同时会通知ReferenceHandler线程进行处理。ReferenceHandler线程收到通知后会调用Cleaner#clean或ReferenceQueue#enqueue方法进行处理。如果引用当前对象的Reference类型为WeakReference且堆内存不足，那么JMV就会把WeakReference加入到pending-Reference链表上，然后ReferenceHandler线程收到通知后会异步地做入队列操作。而我们的应用程序中的线程便可以不断地去拉取ReferenceQueue中的元素来感知JMV的堆内存是否出现了不足的情况，最终达到根据堆内存的情况来做一些处理的操作。实际上WeakHashMap低层便是过通上述过程实现的，只不过实现细节上有所偏差，这个后面再分析。再来看看ReferenceHandler线程收到通知后可能会调用的另外一个类Cleaner的实现。

同样先看一下Cleaner的成员变量，再看主要的方法实现。

```
//继承了PhantomReference类也就是虚引用，PhantomReference源码很简单只是重写了get方法返回null
```

从上面的成变量分析知道Cleaner实现了双向链表的结构。先看构造函数与clean方法。

```
//私有方法，不能直接new
```

可以看到Cleaner的实现还是比较简单，Cleaner实现为PhantomReference类型的引用。当JVM GC时如果发现当前处理的对象只被PhantomReference类型对象引用，同之前说的一样其会将该Reference加pending-Reference链中上，只是ReferenceHandler线程在处理时如果PhantomReference类型实际类型又是Cleaner的话。其就是调用Cleaner.clean方法做清理逻辑处理。Cleaner实际是DirectByteBuffer分配的堆外内存收回的实现，具体见下面的分析。

## DirectByteBuffer堆外内存回收与WeakHashMap敏感内存回收

绕开了一大圈终于回到了文章最开始提到的两个问题，先来看一下分配给DirectByteBuffer堆外内存是如何回收的。在创建DirectByteBuffer时我们实际是调用ByteBuffer#allocateDirect方法，而其实现如下：

```
//直接new一个指定字节大小的DirectByteBuffer对象
```

里面和DirectByteBuffer堆外内存回收相关的代码便是Cleaner.create(this, new Deallocator(base, size, cap))这部分。还记得之前说实际的清理逻辑是里面和DirectByteBuffer堆外内存回收相关的代码便是Cleaner里面的Runnable#run方法吗？直接看Deallocator.run方法源码：

```
public void run() {
```

终于找到了分配给DirectByteBuffer堆外内存是如何回收的的答案。再总结一下，创建DirectByteBuffer对象时会创建一个Cleaner对象，Cleaner对象持有了DirectByteBuffer对象的引用。当JVM在GC时，如果发现DirectByteBuffer被地方法没被引用啦，JVM会将其对应的Cleaner加入到pending-reference链表中，同时通知ReferenceHandler线程处理，ReferenceHandler收到通知后，会调用Cleaner#clean方法，而对于DirectByteBuffer创建的Cleaner对象其clean方法内部会调用unsafe.freeMemory释放堆外内存。最终达到了DirectByteBuffer对象被GC回收其对应的堆外内存也被回收的目的。

再来看一下文章开始提到的另外一个问题WeakHashMap如何实现敏感内存的回收。实际WeakHashMap实现上其Entry继承了WeakReference。

```
//Entry继承了WeakReference, WeakReference引用的是Map的key
```

往WeakHashMap添加元素时，实际都会调用Entry的构造方法，也就是会创建一个WeakReference对象，这个对象的引用的是WeakHashMap刚加入的Key,而所有的WeakReference对象关联在同一个ReferenceQueue上。我们上面说过JVM在GC时，如果发现当前对象只有被WeakReference对象引用，那么会把其对应的WeakReference对象加入到pending-reference链表上，并通知ReferenceHandler线程处理。而ReferenceHandler线程收到通知后，对于WeakReference对象会调用ReferenceQueue#enqueue方法把他加入队列里面。现在我们只要关注queue里面的元素在WeakHashMap里面是在哪里被拿出去啦做了什么样的操作，就能找到文章开始问题的答案啦。最终能定位到WeakHashMap的expungeStaleEntries方法。

```
private void expungeStaleEntries() {
```

现在只看一下WeakHashMap哪些地方会调用expungeStaleEntries方法就知道什么时候WeakHashMap里面的Key变得软可达时我们就可以将其对应的Entry从WeakHashMap里面移除。直接调用有三个地方分别是getTable方法、size方法、resize方法。 getTable方法又被很多地方调用如get、containsKey、put、remove、containsValue、replaceAll。最终看下来，只要对WeakHashMap进行操作就行调用expungeStaleEntries方法。所有只要操作了WeakHashMap，没WeakHashMap里面被再用到的Key对应的Entry就会被清除。再来总结一下，为什么WeakHashMap适合作为内存敏感缓存的实现。当JVM 在GC时，如果发现WeakHashMap里面某些Key没地方在被引用啦(WeakReference除外)，JVM会将其对应的WeakReference对象加入到pending-reference链表上，并通知ReferenceHandler线程处理。而ReferenceHandler线程收到通知后将对应引用Key的WeakReference对象加入到 WeakHashMap内部的ReferenceQueue中，下次再对WeakHashMap做操作时，WeakHashMap内部会清除那些没有被引用的Key对应的Entry。这样就达到了每操作WeakHashMap时，自动的检索并清量没有被引用的Key对应的Entry的目地。

## 总结

本文通过两个问题引出了JDK中Reference相关类的源码分析，最终给出了问题的答案。但实际上一般开发规范中都会建议禁止重写Object#finalize方法同样与Reference类关系密切(具体而言是Finalizer类)。受篇幅的限制本文并未给出分析，有待各位自己看源码啦。半年没有写文章啦，有点对不住关注的小伙伴。希望看完本文各位或多或少能有所收获。如果觉得本文不错就帮忙转发记得标一下出处，谢谢。后面我还会继续分享一些自己觉得比较重要的东西给大家。由于个人能力有限，文中不足与错误还望指正。最后愿大家国庆快乐。

## 参考

http://lovestblog.cn/blog/2015/05/12/direct-buffer/ https://coldwalker.com/2019/02//gc_intro/ http://imushan.com/2018/08/19/java/language/JDK%E6%BA%90%E7%A0%81%E9%98%85%E8%AF%BB-Reference/ http://blog.2baxb.me/archives/974
