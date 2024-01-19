---
source: https://blog.csdn.net/u013978512/article/details/120011860?spm=1001.2014.3001.5501
---
## 一、JUC 架构概览

        java.util.concurrent（简称JUC ）包是java5.0之后提供的工具包，主要是在并发编程中使用的一些工具类。下面是从其他资料中看到的挺不错的一些脑图，供大家在脑海中有一个框架模型。（侵权可删）：

来自于[https://blog.csdn.net/chen7253886/article/details/52769111](https://blog.csdn.net/chen7253886/article/details/52769111)的JUC分类图：

![图片引自百度](https://img-blog.csdn.net/20161009180852140)

来自[https://blog.csdn.net/mulinsen77/article/details/84586859](https://blog.csdn.net/mulinsen77/article/details/84586859)的不同锁之间的关系图

![](https://img-blog.csdnimg.cn/20181128162808800.png?x-oss-process=image/watermark,type_ZmFuZ3poZW5naGVpdGk,shadow_10,text_aHR0cHM6Ly9ibG9nLmNzZG4ubmV0L211bGluc2VuNzc=,size_16,color_FFFFFF,t_70)

 来自于[https://www.jianshu.com/p/38fe92bcca7e](https://www.jianshu.com/p/38fe92bcca7e)的ReentrantLock的继承关系图

![](https://img-blog.csdnimg.cn/img_convert/8004249ec61840c0b4028434ee991a48.png)

## 二、ReentrantLock加锁过程分析

        通过上一节我们知道，AbstractQueuedSynchronizer是JUC中各种锁的基础类，ReentrantLock同样继承自AbstractQueuedSynchronizer，所以我们通过分析ReentrantLock的同时也是对AbstractQueuedSynchronizer的分析。        

        首先看下AbstractQueuedSynchronizer的属性：

```
private transient volatile Node head;private transient volatile Node tail;private volatile int state;
```

        加锁过程就是调 ReentrantLock的lock方法

```
private static ReentrantLock reentrantLock = new ReentrantLock(true);reentrantLock.lock();
```

        ![](https://img-blog.csdnimg.cn/20210901223737385.png?x-oss-process=image/watermark,type_ZHJvaWRzYW5zZmFsbGJhY2s,shadow_50,text_Q1NETiBA5aKo6Imy5bGx5rC0,size_20,color_FFFFFF,t_70,g_se,x_16)

        一个reentrantLock对象内部有[sync](https://so.csdn.net/so/search?q=sync&spm=1001.2101.3001.7020)对象，sync对象内部有state属性，在 reentrantLock.lock()的时候，其实就是把state的状态改了，释放锁就是把state的值改回来。  

        锁，说的通俗一点就是多个线程只能有一个线程对某个共享变量进行变更，其他线程如果发现改变量被变更了，就认为其他线程已经占有了，那么自己就需要阻塞等待。这里state就是那个共享变量，如果是0代表已经没有被某个线程修改，如果是1代表已经被某个线程修改。因为state只能被一个线程修改，所以state需要用[volatile](https://so.csdn.net/so/search?q=volatile&spm=1001.2101.3001.7020)修改，保证读取到的是最新的值，当然，这还不够，因为两个线程从主内存读取到state到自己的工作内存都是0，然后+1后写回主内存，两个线程都认为自己获取到了锁，遮掩就会失去锁的作用。所以在volatile修饰的基础上，还要有CAS的过程，保证只有一个线程可以对state修改成功。  
        公平锁和非公平锁的区别是，公平锁：在判断出state的状态是0时，不是立马通过CAS机制尝试获取锁，而是先判断队列中是否有其他线程等了半天的，有的话就让别人先获取锁。

![](https://img-blog.csdnimg.cn/20210904182330412.png?x-oss-process=image/watermark,type_ZHJvaWRzYW5zZmFsbGJhY2s,shadow_50,text_Q1NETiBA5aKo6Imy5bGx5rC0,size_20,color_FFFFFF,t_70,g_se,x_16)

        上图中左侧是公平锁尝试获取锁的代码，右侧是非公平锁尝试获取锁的代码，可以看到，公平锁只比非公平锁多了一步判断队列中是否有阻塞的线程的逻辑。

        下面主要对公平锁进行分析 

公平锁的加锁过程总共有三步：

1. 尝试获取锁

2.获取锁失败，加入阻塞队列

3. 不断轮询获取锁或被挂起

```
public final void acquire(int arg) {        if (!tryAcquire(arg) &&acquireQueued(addWaiter(Node.EXCLUSIVE), arg))selfInterrupt();    }
```

 tryAcquire方法的流程图 

![](https://img-blog.csdnimg.cn/20210902222520263.png?x-oss-process=image/watermark,type_ZHJvaWRzYW5zZmFsbGJhY2s,shadow_50,text_Q1NETiBA5aKo6Imy5bGx5rC0,size_20,color_FFFFFF,t_70,g_se,x_16)

 addWaiter方法的流程图

![](https://img-blog.csdnimg.cn/20210902223050136.png?x-oss-process=image/watermark,type_ZHJvaWRzYW5zZmFsbGJhY2s,shadow_50,text_Q1NETiBA5aKo6Imy5bGx5rC0,size_20,color_FFFFFF,t_70,g_se,x_16)

aquireQueued方法的流程图

![](https://img-blog.csdnimg.cn/20210903072925524.png?x-oss-process=image/watermark,type_ZHJvaWRzYW5zZmFsbGJhY2s,shadow_50,text_Q1NETiBA5aKo6Imy5bGx5rC0,size_20,color_FFFFFF,t_70,g_se,x_16)

 下面通过源码进行分析

```
protected final boolean tryAcquire(int acquires) {final Thread current = Thread.currentThread();//获取state字段，这个就是AQS的一个属性            int c = getState();if (c == 0) {//如果等于0，就代表目前没有线程获取锁。公平锁的情况下，就要判断下队列中是不是已经有在等待的线程了，如果有的话，自己不能抢，要让给前面等待的线程，这样才能做到先来后到的公平if (!hasQueuedPredecessors() &&//如果队列中没有，那么自己就通过CAS尝试获取锁，其实就是把state由0改成1                    compareAndSetState(0, acquires)) {//如果修改成功，说明获锁成功，把自己的设置为占有者                    setExclusiveOwnerThread(current);return true;                }            }//如果state不为0，那么判断下获取锁的线程是不是当前线程else if (current == getExclusiveOwnerThread()) {//如果是，那么就可重入，即对state++                int nextc = c + acquires;if (nextc < 0)                    throw new Error("Maximum lock count exceeded");                setState(nextc);return true;            }//其他情况下，获取锁失败return false;        }
```

 获取锁失败的情况下，就需要把自己放在队列中等待别人把锁释放掉再尝试获取锁。

首先是把自己放在队列中

```
private Node addWaiter(Node mode) {//根据当前线程创建一个node节点        Node node = new Node(Thread.currentThread(), mode);// Try the fast path of enq; backup to full enq on failure        Node pred = tail;//先尝试一次快速入队if (pred != null) {            node.prev = pred;if (compareAndSetTail(pred, node)) {                pred.next = node;return node;            }        }        enq(node);return node;    }private Node enq(final Node node) {for (;;) {            Node t = tail;if (t == null) { // Must initialize//tail为空时，进行初始化，设置了一个未关联到任何thread的Node为队列的head。if (compareAndSetHead(new Node()))                    tail = head;            } else {//不为空时，加到队尾                node.prev = t;if (compareAndSetTail(t, node)) {                    t.next = node;return t;                }            }        }    }
```

 入队之后，就可以排队抢锁了

```
final boolean acquireQueued(final Node node, int arg) {boolean failed = true;        try {boolean interrupted = false;for (;;) {final Node p = node.predecessor();//判断前置节点是不是head，如果是的话，当前node节点就可以尝试获取锁//为什么呢？因为如果前置节点是head的话，当前节点就是阻塞队列的第一个。好比在窗口排队一样，头部的人是在办理业务，没有阻塞，真正阻塞的是从第二个人开始的。head有可能正在执行，或者执行完成，所以第二个节点就不断尝试获取锁if (p == head && tryAcquire(arg)) {//如果获取锁成功，把当前节点赋值为head                    setHead(node);//原来的head已经处理完了，所以就可以出队了，也即是和后面的节点断开联系                    p.next = null; // help GC                    failed = false;return interrupted;                }//如果获取锁失败，那么就判断是不是要挂起if (shouldParkAfterFailedAcquire(p, node) &&                    parkAndCheckInterrupt())                    interrupted = true;            }        } finally {if (failed)                cancelAcquire(node);        }    }
```

```
private static boolean shouldParkAfterFailedAcquire(Node pred, Node node) {        int ws = pred.waitStatus;if (ws == Node.SIGNAL)/** This node has already set status asking a release* to signal it, so it can safely park.*/return true;if (ws > 0) {//它的前驱节点的waitStatus>0，相当于CANCELLED（因为状态值里面只有CANCELLED是大于0的），那么CANCELLED的节点作废，当前节点不断向前找并重新连接为双向队列，直到找到一个前驱节点waitStats不是CANCELLED的为止/** Predecessor was cancelled. Skip over predecessors and* indicate retry.*/            do {                node.prev = pred = pred.prev;            } while (pred.waitStatus > 0);            pred.next = node;        } else {//它的前驱节点不是SIGNAL状态且waitStatus<=0，利用CAS机制更新为SIGNAL状态/** waitStatus must be 0 or PROPAGATE.  Indicate that we* need a signal, but don't park yet.  Caller will need to             * retry to make sure it cannot acquire before parking.             */            compareAndSetWaitStatus(pred, ws, Node.SIGNAL);        }        return false;    }
```

        由于是多线程对应的[node](https://so.csdn.net/so/search?q=node&spm=1001.2101.3001.7020)节点是先入队，才会走shouldParkAfterFailedAcquire的逻辑，且waitStatus初始化的值为0，所以不会导致后面的节点跳过前面的有效的节点的情况。

        这个方法的逻辑就是当前node节点修改前驱节点的waitStatus状态为SIGNAL，表示前驱节点释放锁的时候唤醒它的后驱节点。

        为什么shouldParkAfterFailedAcquire(p, node)返回false的时候不直接挂起线程呢？原因是为了应对在经过这个方法后，node已经是head的直接后继节点了。在compareAndSetWaitStatus设置head的waitStatus为SIGNAL成功之前，head节点已经释放锁了，设置成功也没什么用，如果这时候，线程还要挂起，那就真的永远挂起了，再也不会被唤醒了。

        如果是挂起之前，前置节点释放锁了怎么办呢？这里挂起和唤醒采用的是LockSupport。park方法是挂起，uppark是唤醒。unpark函数可以先于park调用。比如线程B调用unpark函数，给线程A发了一个“许可”，那么当线程A调用park时，它发现已经有“许可”了，那么它会马上再继续运行。等有机会后面写文章对LockSupport原理进行分析。

参考：

[https://www.cnblogs.com/skywang12345/p/java_threads_category.html](https://www.cnblogs.com/skywang12345/p/java_threads_category.html)

[https://blog.csdn.net/weixin_38106322/article/details/107154961](https://blog.csdn.net/weixin_38106322/article/details/107154961)

[https://mp.weixin.qq.com/s/OtKBAIRbOiRummxazyl3Xg](https://mp.weixin.qq.com/s/OtKBAIRbOiRummxazyl3Xg)

[https://www.jianshu.com/p/dcbcea767d69](https://www.jianshu.com/p/dcbcea767d69)

[https://www.cnblogs.com/xrq730/p/7056614.html](https://www.cnblogs.com/xrq730/p/7056614.html)

[https://www.jianshu.com/p/9ebca222513b](https://www.jianshu.com/p/9ebca222513b)

[https://blog.csdn.net/hengyunabc/article/details/28126139](https://blog.csdn.net/hengyunabc/article/details/28126139)
