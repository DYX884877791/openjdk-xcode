---
source: https://blog.csdn.net/u010412719/article/details/52035792?utm_medium=distribute.pc_relevant.none-task-blog-2~default~baidujs_utm_term~default-1-52035792-blog-121067380.235^v38^pc_relevant_sort_base2&spm=1001.2101.3001.4242.2&utm_relevant_index=4
---
## 《Java源码分析》：ReferenceQueue、Reference及其子类

在看完[WeakHashMap](https://so.csdn.net/so/search?q=WeakHashMap&spm=1001.2101.3001.7020)源码之后，看了有关于讲解WeakHashMap的些许博客，发现了几个比较有意思的类：Reference、Reference子类(SoftReference、WeakReference、PhantomReference)以及ReferenceQueue。

以前自己只是知道这些类的存在，在看WeakHashMap源码之前并不知道他们的用途，因此，借此机会自己就想对这几个类了解下，查了网上相关资料、看了源码和相关API文档，都没能完全的理解这些类以及这些类和垃圾回收之间的交互，只是有一个小小的认识。下面就来一一进行说明，若有错误，请指出并谅解。

## 1、ReferenceQueue类

由于Reference类中有关于ReferenceQueue的引用，因此，先对ReferenceQueue进行介绍。

源码中对该类的说明摘入如下：

Reference queues, to which registered reference objects are appended by the  
garbage collector after the appropriate reachability changes are detected.

中文意思为：Reference queues,在适当的时候检测到对象的可达性发生改变后，垃圾回收器就将已注册的引用对象添加到此队列中。

此类中的方法比较少，只有：enqueue(Reference

```
    //出队标识
    static ReferenceQueue<Object> NULL = new Null<>();
    //出队标识
    static ReferenceQueue<Object> ENQUEUED = new Null<>();
    //锁对象
    static private class Lock { };
    private Lock lock = new Lock();
    //链表的头结点
    private volatile Reference<? extends T> head = null;
    //队列的大小
    private long queueLength = 0;

    boolean enqueue(Reference<? extends T> r) { /* Called only by Reference class */
        synchronized (lock) {
            // Check that since getting the lock this reference hasn't already been
            // enqueued (and even then removed)
            ReferenceQueue<?> queue = r.queue;
            if ((queue == NULL) || (queue == ENQUEUED)) {
                return false;
            }
            assert queue == this;
            r.queue = ENQUEUED;//入队标识
            r.next = (head == null) ? r : head;
            head = r;
            queueLength++;
            if (r instanceof FinalReference) {
                sun.misc.VM.addFinalRefCount(1);
            }
            lock.notifyAll();
            return true;
        }
    }
```

2、poll()方法

实现思路也相当简单，就是判断队列中是否为空，如果不为空，则取出链表中head位置的元素即可，出队的Reference对象要加上出队标识NULL。

源码如下：

```
    public Reference<? extends T> poll() {
        if (head == null)
            return null;
        synchronized (lock) {
            return reallyPoll();
        }
    }

    @SuppressWarnings("unchecked")
    private Reference<? extends T> reallyPoll() {       /* Must hold lock */
        Reference<? extends T> r = head;
        if (r != null) {
            head = (r.next == r) ?
                null :
                r.next; // Unchecked due to the next field having a raw type in Reference
            r.queue = NULL;//出队标识
            r.next = r;//出队的Reference对象的next指向自己
            queueLength--;
            if (r instanceof FinalReference) {
                sun.misc.VM.addFinalRefCount(-1);
            }
            return r;
        }
        return null;
    }
```

remove方法这里就不再介绍，以上就是关于ReferenceQueue的一个简单的介绍。

## 2.Reference类

### 2.1、介绍

在Reference类源码的开头对此类有一个说明，摘入如下：

Abstract base class for reference objects. This class defines the  
operations common to all reference objects. Because reference objects are  
implemented in close cooperation with the garbage collector, this class may  
not be subclassed directly.

比较好理解,中文翻译为：这是引用对象的抽象基类，这个类中定义了所有引用对象的常用操作。由于引用对象是通过与垃圾回收器密切合作来实现的，因此，不能直接为此类创建子类。

以上就是源码中对此类的一个说明，我们可能获得到的有用信息为：Reference类是基类且和GC是密切相关的。

### 2.2、Reference类的4中状态

在源码中，我们可以了解到，Reference有4种状态：

1）、Active

源码中对Active状态说明如下：

Active: Subject to special treatment by the garbage collector. Some  
time after the collector detects that the reachability of the referent has changed to the appropriate state, it changes the instance’s state to either Pending or Inactive, depending upon  
whether or not the instance was registered with a queue when it was  
created. In the former case it also adds the instance to the  
pending-Reference list. Newly-created instances are Active.

翻译为：Active状态的Reference会受到GC的特别关注，当GC察觉到引用的可达性变化为其它的状态之后，它的状态将变化为Pending或Inactive，到底转化为Pending状态还是Inactive状态取决于此Reference对象创建时是否注册了queue.如果注册了queue，则将添加此实例到pending-Reference list中。 新创建的Reference实例的状态是Active。

每当我自己翻译的时候，都感觉翻译技术书籍的人真心不容易，很多东西我们自己知道是什么意思，但是当翻译过来的时候总是感觉没有描述清楚

2）、Pending

源码中对此状态给出的解释为：

**Pending**: An element of the pending-Reference list, waiting to be  
enqueued by the Reference-handler thread. Unregistered instances  
are never in this state.

翻译为：在pending-Reference list中等待着被Reference-handler 线程入队列queue中的元素就处于这个状态。没有注册queue的实例是永远不可能到达这一状态。

3）、Enqueued

源码中对此状态给出的解释为：

**Enqueued**: An element of the queue with which the instance was  
registered when it was created. When an instance is removed from  
its ReferenceQueue, it is made Inactive. Unregistered instances are  
never in this state.

翻译为：当实例被移动到ReferenceQueue中时，Reference的状态为Inactive。没有注册ReferenceQueue的不可能到达这一状态的。

4）、Inactive

源码中对此状态给出的解释为：

**Inactive**: Nothing more to do. Once an instance becomes Inactive its  
state will never change again.

翻译为：一旦一个实例变为Inactive，则这个状态永远都不会再被改变。

以上就是Reference的4中状态的一个说明。

### 2.3 Reference属性的介绍

Reference属性的介绍

在Reference中，从数据结构上看，Reference链表结构内部主要有：

```
    private T referent;         /* Treated specially by GC */
    Reference next;//指向下一个
```

另一个相当重要的属性为：

```
  volatile ReferenceQueue<? super T> queue;
```

这个queue是通过构造函数传入的，表示创建一个Reference时，要将其注册到那个queue上。

Queue的另外一个作用是可以区分不同状态的Reference。Reference有4中状态。分别为：Active、Pending、Enqueued和Inactive状态,关于这4中状态的具体含义在本文上面已经进行了简单的介绍。而这4中状态所对应的Queue如下：

1、Active

Reference类中源码给出的结果为：

```
    Active: queue = ReferenceQueue with which instance is registered, or
          ReferenceQueue.NULL if it was not registered with a queue; next =
          null
```

至于为什么是这样的，我们可以从Reference类中的构造函数分析得到。

Reference类中的构造函数为：

```
    /* -- Constructors -- */

    Reference(T referent) {
        this(referent, null);
    }

    Reference(T referent, ReferenceQueue<? super T> queue) {
        this.referent = referent;
        this.queue = (queue == null) ? ReferenceQueue.NULL : queue;
    }
```

由于Newly-created instances are Active.也就是利用构造出来的Reference实例都处于这样一个状态，而从构造函数可以看出，得到如下的结论：queue = ReferenceQueue with which instance is registered, or ReferenceQueue.NULL if it was not registered with a queue; next = null。

2、Pending

Reference类中源码给出的结果为：

```
     Pending: queue = ReferenceQueue with which instance is registered;
          next = this
```

Pending状态的Reference实例的queue = ReferenceQueue with which instance is registered;这个我们都比较好理解，但是next = this ，这个目前我就找不到根源来分析得出了。

3、Enqueued

Reference类中源码给出的结果为：

```
     Enqueued: queue = ReferenceQueue.ENQUEUED; next = Following instance
          in queue, or this if at end of list.
```

而为什么Enqueued状态的Reference实例的queue为ReferenceQueue.ENQUEUED以及next的值，我们是可以从ReferenceQueue中分析得到的？？

分析如下：从上面的介绍可知，当Reference实例入队列queue时，Reference状态就变为了Enqueued，而从ReferenceQueue的enqueue方法的源码中有这样两行代码`r.queue = ENQUEUED;r.next = (head == null) ? r : head;`，r.queue = ENQUEUED实现了对于入队的Reference的queue都进行了入队标识；这行代码r.next = (head == null) ? r : head;实现入队的Reference加在了链表的head位置

4、Inactive

Reference类中源码给出的结果为：

```
  Inactive: queue = ReferenceQueue.NULL; next = this.
```

而为什么Inactive状态的Reference实例的queue为ReferenceQueue.NULL以及next = this，我们是可以从ReferenceQueue中分析得到的？？

分析如下：从上面的介绍可知，当Reference实例出队列queue时，Reference状态就变为了Inactive，而从ReferenceQueue的poll方法的源码中有这样两行代码`r.queue = NULL；r.next = r;`，r.queue = NULL实现了对于出队的Reference的queue都进行了出队标识；这行代码r.next = r;实现出队的Reference对象的next指向自己

在Reference源码中，还有这样两个属性：

1）、Reference类中的pending属性

这个对象，定义为private的，但是全局没有任何地方给出他赋值的地方，根据下面的注释，我们可以了解到这个变量是和垃圾回收打交道的。

```
    /* List of References waiting to be enqueued.  The collector adds
     * References to this list, while the Reference-handler thread removes
     * them.  This list is protected by the above lock object. The
     * list uses the discovered field to link its elements.
     */
    private static Reference<Object> pending = null;
```

2）、Reference类中的discovered属性

与pending属性一样，同为private且上下文都没有任何地方使用它，在注释中，我们可以看到这个变量也是和垃圾回收打交道的。

```
    /* When active:   next element in a discovered reference list maintained by GC (or this if last)
     *     pending:   next element in the pending list (or null if last)
     *   otherwise:   NULL
     */
    transient private Reference<T> discovered;  /* used by VM */
```

### 2.4、ReferenceHandler线程

下面看Reference类中的ReferenceHandler线程

源码如下，

从源码中可以看出，这个线程在Reference类的static构造块中启动，并且被设置为最高优先级和daemon状态。此线程要做的事情就是不断的的检查pending是否为null，如果pending不为null，则将pending进行enqueue，否则线程进行wait状态。

```
    private static class ReferenceHandler extends Thread {

        ReferenceHandler(ThreadGroup g, String name) {
            super(g, name);
        }

        public void run() {
            for (;;) {
                Reference<Object> r;
                synchronized (lock) {
                    if (pending != null) {
                        r = pending;
                        pending = r.discovered;
                        r.discovered = null;
                    } else {
                        // The waiting on the lock may cause an OOME because it may try to allocate
                        // exception objects, so also catch OOME here to avoid silent exit of the
                        // reference handler thread.
                        //
                        // Explicitly define the order of the two exceptions we catch here
                        // when waiting for the lock.
                        //
                        // We do not want to try to potentially load the InterruptedException class
                        // (which would be done if this was its first use, and InterruptedException
                        // were checked first) in this situation.
                        //
                        // This may lead to the VM not ever trying to load the InterruptedException
                        // class again.
                        try {
                            try {
                                lock.wait();
                            } catch (OutOfMemoryError x) { }
                        } catch (InterruptedException x) { }
                        continue;
                    }
                }

                // Fast path for cleaners
                if (r instanceof Cleaner) {
                    ((Cleaner)r).clean();
                    continue;
                }

                ReferenceQueue<Object> q = r.queue;
                if (q != ReferenceQueue.NULL) q.enqueue(r);
            }
        }
    }

    static {
        ThreadGroup tg = Thread.currentThread().getThreadGroup();
        for (ThreadGroup tgn = tg;
             tgn != null;
             tg = tgn, tgn = tg.getParent());
        Thread handler = new ReferenceHandler(tg, "Reference Handler");
        /* If there were a special system-only priority greater than
         * MAX_PRIORITY, it would be used here
         */
        handler.setPriority(Thread.MAX_PRIORITY);
        handler.setDaemon(true);
        handler.start();
    }
```

有了以上的基础，我们来看一个Reference的应用

```
    /*
     * 创建一个WeakReference，并且将其referent改变
     * */
    public class TestReference2 {

        public static void main(String[] args) {
            Object o = new Object();
            WeakReference<Object> wr = new WeakReference<Object>(o);

            System.out.println(wr.get());//java.lang.Object@19e0bfd
            o = null;
            System.gc();
            System.out.println(wr.get());//null
        }

    }
```

上面的代码我们都比较容易的知道运行结果。但是至于内部实现的原因，是这样的。

由于pending是由JVM来赋值的，当Reference内部的referent对象的可达状态发生改变时，JVM会将Reference对象放入到pending链表中。因此，在例子中的代码o = null这一句，它使得o对象满足垃圾回收的条件，并且在后边显示调用了System.gc(),垃圾收集进行的时候会标记WeakReference的referent的对象o为不可达(使得wr.get()==null),并且通过赋值给pending，触发ReferenceHandler线程来处理pending.ReferenceHandler线程要做的是将pending对象enqueue。但在这个程序中我们从构造函数传入的为null,即实际使用的是ReferenceQueue.NULL,ReferenceHandler线程判断queue如果为ReferenceQueue.NULL则不进行enqueue，如果不是，则进行enqueue操作。

ReferenceQueue.NULL相当于我们提供了一个空的Queue去监听垃圾回收器给我们的反馈，并且对这种反馈不做任何处理。要处理反馈，则必须要提供一个非ReferenceQueue.NULL的queue。WeakHashMap类中提供的就是一个由意义的ReferenceQueue，非ReferenceQueue.NULL。

以上就是关于Reference类的一个介绍，可能会比较不好理解，因为确实不怎么好理解。

## 4种引用

我们都知道在Java中有4种引用，这四种引用从高到低分别为：

1）、StrongReference

这个引用在Java中没有相应的类与之对应，但是强引用比较普遍，例如：Object obj = new Object();这里的obj就是要给强引用，如果一个对象具有强引用，则垃圾回收器始终不会回收此对象。当内存不足时，JVM情愿抛出OOM异常使程序异常终止也不会靠回收强引用的对象来解决内存不足的问题。

2）、SoftReference

如果一个对象只有软引用，则在内存充足的情况下是不会回收此对象的，但是，在内部不足即将要抛出OOM异常时就会回收此对象来解决内存不足的问题。

```
    public class TestReference3 {
        private static ReferenceQueue<Object> rq = new ReferenceQueue<Object>();
        public static void main(String[] args){
            Object obj = new Object();
            SoftReference<Object> sf = new SoftReference(obj,rq);
            System.out.println(sf.get()!=null);
            System.gc();
            obj = null;
            System.out.println(sf.get()!=null);

        }
    }
```

运行结果均为：true。

这也就说明了当内存充足的时候一个对象只有软引用也不会被JVM回收。

3）、WeakReference

WeakReference基本与SoftReference类似，只是回收的策略不同。

只要GC发现一个对象只有弱引用，则就会回收此弱引用对象。但是由于GC所在的线程优先级比较低，不会立即发现所有弱引用对象并进行回收。只要GC对它所管辖的内存区域进行扫描时发现了弱引用对象就进行回收。

看一个例子：

```
    public class TestWeakReference {
        private static ReferenceQueue<Object> rq = new ReferenceQueue<Object>();
        public static void main(String[] args) {
            Object obj = new Object();
            WeakReference<Object> wr = new WeakReference(obj,rq);
            System.out.println(wr.get()!=null);
            obj = null;
            System.gc();
            System.out.println(wr.get()!=null);//false，这是因为WeakReference被回收
        }

    }
```

运行结果为： true 、false

在指向obj = null语句之前，Object对象有两条引用路径，其中一条为obj强引用类型，另一条为wr弱引用类型。此时无论如何也不会进行垃圾回收。当执行了obj = null.Object对象就只具有弱引用，并且我们进行了显示的垃圾回收。因此此具有弱引用的对象就被GC给回收了。

4）、PhantomReference

PhantomReference，即虚引用，虚引用并不会影响对象的生命周期。虚引用的作用为：跟踪垃圾回收器收集对象这一活动的情况。

当GC一旦发现了虚引用对象，则会将PhantomReference对象插入ReferenceQueue队列，而此时PhantomReference对象并没有被垃圾回收器回收，而是要等到ReferenceQueue被你真正的处理后才会被回收。

注意：PhantomReference必须要和ReferenceQueue联合使用，SoftReference和WeakReference可以选择和ReferenceQueue联合使用也可以不选择，这使他们的区别之一。

接下来看一个虚引用的例子。

```
    public class TestPhantomReference {

        private static ReferenceQueue<Object> rq = new ReferenceQueue<Object>();
        public static void main(String[] args){

            Object obj = new Object();
            PhantomReference<Object> pr = new PhantomReference<Object>(obj, rq);
            System.out.println(pr.get());
            obj = null;
            System.gc();
            System.out.println(pr.get());
            Reference<Object> r = (Reference<Object>)rq.poll();
            if(r!=null){
                System.out.println("回收");
            }
        }
    }
```

运行结果:null null 回收

根据上面的例子有两点需要说明：

1）、PhantomReference的get方法无论在上面情况下都是返回null。这个在PhantomReference源码中可以看到。

2）在上面的代码中，如果obj被置为null，当GC发现虚引用，GC会将把PhantomReference对象pr加入到队列ReferenceQueue中，注意此时pr所指向的对象并没有被回收，在我们现实的调用了rq.poll()返回Reference对象之后当GC第二次发现虚引用，而此时JVM将虚引用pr插入到队列rq会插入失败，此时GC才会对虚引用对象进行回收。

下面对Reference的3个子类进行一个简要的说明。

## SoftReference类

在JDK文档对此类的介绍如下：

1）、软引用对象，在响应内存需要时，由垃圾回收器决定是否清除此对象。软引用对象最常用于实现内存敏感的缓存。

2）、假定垃圾回收器确定在某一时间点某个对象是软可到达对象。这时，它可以选择自动清除针对该对象的所有软引用，以及通过强引用链从其可以到达该对象的针对任何其他软可到达对象的所有软引用。在同一时间或晚些时候，它会将那些已经向引用队列注册的新清除的软引用加入队列。

3）、软可到达对象的所有软引用都要保证在虚拟机抛出 OutOfMemoryError 之前已经被清除。否则，清除软引用的时间或者清除不同对象的一组此类引用的顺序将不受任何约束。然而，虚拟机实现不鼓励清除最近访问或使用过的软引用。

4）、此类的直接实例可用于实现简单缓存；该类或其派生的子类还可用于更大型的数据结构，以实现更复杂的缓存。只要软引用的指示对象是强可到达对象，即正在实际使用的对象，就不会清除软引用。例如，通过保持最近使用的项的强指示对象，并由垃圾回收器决定是否放弃剩余的项，复杂的缓存可以防止放弃最近使用的项。

总结：SoftReference是软引用，只有在JVM在抛OOM异常时才会回收。其它情况下不会回收。

## WeakReference类

在JDK文档对此类的介绍如下：

1）、弱引用对象，它们并不禁止其指示对象变得可终结，并被终结，然后被回收。弱引用最常用于实现规范化的映射。

2）、假定垃圾回收器确定在某一时间点上某个对象是弱可到达对象。这时，它将自动清除针对此对象的所有弱引用，以及通过强引用链和软引用，可以从其到达该对象的针对任何其他弱可到达对象的所有弱引用。同时它将声明所有以前的弱可到达对象为可终结的。在同一时间或晚些时候，它将那些已经向引用队列注册的新清除的弱引用加入队列。

## PhantomReference类

在JDK文档对此类的介绍如下：

1）、虚引用对象，在回收器确定其指示对象可另外回收之后，被加入队列。虚引用最常见的用法是以某种可能比使用 Java 终结机制更灵活的方式来指派 pre-mortem 清除动作。

2）、如果垃圾回收器确定在某一特定时间点上虚引用的指示对象是虚可到达对象，那么在那时或者在以后的某一时间，它会将该引用加入队列。

3）、为了确保可回收的对象仍然保持原状，虚引用的指示对象不能被获取：虚引用的 get 方法总是返回 null。

4）、与软引用和弱引用不同，虚引用在加入队列时并没有通过垃圾回收器自动清除。通过虚引用可到达的对象将仍然保持原状，直到所有这类引用都被清除，或者它们都变得不可到达。

## 应用

最后，看一个在《Java编程思想》这本书上的一个例子

```
    public class References {

        private static ReferenceQueue<VeryBig> rq = new ReferenceQueue<VeryBig>();
        public static void checkQueue(){
            Reference<? extends VeryBig> inq = rq.poll();
            if(inq!=null){
                System.out.println("In queue:"+inq.getClass().getSimpleName());
            }
        }

        public static void main(String[] args) {
            int size = 10;
            /*
             * SoftReference:在内存不足时才会回收这样软引用对象
             * */
            LinkedList<SoftReference<VeryBig>> sa = new  LinkedList<SoftReference<VeryBig>>();
            for(int i=0;i<size;i++){
                sa.add(new SoftReference(new VeryBig("Soft "+i),rq));
                System.out.println("Just created: "+sa.getLast().get());
                checkQueue();//一直为空
            }
            /*
             * WeakReference:在GC发现只具有弱引用的对象会立即对其会回收
             * */
            LinkedList<WeakReference<VeryBig>> wa = new  LinkedList<WeakReference<VeryBig>>();
            for(int i=0;i<size;i++){
                wa.add(new WeakReference(new VeryBig("Weak "+i),rq));
                System.out.println("Just created: "+wa.getLast().get());
                checkQueue();
            }

            SoftReference<VeryBig> sf = new SoftReference<VeryBig>(new VeryBig("Soft "));
            WeakReference<VeryBig> wf = new WeakReference<VeryBig>(new VeryBig("Weak"));

            System.gc();//显示的进行垃圾回收，什么时候执行就由JVM决定

            LinkedList<PhantomReference<VeryBig>> pa = new  LinkedList<PhantomReference<VeryBig>>();
            for(int i=0;i<size;i++){
                pa.add(new PhantomReference(new VeryBig("Phantom "+i),rq));
                System.out.println("Just created: "+pa.getLast());
                checkQueue();
            }       
        }

    }

    class VeryBig{
        private static final int SIZE = 10000;
        private long[] la = new long[SIZE];
        private String ident;
        public VeryBig(String id){
            ident = id;
        }
        public String toString(){
            return ident;
        }

        protected void finalize(){
            System.out.println("Finalizing "+ ident);
        }

    }
```

运行结果为：

```
    Just created: Soft 0
    Just created: Soft 1
    Just created: Soft 2
    Just created: Soft 3
    Just created: Soft 4
    Just created: Soft 5
    Just created: Soft 6
    Just created: Soft 7
    Just created: Soft 8
    Just created: Soft 9
    Just created: Weak 0
    Just created: Weak 1
    Just created: Weak 2
    Just created: Weak 3
    Just created: Weak 4
    Just created: Weak 5
    Just created: Weak 6
    Just created: Weak 7
    Just created: Weak 8
    Just created: Weak 9
    Just created: java.lang.ref.PhantomReference@19e0bfd
    In queue:WeakReference
    Just created: java.lang.ref.PhantomReference@139a55
    In queue:WeakReference
    Just created: java.lang.ref.PhantomReference@1db9742
    In queue:WeakReference
    Just created: java.lang.ref.PhantomReference@106d69c
    In queue:WeakReference
    Just created: java.lang.ref.PhantomReference@52e922
    In queue:WeakReference
    Just created: java.lang.ref.PhantomReference@25154f
    In queue:WeakReference
    Just created: java.lang.ref.PhantomReference@10dea4e
    In queue:WeakReference
    Just created: java.lang.ref.PhantomReference@647e05
    In queue:WeakReference
    Just created: java.lang.ref.PhantomReference@1909752
    In queue:WeakReference
    Just created: java.lang.ref.PhantomReference@1f96302
    In queue:WeakReference
```

根据上面的知识点的介绍，理解这个程序应该不难。下面还是分析下：

首先对于下一段代码，由于是SoftReference引用，因此只有在内存不足时才会被回收。

```
            LinkedList<SoftReference<VeryBig>> sa = new  LinkedList<SoftReference<VeryBig>>();
            for(int i=0;i<size;i++){
                sa.add(new SoftReference(new VeryBig("Soft "+i),rq));
                System.out.println("Just created: "+sa.getLast().get());
                checkQueue();//一直为空
            }
```

对于下面这段代码，由于是WeakReference引用，因此当GC发现其就会被回收。由于这段代码最后一句进行了gc的显示调用，因此这些具有弱引用的对象就都会被回收掉，在回收之前调用了对象的finalize的方法并将WeakReference加入的队列Queue中。

```
            LinkedList<WeakReference<VeryBig>> wa = new  LinkedList<WeakReference<VeryBig>>();
            for(int i=0;i<size;i++){
                wa.add(new WeakReference(new VeryBig("Weak "+i),rq));
                System.out.println("Just created: "+wa.getLast().get());
                checkQueue();
            }

            System.gc();//显示的进行垃圾回收，什么时候执行就由JVM决定
```

对于最后一段代码，在执行这段代码之前，如果调用System.gc()后JVM立即进行垃圾回收则队列queue装的是已经被回收的WeakReference引用，因此，在下段代码中的每个for循环中调用的checkQueue()函数就有输出了

由于是PhantomReference，因此在垃圾回收前要将PhantomReference加入到其注册的队列中。

```

            LinkedList<PhantomReference<VeryBig>> pa = new  LinkedList<PhantomReference<VeryBig>>();
            for(int i=0;i<size;i++){
                pa.add(new PhantomReference(new VeryBig("Phantom "+i),rq));
                System.out.println("Just created: "+pa.getLast());
                checkQueue();
            }
```

如果我们在上面代码后面也添加一个System.gc(),则也会对虚引用对象进行一个垃圾回收。

最后要说明的是：上面程序的运行结果每次运行可能都不会一致，这是因为当我们显示调用System.gc();时JVM虚拟机有时并不会立即执行。

例如：不立即执行的其中一种情况的结果为：

```
    Just created: Soft 0
    Just created: Soft 1
    Just created: Soft 2
    Just created: Soft 3
    Just created: Soft 4
    Just created: Soft 5
    Just created: Soft 6
    Just created: Soft 7
    Just created: Soft 8
    Just created: Soft 9
    Just created: Weak 0
    Just created: Weak 1
    Just created: Weak 2
    Just created: Weak 3
    Just created: Weak 4
    Just created: Weak 5
    Just created: Weak 6
    Just created: Weak 7
    Just created: Weak 8
    Just created: Weak 9
    Just created: java.lang.ref.PhantomReference@19e0bfd
    Just created: java.lang.ref.PhantomReference@139a55
    Just created: java.lang.ref.PhantomReference@1db9742
    Just created: java.lang.ref.PhantomReference@106d69c
    Just created: java.lang.ref.PhantomReference@52e922
    Just created: java.lang.ref.PhantomReference@25154f
    Just created: java.lang.ref.PhantomReference@10dea4e
    Just created: java.lang.ref.PhantomReference@647e05
    Just created: java.lang.ref.PhantomReference@1909752
    Just created: java.lang.ref.PhantomReference@1f96302
    Finalizing Weak
    Finalizing Weak 9
    Finalizing Weak 8
    Finalizing Weak 7
    Finalizing Weak 6
    Finalizing Weak 5
    Finalizing Weak 4
    Finalizing Weak 3
    Finalizing Weak 2
    Finalizing Weak 1
    Finalizing Weak 0
```

## 参考资料

1、[http://hongjiang.info/java-referencequeue/](http://hongjiang.info/java-referencequeue/)

2、[http://blog.sina.com.cn/s/blog_667ac0360102e9f3.html](http://blog.sina.com.cn/s/blog_667ac0360102e9f3.html)
