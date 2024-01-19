---
source: https://www.zeral.cn/java/unsafe.park-vs-object.wait/
---
`Unsafe.park/unpark` 和 `Object.wait/notify` 都可以用来实现线程的阻塞和唤醒，但两者有些本质的区别。

> 当线程被阻塞时，它不会消耗任何 CPU 时间，也不会被操作系统调度执行。
> 
> Park ：停放，在某个地方停留一段时间。

## LockSupport

`Unsafe.park` 通常被用在 `LockSupport` 的 `park` 方法中，`LockSupport` 用于创建锁和其他同步类的**基本线程阻塞原语**。

`AbstractQueuedSynchronizer(AQS)` 框架中的方法大量使用该类来构建，该抽象类用于实现依赖先进先出 (FIFO) 等待队列的阻塞锁和相关同步器（信号量、事件等），位于 `java.util.concurrent` 包下的大部分状态依赖类都构建于它之上，例如 `ReentrantLock`、`Semaphore` 等。

此类与使用它的每个线程相关联一个许可（在 `Semaphore` 信号量类的意义上）。如果许可证可用，`park` 调用将立即返回，进程也将消费掉该许可；否则当前线程将出于线程调度目的而被阻塞并处于休眠状态。如果许可证不可用，则调用 `unpark` 可使许可证可用。（与信号量不同，许可不会累积。最多只有一个。）

方法 `park` 和 `unpark` 提供了阻塞和解除阻塞线程的有效方法，并且不会因为遇到方法 `Thread.suspend` 和 `Thread.resume` 而变得不可用，这是因为：由于许可的存在，调用 `park` 的线程和另一个试图将其 `unpark` 的线程之间的竞争将保持活性。此外，如果调用者的线程被中断， `park` 将返回，并且支持超时版本。

`park` 方法还可以在其他任何时间由于虚假唤醒“毫无理由”地返回，**因此通常必须在返回条件的循环里调用此方法重新检查**。从这个意义上说，`park` 是“忙碌等待”的一种优化，它不会浪费这么多的时间进行自旋，但是必须将它与 `unpark` 配对使用才更高效。

也就是说，park 方法在发生以下三种情况之一才会返回：

-   其他一些线程以当前线程为目标调用 unpark；
-   其他一些线程中断当前线程；
-   虚假唤醒返回。

这些方法旨在用作创建更高级别同步实用程序的工具，并且它们本身对大多数并发控制应用程序没有用处。 `park` 方法通常的使用形式：

```
while (!canProceed()) {
   // 确保 unpark 请求对其它线程可见
   ...
   LockSupport.park(this);
 }
```

Java docs 中的示例用法：先进先出非重入锁类的草图：

```
 class FIFOMutex {
   private final AtomicBoolean locked = new AtomicBoolean(false);
   private final Queue<Thread> waiters
     = new ConcurrentLinkedQueue<>();

   public void lock() {
     boolean wasInterrupted = false;
     // 发布当前线程给 unparkers
     waiters.add(Thread.currentThread());

     // 当前线程不是队列中的第一个或无法获取锁时阻塞
     while (waiters.peek() != Thread.currentThread() ||
            !locked.compareAndSet(false, true)) {
       LockSupport.park(this);
       // 等待时忽略中断
       if (Thread.interrupted())
         wasInterrupted = true;
     }

     waiters.remove();
     // 在返回时确保正确的中断状态
     if (wasInterrupted)
       Thread.currentThread().interrupt();
   }

   public void unlock() {
     locked.set(false);
     LockSupport.unpark(waiters.peek());
   }

   static {
     // 减小由于类加载导致的程序无响应“丢失 unpark”的风险
     Class<?> ensureLoaded = LockSupport.class;
   }
 }
```

我们也可以使用它来有效地“休眠”线程，使用 `LockSupport.parkNanos` 或 `LockSupport.parkUntil`，而无需另一个线程唤醒（以纪元时间为初始值的毫秒），它们内部都是直接调用的 Unsafe 类。

虽然 LockSupport 可以用来休眠，但由于它可能存在虚假唤醒、由于中断而返回并且没有抛出中断异常、并且不处理休眠时监视器的所有权等，并不适合我们直接来调用，更好的方法还是使用更加直观明了的 `Thread.sleep`。

> Thread.sleep 和 LockSupport.park 都不会释放当前线程持有的任何锁。

### 内部实现

```
public class LockSupport {
private LockSupport() {} // Cannot be instantiated.
// Hotspot 实现依赖内部 API
private static final Unsafe U = Unsafe.getUnsafe();
private static final long PARKBLOCKER = U.objectFieldOffset(Thread.class, "parkBlocker");

// 除非许可可用，否则禁用当前线程以进行线程调度。
public static void park() {
        U.park(false, 0L);
  }
  
  // 使给定线程的许可证可用，如果它尚不可用。
  // 如果线程在 park 上被阻塞，那么调用该方法将解除阻塞。
  // 否则，调用该方法后的下一次 park 调用将不会阻塞。如果给定线程尚未启动，则不保证此操作有任何效果。
  public static void unpark(Thread thread) {
        if (thread != null)
            U.unpark(thread);
  }
}
```

从上面代码来看，**park/unpark** 很简单，主要实现在 **Unsafe** 类：

```
public final class Unsafe {
public native void unpark(Object thread);

public native void park(boolean isAbsolute, long time);
}
```

但是 Unsafe 中的定义是 native，也就意味着，得去 Hotspot 源码中看真正的底层实现。

#### Hotspot 中的 Unsafe 实现

[jdk8u/hotspot/src/share/vm/prims/unsafe.cpp](https://github.com/openjdk/jdk8u/blob/master/hotspot/src/share/vm/prims/unsafe.cpp)

首先看 **park** 方法

```
// These are the methods for 1.8.0
// 方法名映射
static JNINativeMethod methods_18[] = {
    ...
    ...
    {CC "park",               CC "(ZJ)V",                    FN_PTR(Unsafe_Park)},
    {CC "unpark",             CC "(" OBJ ")V",               FN_PTR(Unsafe_Unpark)}
};

UNSAFE_ENTRY(void, Unsafe_Park(JNIEnv *env, jobject unsafe, jboolean isAbsolute, jlong time))
  ...
  JavaThreadParkedState jtps(thread, time != 0);
  // 重要方法
  thread->parker()->park(isAbsolute != 0, time);
...
UNSAFE_END
```

再看 **unpark** 方法

```
UNSAFE_ENTRY(void, Unsafe_Unpark(JNIEnv *env, jobject unsafe, jobject jthread))
  UnsafeWrapper("Unsafe_Unpark");
  Parker* p = NULL;
  ...
  java_thread = JNIHandles::resolve_non_null(jthread);
  // jvm 线程
  JavaThread* thr = java_lang_Thread::thread(java_thread);
  p = thr->parker();
  if (p != NULL) {
    // 关键点
    p->unpark();
  }
UNSAFE_END
```

可见，其关键实现在 `JavaThread` 中的 parker 对象。

#### JavaThread

Java.lang.Thread 是 Jdk 应用层面的线程，JavaThread 为 HotSpot 中的线程，一一对应，其关系如下：

![JavaThread](https://www.zeral.cn/java/unsafe.park-vs-object.wait/images/121341870-aa34fd00-c953-11eb-8642-06120633e755.png)

[jdk8u/hotspot/src/share/vm/runtime/thread.hpp->JavaThread](https://github.com/openjdk/jdk8u/blob/master/hotspot/src/share/vm/runtime/thread.hpp)

```
// 类层次结构
// - Thread
//   - NamedThread
//     - VMThread
//     - ConcurrentGCThread
//     - WorkerThread
//       - GangWorker
//       - GCTaskThread
//   - JavaThread
//   - WatcherThread

// thread.hpp
class Thread: public ThreadShadow {
  
  protected:
  // 与线程关联的操作系统数据
  OSThread* _osthread;  // 特定于平台的线程信息
  ParkEvent * _ParkEvent;    // 用于 synchronized(), 实现 wait/notify
  ParkEvent * _SleepEvent;   // 用于 Thread.sleep
  // JSR166 per-thread parker
  Parker*    _parker; // 用于 LockSupport::park
  
  ......
}

class JavaThread: public Thread {
oop          _threadObj; // Java 级别线程对象

// JSR166 per-thread parker
private:
  Parker*    _parker;  
  
  ......
}
```

还没到，我们还需要进一步看 Parker 实现：

#### Parker 和 PlatformParker

[jdk8u/hotspot/src/share/vm/runtime/park.hpp](https://github.com/openjdk/jdk8u/blob/master/hotspot/src/share/vm/runtime/park.hpp)

```
class Parker : public os::PlatformParker {
private:
  // 重要变量，通过 0/1 表示是否持有许可，决定是否阻塞
  volatile int _counter ;
  JavaThread * AssociatedWith ; // 当前关联的 JavaThread

public:
  Parker() : PlatformParker() {
    _counter       = 0 ;
    AssociatedWith = NULL ;
  }

public:
  void park(bool isAbsolute, jlong time);
  void unpark();
  ...
  ...
};
```

Parker 继承自 os::PlatformParker，也就是说，由各个平台具体实现。

下面我们看 Linux 下的实现：[jdk8u/hotspot/src/os/linux/vm/os_linux.hpp->PlatformParker](https://github.com/openjdk/jdk8u/blob/master/hotspot/src/os/linux/vm/os_linux.hpp)

```
class PlatformParker : public CHeapObj<mtInternal> {
    int _cur_index;  // 正在使用哪个条件：-1, 0, 1
    // 锁
    pthread_mutex_t _mutex [1] ;
    // 条件变量
    pthread_cond_t  _cond  [2] ; // 一个用于相对时间，一个用于绝对时间。
    ...
    ...
};
```

**park()/unpark() 最终实现**：[jdk8u/hotspot/src/os/linux/vm/os_linux.cpp](https://github.com/openjdk/jdk8u/blob/master/hotspot/src/os/linux/vm/os_linux.cpp)

```
void Parker::park(bool isAbsolute, jlong time) {
  // 原 _counter 不为零，许可可用，不需等待
  // 否则依赖于具有完整屏障语义的 Atomic::xchg() 对 _counter 进行无锁更新设置为0
  if (Atomic::xchg(0, &_counter) > 0) return;
  
// 检查中断，加锁
  if (Thread::is_interrupted(thread, false) || pthread_mutex_trylock(_mutex) != 0) {
    return;
  }

......

  if (time == 0) {
  // 等待并自动释放 mutex 锁
    status = pthread_cond_wait (&_cond[_cur_index], _mutex) ;
  } else {
    // 计时等待并自动释放 mutex 锁
    status = os::Linux::safe_cond_timedwait (&_cond[_cur_index], _mutex, &absTime) ;
  }
  
  ......
  
  // 已经从 block 住状态中恢复返回了, 把 _counter 设 0.
  _counter = 0 ;
  // 解锁
  status = pthread_mutex_unlock(_mutex) ;
}

void Parker::unpark() {
  // 整体按照加锁->通知->解锁的顺序
  int s, status ;
  status = pthread_mutex_lock(_mutex);
  s = _counter;
  _counter = 1;
  if (s < 1) {
    // thread might be parked
    if (_cur_index != -1) {
      // thread is definitely parked
      if (WorkAroundNPTLTimedWaitHang) {
        status = pthread_cond_signal (&_cond[_cur_index]);
        status = pthread_mutex_unlock(_mutex);
      } else {
        int index = _cur_index;
        status = pthread_mutex_unlock(_mutex);
        status = pthread_cond_signal (&_cond[index]);
      }
    } else {
      pthread_mutex_unlock(_mutex);
    }
  } else {
    pthread_mutex_unlock(_mutex);
  }
}
```

到这里，我们应该可以说是较为透彻但仅是粗线条的搞清了锁和线程同步底层实现的脉络。

#### 类的关系

上面介绍的几个底层实现类，其关系如下：

![JavaThread](https://www.zeral.cn/java/unsafe.park-vs-object.wait/images/121477019-e6229d80-c9f9-11eb-981e-2f4a65db31e9.png)

#### pthread_cond_wait？

追踪至 **pthread_cond_wait**，可以说明确了实现脉络，其内部究竟如何实现呢？

通过本节参考，使用 strace，可以再进一步。示例代码节选自本节参考：

```
#include <stdio.h>
...

int main() {
    pthread_cond_t cond;
    pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;

    if (initializeCond(&cond)) {
        return -1;
    }
    ...
    
    struct timespec start, end, deadline;
    if (pthread_mutex_lock(&lock)) {
        return -1;
    }
    clock_gettime(CLOCK_TYPE, &start);
    addNanos(&start, &deadline, deltaNanos);

    int iteration = 0;
    while (!isDeadlineReached(&deadline)) {
        // pthread_cond_timedwait(&cond, &lock, &deadline);
      pthread_cond_wait(&cond, &lock);
        iteration++;;
    }
...
    clock_gettime(CLOCK_TYPE, &end);
    return 0;
}
```

编译：`gcc test.c -o a.out -lpthread`

追踪：`strace -e futex ./a.out`

![futex](https://www.zeral.cn/java/unsafe.park-vs-object.wait/images/121489839-0e64c900-ca07-11eb-804f-cc4d276cc79b.png)

本节参考 [LockSupport.parkNanos() Under the Hood and the Curious Case of Parking](https://hazelcast.com/blog/locksupport-parknanos-under-the-hood-and-the-curious-case-of-parking/)

由此可见，**pthread_cond_wait/pthread_cond_timedwait** 由 futex 实现。那么 futex 又是什么东西呢？

#### futex

> `futex()` 系统调用提供了一种等待某个条件成立的方法。它通常用作共享内存同步上下文中的阻塞构造。使用 futex 时，大多数同步操作都是在用户空间中执行的。用户空间程序仅在程序可能必须阻塞更长的时间直到条件变为真时才使用`futex()`系统调用。其他 `futex()` 操作可用于唤醒任何等待特定条件的进程或线程。

futex 方法里重要的是 **FUTEX_WAIT** 和 **FUTEX_WAKE**，基本对应了 wait/notify。

futex（“Fast userspace mutex”的缩写），何谓 fast，大多数情况下，锁是没有竞争的，通过用户态原子操作即可完成，这称为轻量级；少数需要等待情况下，才进入内核态，是谓重量级操作。

![Futex](https://www.zeral.cn/java/unsafe.park-vs-object.wait/images/122011603-dab2e600-cdee-11eb-9bb9-1636a2eb247e.png)

[Basics of Futexes](https://eli.thegreenplace.net/2018/basics-of-futexes/)

[Fuss, Futexes and Furwocks: Fast Userlevel Locking in Linux](https://www.kernel.org/doc/ols/2002/ols2002-pages-479-495.pdf)

[Linux Locking Mechanisms](https://www.slideshare.net/kerneltlv/linux-locking-mechanisms)

## Object.wait/notify

Object 中的 `wait`、`notify`、 `notifyAll` 方法构成了内部条件队列的 API。一个对象的内部锁与它的内部条件队列是相关的：为了能够调用对象 X 中的任一个条件队列方法，你必须持有对象 X 的锁。

条件队列可以让一组线程 一一 称作等待集，以某种方式等待相关条件变成真，它也由此得名。不同于传统的队列，它们的元素是数据项，条件队列的元素是等待相关条件的线程。就像每个 Java 对象都能当作锁一样，每个对象也能当作条件队列。

这是因为“等待基于状态的条件”机制必须和“维护状态一致性”机制紧密地绑定在一起：除非你能检査状态，否则你不能等待条件(这里看后面代码就明白了，说的就是只有 if/while 的条件判断后面才能跟上 wait 方法)；同时，除非你能改变状态，否则你不能从条件等待(队列)中释放其他的线程（这里说的就是，要先改变先验条件的状态，才能调用 `notify` 或 `notifyAll` 方法）。

`object.wait` **会自动释放锁**，并请求 OS（操作系统）挂起当前线程，让其他线程获得该锁进而修改对象的状态。当它被唤醒时，它会在返回前重新获得锁。直观上看，调用 `wait` 意味着“我要去休息了，但是发生了需要关注的事情后叫醒我”，调用通知（`notify/notifyAll`）方法意味着“需要关注的事情发生了”。

```
// 条件依赖方法的规范式
void stateDependentMethod() throws InterruptedException {
    // 条件谓词必须被锁守护
    synchronized (lock) {
        while (!conditionPredicate()) {  // 使用while的原因也是避免虚假唤醒，唤醒后重新检查状态
            lock.wait();
        }
        // 现在,对象处于期望的状态中
    }
}

// 其它线程改变先验条件
void changeState() {
    synchronized (lock) {
      // 先改变先验状态
      changeConditionPredicateToTrue();
      lock.notifyAll();
    }
}
```

### 内部实现

```
public class Object {

public final void wait() throws InterruptedException {
        wait(0L);
  }
  
  // 使当前线程等待直到它被唤醒，通常是通过被通知 notify 或中断 interrupute，或者直到经过一定的实时时间。
  public final native void wait(long timeoutMillis) throws InterruptedException;
  
 /** 唤醒正在此对象的监视器上等待的单个线程。如果有任何线程正在等待该对象，则选择其中一个被唤醒。该选择是任意的，并由实现自行决定。线程通过调用其中一个 wait 方法在对象的监视器上等待。
  * 被唤醒的线程将无法继续，直到当前线程放弃对该对象的锁定。
  * 被唤醒的线程将以通常的方式与可能正在积极竞争以在此对象上同步的任何其他线程竞争；例如，被唤醒的线程在成为下一个锁定该对象的线程时不享有可靠的特权或劣势。
  * 此方法只能由作为该对象监视器所有者的线程调用。线程通过以下三种方式之一成为对象监视器的所有者：
  * 对象实例同步方法: synchronized(this) {}；
  * 以该对象为锁的同步代码块: synchronized(lockObject) {}；
  * 类的同步静态方法: synchronized void method {}。
  * 一次只有一个线程可以拥有一个对象的监视器。
  */
  public final native void notify();
}
```

#### 进程或者线程如何 sleep 和 wait()？

锁等待状态需要主动放弃时间片时，底层需要调用常见方法 **sleep/wait/yield**，起到释放的作用。

同样的，这些方法也是 native 方法，`Object.wait()` 方法是使用 `JVM_MonitorWait` 本机方法实现的，根据 `ThreadReference` javadoc：

```
/** Thread is waiting - Object.wait() or JVM_MonitorWait() was called */
public final int THREAD_STATUS_WAIT = 4;
```

该方法的实现可以在 [`jvm.cpp`](http://hg.openjdk.java.net/jdk/jdk11/file/80abf702eed8/src/hotspot/share/prims/jvm.cpp#l602) 中找到，其使用 `ObjectSynchronizer::wait`：

```
JVM_ENTRY(void, JVM_MonitorWait(JNIEnv* env, jobject handle, jlong ms))
  JVMWrapper("JVM_MonitorWait");
  Handle obj(THREAD, JNIHandles::resolve_non_null(handle));
  JavaThreadInObjectWaitState jtiows(thread, ms != 0);
  if (JvmtiExport::should_post_monitor_wait()) {
    JvmtiExport::post_monitor_wait((JavaThread *)THREAD, (oop)obj(), ms);

    // 当前线程已经拥有监视器并且还没有添加到等待队列，因此当前线程不能做接班人。
    // 这意味着 JVMTI_EVENT_MONITOR_WAIT 事件处理程序不能意外消费 unpark() 与此 ObjectMonitor 关联的 ParkEvent。
  }
  ObjectSynchronizer::wait(obj, ms, CHECK);
JVM_END
```

`ObjectSynchronizer::wait` 的实现在 [`synchronizer.cpp`](http://hg.openjdk.java.net/jdk/jdk11/file/caf115bb98ad/src/hotspot/share/runtime/synchronizer.cpp#l479) 中，其委托给 [`objectMonitor.cpp`](http://hg.openjdk.java.net/jdk/jdk11/file/83aec1d357d4/src/hotspot/share/runtime/objectMonitor.cpp#l1416) 中的 `ObjectMonitor::wait`。

#### ObjectMonitor

Monitor 可以理解为一个同步工具或一种同步机制，通常被描述为一个对象。每一个 Java 对象都有一把看不见的锁，称为内部锁或者 Monitor 监视器锁。

通常所说的对象的内置锁，是对象头 Mark Word 中的重量级锁指针指向的 monitor 对象，该对象就是 HotSpot 中的 ObjectMonitor：

```
// 结构体如下
ObjectMonitor::ObjectMonitor() {  
  _header       = NULL;  
  _count       = 0;  
  _waiters      = 0,  
  _recursions   = 0;       // 线程的重入次数
  _object       = NULL;  
  _owner        = NULL;    // 标识拥有该 monitor 的线程
  _WaitSet      = NULL;    // 等待线程组成的双向循环链表，_WaitSet 是第一个节点
  _WaitSetLock  = 0 ;  
  _Responsible  = NULL ;  
  _succ         = NULL ;  
  _cxq          = NULL ;    // 多线程竞争锁进入时的单向链表
  FreeNext      = NULL ;  
  _EntryList    = NULL ;    // _owner 从该双向循环链表中唤醒线程结点，_EntryList 是第一个节点
  _SpinFreq     = 0 ;  
  _SpinClock    = 0 ;  
  OwnerIsThread = 0 ;  
}  
```

特别重要的两个属性：

监控区（Entry List）：锁已被其他线程获取，期待获取锁的线程就进入 Monitor 对象的监控区

待授权区（Wait Set）：曾经获取到锁，但是调用了 wait 方法，线程进入待授权区

**`ObjectMonitor`队列之间的关系转换：**

[![ObjectMonitor Set](https://www.zeral.cn/java/unsafe.park-vs-object.wait/images/1053518-20180206154923920-1943749421.png)](https://images2017.cnblogs.com/blog/1053518/201802/1053518-20180206154923920-1943749421.png)

**内置锁状态转换图：**

[![ObjectMonitor State](https://www.zeral.cn/java/unsafe.park-vs-object.wait/images/443934-20210310171645384-756938550.png)](https://img2020.cnblogs.com/blog/443934/202103/443934-20210310171645384-756938550.png)

**对象内置锁 ObjectMonitor 流程**：

-   所有期待获得锁的线程，在锁已经被其它线程拥有的时候，这些期待获得锁的线程就进入了对象锁的 `entry set` 区域。
-   所有曾经获得过锁，但是由于其它必要条件不满足而需要 wait 的时候，线程就进入了对象锁的 `wait set` 区域 。
-   在 `wait set` 区域的线程获得 `Notify/notifyAll` 通知的时候，随机的一个 `Thread（Notify）` 或者是全部的 `Thread（NotifyALL）` 从对象锁的 `wait set` 区域进入了 `entry set` 中。
-   在当前拥有锁的线程释放掉锁的时候，处于该对象锁的 `entry set` 区域的线程都会抢占该锁，但是只能有任意的一个 Thread 能取得该锁，而其他线程依然在 `entry set` 中等待下次来抢占到锁之后再执行。

`_waitSet` 和 `_EntryList` 和我们要探索的 `wait` 和 `notify` 方法有关：

##### **wait 方法的实现过程**

```
// 1.调用 ObjectSynchronizer::wait 方法
void ObjectSynchronizer::wait(Handle obj, jlong millis, TRAPS) {
...
  // 2.获得 Object 的 monitor 对象(即内置锁)
  ObjectMonitor* monitor = ObjectSynchronizer::inflate(THREAD, obj());
  DTRACE_MONITOR_WAIT_PROBE(monitor, obj(), THREAD, millis);
  // 3.委托给 monitor 的 wait 方法
  monitor->wait(millis, true, THREAD);
  ...
}

void ObjectMonitor::wait(jlong millis, bool interruptible, TRAPS) {
  ...
  if (interruptible && Thread::is_interrupted(Self, true) && !HAS_PENDING_EXCEPTION) {
    ...
    // 抛出异常，不会直接进入等待
    THROW(vmSymbols::java_lang_InterruptedException());
    ...
  }
  ...
  ObjectWaiter node(Self);
  node.TState = ObjectWaiter::TS_WAIT;
  Self->_ParkEvent->reset();
  OrderAccess::fence();

  // 通过自旋获得互斥锁保证并发安全
  Thread::SpinAcquire(&_WaitSetLock, "WaitSet - add");
  // 4.在 wait 方法中调用 addWaiter 方法
  AddWaiter(&node);
  Thread::SpinRelease(&_WaitSetLock);

  if ((SyncFlags & 4) == 0) {
    _Responsible = NULL;
  }

  ...
  // 5.然后在 ObjectMonitor::exit 释放锁，接着 thread_ParkEvent->park 也就是 wait
  exit(true, Self); 
  ...
  if (interruptible && (Thread::is_interrupted(THREAD, false) || HAS_PENDING_EXCEPTION)) {
        // 故意留空
      } else if (node._notified == 0) {
        if (millis <= 0) {
          Self->_ParkEvent->park();
        } else {
          ret = Self->_ParkEvent->park(millis);
        }
  }
  // 被 notify 唤醒之后的善后逻辑
  ...
}

inline void ObjectMonitor::AddWaiter(ObjectWaiter* node) {
  ...
  if (_WaitSet == NULL) {
    // _WaitSet 为 null，就初始化 _waitSet
    _WaitSet = node;
    node->_prev = node;
    node->_next = node;
  } else {
    // 否则就尾插
    ObjectWaiter* head = _WaitSet ;
    ObjectWaiter* tail = head->_prev;
    assert(tail->_next == head, "invariant check");
    tail->_next = node;
    head->_prev = node;
    node->_next = head;
    node->_prev = tail;
  }
}
```

##### notify 方法的底层实现

```
// 1.调用 ObjectSynchronizer::notify 方法
void ObjectSynchronizer::notify(Handle obj, TRAPS) {
  ...
  // 2.调用 ObjectSynchronizer::inflate 方法
  ObjectSynchronizer::inflate(THREAD, obj())->notify(THREAD);
}

// 3.通过 inflate 方法得到 ObjectMonitor 对象
ObjectMonitor * ATTR ObjectSynchronizer::inflate (Thread * Self, oop object) {
  ...
  if (mark->has_monitor()) {
    ObjectMonitor * inf = mark->monitor() ;
    assert (inf->header()->is_neutral(), "invariant");
    assert (inf->object() == object, "invariant") ;
    assert (ObjectSynchronizer::verify_objmon_isinpool(inf), "monitor is inva;lid");
    return inf 
  }
  ...
}

// 4.调用 ObjectMonitor 的 notify 方法
void ObjectMonitor::notify(TRAPS) {
  ...
  // 5.调用 DequeueWaiter 方法移出 _waiterSet 第一个结点
  ObjectWaiter * iterator = DequeueWaiter() ;
  // 6.根据不同的 notify 策略，选择如何处置唤醒节点
  // 策略 0：将需要唤醒的 node 放到 EntryList 的头部
  // 策略 1：将需要唤醒的 node 放到 EntryList 的尾部
  // 策略 2：将需要唤醒的 node 放到 _cxq 的头部
  // 策略 3：将需要唤醒的 node 放到 _cxq 的尾部
...
}
```

#### Parker 和 ParkEvent

我们可以看到基于监视器锁的 wait/notify 方法很大一部分是在管理和调度关联锁的线程节点，而真正阻塞/唤醒的原语主要是 JVM 中 JavaThread 的 _ParkEvent 的 park/unpark 方法，JavaThread 之前已经描述过，它是 JVM 层面 Java 线程的实现。

我们再回过头看下 [jdk8u/hotspot/src/share/vm/runtime/thread.hpp->JavaThread](https://github.com/openjdk/jdk8u/blob/master/hotspot/src/share/vm/runtime/thread.hpp)，它里面有两个 ParkEvent 和一个 Parker，其实 ParkEvent 和 Parker 实现和功能十分类似。一个 ParkEvent 是实现 synchronized 关键字，wait，notify 用的，一个是给 Thread.sleep 用的。Parker 是用来实现 J.U.C 的 park/unpark (阻塞 / 唤醒)。也就是说 ParkEvent 是用来处理多个线程竞争同一个资源的情况，而 Parker 比较简单，是用来处理一个线程的阻塞，其他线程唤醒它的情况。

这里我们就不贴代码了，具体可以查看 [Java Thread 和 Park](https://www.beikejiedeliulangmao.top/java/concurrent/thread-park) 中的代码解释。

对比我们可以看到 ParkEvent 的 park 函数和 Parker 的 park 函数很像，先是通过 cas 修改了 _Event，然后根据 _Event 的原始值决定是不是要加 mutex 锁和睡眠。其实，JVM park 的这个思路很类似于 Java 实现单例的双重检查模式，因为第一次通过 cas 的检查，如果发现有令牌就不加锁，不等待了，毕竟加锁和等待都是很重的过程，可能会被阻塞，而 cas 很轻很快，效率更高。

然后在一个循环中执行 park，这一点和 Parker 不太一样，在 Parker 中没有这一层循环，这是因为 Parker 调用 Parker::park 函数的只有一个线程，就是 parker 对象从属的线程，所以当它被唤醒时资源的使用不存在竞争，而 ParkEvent 则不同，ParkEvent 用来实现 wait 和 monitor 锁，所以是会出现多个线程都在等待，当多个线程都被唤醒时就要通过判断当前令牌是否已经被别人抢走来决定之后的处理流程。如果令牌被别的线程抢走了，自己就继续睡眠，否则返回用户代码。但是，即便如此 Parker::park 可能也会有虚假唤醒（spurious wakeup）的情况发生。

从源码我们也可以看到，不管是 LockSupport 的 park 还是 Object 的 wait/notify 或是 Thread.sleep 它们最终都会使用平台的阻塞原语，Linux 平台下它们都指向了 **pthread_cond_wait/pthread_cond_timedwait**，用来构建基于条件的高效阻塞互斥量。

## 比较

他们都将挂起正在运行的线程并将其置于阻塞等待状态，有 3 种方法可以使线程处于 `Thread.state.WAITING` 状态：

-   没有超时的 `Object.wait`
-   没有超时的 `Thread.join`
-   没有超时的 `LockSupport.park`

使线程处于 `Thread.state.TIMED_WAITING` 状态的方法除了上述方法的超时版本，还有 `Thread.sleep`：

-   `Thread.sleep(long millis)`
-   有超时的 `Object.wait(long timeout)`
-   有超时的 `Thread.join(long millis)`
-   有超时的 `LockSupport.parkNanos(long nanos)`
-   有超时的 `LockSupport.parkUntil(long deadline)`

但这两种方法的工作原理不同。 **`Object.wait()` 方法适用于基于监视器锁的同步，在调用该方法后会释放锁**。为了将等待线程恢复为可运行状态，我们将在同一个监视器对象上使用 `Object.notify()` 方法。当线程回到可运行状态时，它肯定会获得跨多个线程共享的变量的更新值。**JVM 将确保线程状态与主内存同步，但这是额外的开销。**

`Unsafe.park()` 用于 JSR166-JUC 支撑基本线程阻塞原语构建高级同步工具，它直接在单个线程上工作，park 方法并不会释放当前线程持有的任何锁，它将线程作为参数并将其置于休眠状态。要将 `park` 的线程变回可运行状态，我们需要在同一线程上调用 `Unsafe.unpark()` 方法。它在许可的基础上工作。当 `Unsafe.unpark()` 被调用时，如果线程已经被 `park`，它将解除阻塞，或者将确保线程上的下一个 `park` 调用立即解除阻塞。**所以它的性能应该更好，因为不需要与主存同步。** 这就是为什么线程池（例如 `ExecutorService`）在等待来自阻塞队列的任务时使用 `park` 方法的原因。

如您所见，这些用例是不同的。如果您有跨线程共享的状态并且您想确保一个线程在继续更新之前应该等待另一个线程，那么您应该继续使用 `wait()` 和 `notify()` 方法。作为应用程序开发人员，**大多数情况下您不必使用 `park()` 方法，它的 API 级别太低，而且要注意它不会自动同步线程的本地内存缓存。**

## 参考

-   [java锁,协作与线程-3-LockSupport之park/unpark实现溯源](https://selfpoised.github.io/java/thread/hotspot/jvm/locksupport/park/unpark/futex/2021/06/10/java%E9%94%81,%E5%8D%8F%E4%BD%9C%E4%B8%8E%E7%BA%BF%E7%A8%8B-3-LockSupport%E4%B9%8Bpark-unpark%E5%AE%9E%E7%8E%B0%E6%BA%AF%E6%BA%90.html)
    
-   [Unsafe.park vs Object.wait](https://newbedev.com/unsafe-park-vs-object-wait)
    
-   [对象内置锁 ObjectMonitor](https://www.cnblogs.com/hongdada/p/14513036.html)
    
-   [java-并发之基石篇](https://createchance.github.io/post/java-%E5%B9%B6%E5%8F%91%E4%B9%8B%E5%9F%BA%E7%9F%B3%E7%AF%87/)
    
-   [Java Thread 和 Park](https://www.beikejiedeliulangmao.top/java/concurrent/thread-park)
