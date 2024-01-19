---
source: https://blog.csdn.net/wenxindiaolong061/article/details/107193637
---
## 什么是弱引用

-   弱引用实例：java.lang.ref.WeakReference类或者其子类的一个实例，就是一个弱引用实例。
-   弱引用：如果一个弱引用实例的成员变量referent引用了一个对象obj，那么就称这个弱引用实例对obj的引用是弱引用。
-   弱引用对象：被一个弱引用实例引用的对象，称为弱引用对象。

```
public class WeakReference<T> extends Reference<T> {

    /**
     * 构造一个弱引用实例，此弱引用实例会引用给定的参数对象
     */
    public WeakReference(T referent) {
        super(referent);
    }
    
    /**
     * 构造一个引用了给定对象referent的弱引用实例，同时将referent对象注册到一个引用队列
     */
    public WeakReference(T referent, ReferenceQueue<? super T> q) {
        super(referent, q);
    }
}
```

## 示例

在实际业务场景中，我们通常会定义一个WeakReference的子类来解决我们的需求。  
例如：

```
class Apple{
String color;
void Apple(String color){
this.color = color;
}
String getColor(){
return color;
}
void setColor(String color){
this.color = color;
}
public String toString(){
return new StringBuilder("Apple[color=").append(this.color).append("]").toString();
}
// 当对象被GC回收时，会回调finalize()方法
protected void finalize() throws Throwable{
System.out.println(this);
}
}
```

如下所定义的Salad类，其实例是一个弱引用实例，其实例会持有一个Apple类对象的弱引用。当一个Apple实例对象只被salad类实例（或者其它弱引用实例）引用时，它就会被GC回收。

```
class Salad extends WeakReference<Apple>{
Apple apple;
void Salad(Apple apple){
super(apple);
}
}
```

## 弱引用 WeakReference 相关的GC回收规则

当一个对象只被弱引用实例引用（持有）时，这个对象就会被GC回收。

WeakReference类的javadoc：

一个弱引用实例，不会对它(的成员变量referent)所引用的对象的finalizable（是否可销毁）、finalized（销毁）和 reclaimed（GC回收）产生任何影响。

如果GC在某个时间点确定某对象是弱可达的（只被某个或某些弱引用对象引用），那么它就会清除对该弱可达对象的所有弱引用（将引用了弱可达对象的弱引用实例的referent置为null：referent=null），同时还会找出从"GC Roots"到该对象的强引用链和软引用链上的所有弱可达对象，然后也会清除对这些弱可达对象的所有弱引用。

同时，GC会将以上弱可达对象标记为可销毁的(finalizable)。然后会立刻或者在稍后的某个时间点，将以上那些清除了的弱引用实例对象入队到它们在创建时就注册到的queue中去。（参考Reference）

弱引用通常用于实现规范化映射（WeakHashMap、WeakCache）。

**注意**

-   上述规则中，会被GC标记为finalizable的的是弱引用实例引用的对象，而非弱引用实例本身
-   如果显式地声明了一个变量E e，并使之指向一个对象：e = new E()，这时变量e就是这个新创建的对象的一个强引用。如果变量e所引用的这个对象同时又被WeakReference的一个实例持有，则由于存在对对象的一个强引用e，对象并不符合上述回收规则，因此对象至少在变量e的作用域范围内都不会被回收。

示例：

```
public class Test{

    public static void main(String[] args) throws InterruptedException{
// saladWithRedApple 引用的Apple对象符合弱引用回收规则
Salad saladWithRedApple = new Salad(new Apple("red "));  

Apple green = new Apple("green");
// saladWithGreenApple 引用的Apple对象不符合弱引用回收规则，因为它同时被green这个强引用所引用
Salad saladWithGreenApple = new Salad(green); 

System.gc();
try{
Thread.sleep(5000);
}catch(InterruptedException e){
e.printStackTrace();
}
out.println(saladWithRedApple.get()==null);  // true
out.println(saladWithGreenApple.get()==null);  // false
}
}
```

## Reference

此类是所有Reference类的实例对象（所有Reference的实现类的实例对象）的抽象基类，此类中定义了所有Reference类的实例对象的通用操作。由于引用类对象同垃圾回收两者之间有密切的关系（对象的回收本身就与对象的引用关系密切，例如初代垃圾收集器就是判断对象是否还被变量引用来确定对象是否可以被会回收的），因此子类可能不会直接实现Reference类（而是实现WeakReference等类，以避免出现垃圾对象不被及时回收的情况。注：如果用户直接实现Reference类，就相当于一个定义一个强引用类，因为GC对于用户自定义的类并没有做任何特殊处理。但GC对于JDK中定义的 SoftReference 和 WeakReference 等，都做了特殊处理，因此就有了不要直接实现Reference类的建议。）。

在Reference类中定义了以下几个实例变量：

```
private T referent;         /* 会被GC进行特殊处理 */  /* 此引用实际指向的对象 */

    volatile ReferenceQueue<? super T> queue;      /* 当前实例创建时如果对此queue赋值，则称当前实例注册到了此queue */
    
    Reference next;   /* 用于确定当前实例是否处于active状态。active:null, pending:this, enqueued:next element in queue, inactive:this */
    transient private Reference<T> discovered;     /* used by VM */
```

Reference对实例对象定义了4种内部状态（没有显式地用枚举类声明出来）：

-   活动的（active）:刚创建的Reference实例处于活动状态。垃圾收集器会对active状态的引用所指向的实际对象referent做特殊处理：当垃圾收集器监测到active状态的实例的referent的可达性变成了某个特定状态时，会将当前Reference实例的状态由active更改为pending或者inactive。具体取决于实例创建时，是否注册到了一个ReferenceQueue队列（即r的queue是否为null），如果实例创建时注册了queue（注意注册到queue与添加到queue不是一个概念，这里的注册到了queue，实际是指r持有了一个ReferenceQueue实例的引用），则实例状态改为pending，并会被添加到挂起队列（pendinglist，挂起队列同queue不是一个队列）；否则实例状态被改为inactive。
-   挂起（pending）：当实例被添加到挂起队列pending-list中后，状态就会被改为pending，即挂起队列中的所有元素的状态都是Pending。挂起队列中的元素都在等待线程类将实例入队(添加到元素自身持有的queue中去)。创建时没有注册到queue的Reference实例永远也不会变成此状态。
-   enqueued：当实例被添加到其自身持有的queue（即其创建时注册的queue）后，状态被更改为enqueued。当实例被从此队列中移除后，状态就变为inactive
-   不活跃（inactive）：当实例被更改成此状态后，其状态就不会再改变了。

实例在各个状态下时，其所持有的ReferenceQueue实例-queue变量和持有的Reference实例-next变量的值如下：

-   active状态时：queue = 实例被创建时如果注册了queue，则此queue就不会空。否则queue=ReferenceQueue.NULL； next = null.
-   pending状态时：queue=创建时注册到的queue，next=this。
-   enqueued状态时：queue=ReferenceQueue.ENQUEUED（其实也是null）；next=原来的队头（头插法），如果原来的队列为空，则next=this。
-   inactive状态时：queue=ReferenceQueue.NULL，next=this。

在如上这种模式下，垃圾收集器仅需要通过检查next字段就能确定实例是否需要特别的处理：如果next==null，那么实例处于active状态，如果next!=null，这垃圾收集器只需对实例进行常规处理。  
为了确保垃圾收集器能在不干扰对reference实例对象进行enqueue()的应用线程的正常运行的情况下，能发现处于active状态的实例，垃圾收集器应该通过discovered字段链接这些处于active状态的实例。  
discovered字段也用于链接挂起列表中的Reference实例对象。

## ReferenceQueue

Reference类中定义了一个此类的对象：

`volatile ReferenceQueue<? super T> queue;`

当某Reference类实例（或其子类的实例）可能将不会再被使用，需要被垃圾收集器监测以回收时，应将对象追加到此queue中。

垃圾收集器会不断监测此queue中的实例的状态，当监测到实例变更为某种状态时，会对对象进行垃圾回收。

当前对象r入队后即queue.enqueue()，就会将自己的queue变量置空，即r.queue=null，以便垃圾收集器回收。

## Lock

```
static private class Lock { }
private static Lock lock = new Lock();
```

定义了Lock类并创建了一个此类的实例，用于作为同步垃圾收集线程的对象。  
垃圾收集线程在每个收集周期开始时必须先获得此锁，因此获得此锁的其它代码应尽快完成：尽量不要创建新的对象、尽量避免调用用户代码。

## pending

pending是一个全局变量，每个JVM中只有一份。

`private static Reference<Object> pending=null; // 是一个链表的头结点`

pending指向一个链表的头结点，当某个处于特殊状态的Reference实例需要插入此链表中时，会采用头插法的方式，将自己设置为pending，成为新的头结点。

pending所指向的链表中的所有 Reference 实例都是处于挂起状态、等待入队的 Reference 实例。

当active状态的引用实例的referent的可达性处于某个特定状态时，垃圾收集线程会将此Reference实例添加到这个pending链表，并等待引用处理线程将元素从pending链表中移除，然后enqueue()到元素注册的queue中去。

这个链表被lock对象保护。

这个链表用元素的discovered字段链接每个元素（相当于链表节点中的next字段）。

## ReferenceHandler

此线程用于将pending所指向的链表中的所有处于pending状态的Reference实例，入队到它们各自持有的queue中去。

此线程会调用boolean tryHandlePending(boolean waitFor)方法来处理pending状态的引用对象。

此线程的优先级被设置为最高。

如果可能还有其它pending状态的引用实例，tryHandlePending会返回true。如果没有其它pending状态的实例，并且希望应用程序可以做更多的有意义的工作而不是这个线程一直自旋，一直占用CPU，则会返回false。

tryHandlePending方法的waitForNotify参数的意义：如果参数值为true，则线程会一直wait直到VM notify了它、或者线程被interrupted。如果参数值为false，则当没有pending状态的引用时，线程就立即退出了。  
如果处理了一个pending状态的引用，则方法返回true。如果没有要处理的对象，则一直wait，直到被notify或者被

```
private static class ReferenceHandler extends Thread {
...
public void run() {
            while (true) {
                tryHandlePending(true);
            }
        }
        ...
}        
```

```
static boolean tryHandlePending(boolean waitForNotify) {
        Reference<Object> r;
        Cleaner c;
        try {
            synchronized (lock) {
                if (pending != null) {
                    r = pending;
                    // 'instanceof' might throw OutOfMemoryError sometimes
                    // so do this before un-linking 'r' from the 'pending' chain...
                    c = r instanceof Cleaner ? (Cleaner) r : null;
                    // unlink 'r' from 'pending' chain
                    pending = r.discovered;
                    r.discovered = null;
                } else {
                    // The waiting on the lock may cause an OutOfMemoryError
                    // because it may try to allocate exception objects.
                    if (waitForNotify) {
                        lock.wait();
                    }
                    // retry if waited
                    return waitForNotify;
                }
            }
        } catch (OutOfMemoryError x) {
            // Give other threads CPU time so they hopefully drop some live references
            // and GC reclaims some space.
            // Also prevent CPU intensive spinning in case 'r instanceof Cleaner' above
            // persistently throws OOME for some time...
            Thread.yield();
            // retry
            return true;
        } catch (InterruptedException x) {
            // retry
            return true;
        }

        // Fast path for cleaners
        if (c != null) {
            c.clean();
            return true;
        }

        ReferenceQueue<? super Object> q = r.queue;
        if (q != ReferenceQueue.NULL) q.enqueue(r);
        return true;
    }
```

## Reference类中的静态代码块

Reference中定义了一些静态代码块，主要是启动一个线程，将处于pending状态的引用类对象入队，入队后的Reference实例的状态将变成Enqueued。

在Reference类中，lock、pending、handler = new ReferenceHandler(…)、tryHandlePending(…)这些成员都是类成员，因此，handler 线程是对全局的pending链表中的所有处于pending状态的Re实例进行处理。  
queue、next、discovered则是实例变量。

```
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

        // provide access in SharedSecrets
        SharedSecrets.setJavaLangRefAccess(new JavaLangRefAccess() {
            @Override
            public boolean tryHandlePendingReference() {
                return tryHandlePending(false);
            }
        });
    }

```

## WeakReference

弱引用对象，不会其引用的对象被JVM设置为可回收状态，然后被回收。弱引用通常用于实现规范化映射。

如果垃圾收集器在某个时间点确定某个对象的可达性是弱可达的（即这个对象可以通过一个弱引用链可达，即使同时也有其它强引用链或者软引用链可达此对象），那么GC就会清除所有引用这个对象的弱引用，还会通过可以到达这个对象的强引用链和软引用链找到链上其它对象上的所有弱引用、并清除所有这些弱引用。

同时，GC还会将所有之前被清除了弱引用的对象声明为finalizable的。并且可能同时或者接着就会将那些弱引用实例本身添加到它们注册到的ReferenceQueue队列中去。

## ReferenceQueue 类什么用？

在Reference类中，定义了一个ReferenceQueue类型的成员变量，变量名为queue。  
并定义了相应的构造函数：

```
public Reference( T referent, ReferenceQueue<? super T> queue){
this.referent = referent;
this.queue = (queue == null) ? ReferenceQueue.NULL : queue;
}
```

当构造一个引用实例时，如果初始化了成员变量queue的值，我称之为将引用实例与queue绑定了。  
那么将引用实例与这个queue绑定，有什么用呢？  
由于GC会对Reference类及其子类的实例进行特殊方式的处理，比如对于weak引用实例，会在每次GC时都会将其发现的所有weak引用实例的referent 断开其引用。但是用户可能需要对此做一些个性化的处理。因此，JVM设计出这样的方式：GC在清理weak实例时，会将weak实例入队到其绑定的queue中，用户就可以去queue中获取这些被GC处理了的weak实例，然后再做一些个性化处理。

并不是所有的reference实例都必须绑定一个queue，如果用户不需要对被GC的实例做特殊的处理，就不用设置。

一般在缓存map场景下，会定义一个ReferenceQueue，如WeakHashMap，WeakCache等。因为通常将实际的key封装成一个WeakReference类实例，存储到缓存map的key中，目的是借助GC自动及时释放缓存内存，防止map过大。但GC自动将map中weak实例对实际key的referent置为null后，相应的entry就失去了在map中存在的意义了，这时queue的作用就出来了：GC在清理weak实例时，将此实例入队到创建实例时绑定到的queue中，用户主动遍历这个queue，将queue中元素对应的map中的entry清理掉。否则map永远也不会释放这些已经失去意义的entry，这就会造成内存泄漏。

## WeakReference常用场景下的内存泄漏问题

以ThreadLocal.ThreadLocalMap为例，经常看到如下说法：ThreadLocalMap中，`Entry extends WeakReference<ThreadLocal>`，Entry的key也即ThreadLocal实例本身会被赋值给WeakReference的referent，JVM执行GC时，只要遇到弱引用就会将其断开，即设置`referent=null`，则Entry的key变null了，那么Entry的value就已经没有意义了，也应该能被GC回收掉，否则就是内存泄漏。但是如果我们不主动调用`threadlocal.remove()`，不主动设置vlaue=null，那么被value引用的对象就会一直到线程销毁都无法被GC回收掉，这就是ThreadLocal会造成内存泄漏的说法。

但是，针对这种情况，ThreadLocal也不是什么都没做。

在ThreadLocal实例每次执行set(T value)方法时（首次创建线程的threadLocals对象时除外），最后都会执行以下代码

```
if (!cleanSomeSlots(i, sz) && sz >= threshold)
                rehash();
```

cleanSomeSlots方法代码如下：

```
private boolean cleanSomeSlots(int i, int n) {
            boolean removed = false;
            Entry[] tab = table;
            int len = tab.length;
            do {
                i = nextIndex(i, len);
                Entry e = tab[i];
                if (e != null && e.get() == null) {   // 检查key是否为null
                    n = len;
                    removed = true;
                    i = expungeStaleEntry(i);        // 清理value
                }
            } while ( (n >>>= 1) != 0);
            return removed;
        }
```

可见，除了第一次，其后每次向线程的threadLocals 中添加entry时，都会清理在此之前被GC掉的的key对应的entry。  
也就是说，通常情况下，每个线程最多只会存在一个应该被GC回收但未能被回收的泄漏的对象。  
如果这个对象非常大，占用JVM内存空间较多，那么就影响较大。  
如果线程非常多，每个线程都有一个泄漏的对象，那么影响也较大。


