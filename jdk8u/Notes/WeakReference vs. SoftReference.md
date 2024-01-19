---
source: https://zhuanlan.zhihu.com/p/29415902
---
接上面的两节课，我们这节课看一下上节课留下的两个问题：

第一，WeakReferent何时加到链表中去？是否应该添加，由什么来决定？

第二，在既有弱引用，又有强引用的情况下，如何把对象的新地址更新到弱引用中去。

我还是用图说明一下这两个问题JVM分别是怎么处理的。首先，上节课说了，当GC扫描的时候，遇到弱引用，就会先把这个弱引用放到链表里，但却不会搬移被引用对象。这是一种情况。

第二种情况，同时存在强引用和弱引用。如下图所示，黑色虚线代表弱引用，实线代表强引用。

![](https://pic1.zhimg.com/v2-c0d3c677357bc908527913f945e738ec_b.png)

如果说在GC扫描的时候，先扫描到A，然后检查到C，那么毫无疑问，A和C都会被搬到survivor空间去。也就是做一次forward操作。

然后，当扫描到B的时候，发现B所引用的对象，也就是C已经被forward到新的survivor空间中去了，那这时，就把B也搬到新的空间，然后把C的新地址更到B那里。结果如下图所示，这时，并不会把B加到discovered链表中去。B对C的引用其实和强引用就没有什么区别了。

![](https://pic2.zhimg.com/v2-ec24d9ad9347c452572e4ecafac8b561_b.png)

那如果说，B先扫描怎么办呢？B看到C并没有在新的survivor space中，这时候，JVM还是先把B放到链表里，并且把B搬到survivor中（注意B一直是存活的），并且保持C不动。然后当扫描到A，仍然当做普通的引用进行处理，A和C都会被搬到新的空间里。但是，这样一来，当GC结束的时候，B所指的对象是不对的了。如下图所示：

![](https://pic2.zhimg.com/v2-b36cd67411d9890769de3cf3b5e26e8d_b.png)

所以，我们必须得在GC结束以后，对这种情况再做一次检查。

做法就是把Reference链表遍历一遍，看一下每个Reference的 referent 对象是否存活，也就是是否被forward到了新的survivor中去。我们之前介绍copy gc的时候，已经说过了，一个被forward过的对象，它的markOop会指向新的地址，也就是说，上图中，C' 的 forwarding 指针是指向 C 的，我们通过这个指针把 B 再指向C就行了。当然，别忘了，这时的B已经不需要被处理了，所以应该把B从链表中删除了。经过这样的处理以后，在新的空间里，就变成这样了：

![](https://pic1.zhimg.com/v2-f7410a0cc057c1c85d9cdff95d357038_b.png)

这样一来，上节课所遗留的两个问题就全部解决了。

最后一步，添加到链表中以后，JVM会负责把WeakReference对象的引用置为NULL，然后，由ReferenceHandler线程再去处理这个链表。

好了。WeakReference这个还有不明白的，直接向我提问。我感觉我已经讲得挺清楚的了。

SoftReference的处理与WeakReference的处理是一样的。所不同的是，它仅仅是在是否把Reference添加到链表里，这一步多增了一些判断而已。

在hotspot/src/share/vm/memory/referenceProcessor.cpp的discover_reference这个方法里，可以看到这样的条件：

```
if (rt == REF_SOFT) {
    // For soft refs we can decide now if these are not
    // current candidates for clearing, in which case we
    // can mark through them now, rather than delaying that
    // to the reference-processing phase. Since all current
    // time-stamp policies advance the soft-ref clock only
    // at a major collection cycle, this is always currently
    // accurate.
    if (!_current_soft_ref_policy->should_clear_reference(obj, _soft_ref_timestamp_clock)) {
      return false;
    }    
  }
```

should_clear_reference的实现如下所示：

```
// The oop passed in is the SoftReference object, and not
// the object the SoftReference points to.
bool LRUMaxHeapPolicy::should_clear_reference(oop p,
                                             jlong timestamp_clock) {
  jlong interval = timestamp_clock - java_lang_ref_SoftReference::timestamp(p);
  assert(interval >= 0, "Sanity check");

  // The interval will be zero if the ref was accessed since the last scavenge/gc.
  if(interval <= _max_interval) {
    return false;
  }

  return true;
}
```

可见，SoftReference的回收还要满足一个条件，那就是当前引用的存活时间是不是大于_max_interval，如果大于_max_interval，那它就和WeakReference一样处理，如果不大于的话，那就当成普通的强引用处理。

这里的几个timestamp，是在JDK中定义的。

```
public class SoftReference<T> extends Reference<T> {
    // 由JVM负责更新的，记录了上一次GC发生的时间。
    static private long clock;

    // 每次调用 get 方法都会更新，记录了当前Reference最后一次被访问的时间。
    private long timestamp;

    public SoftReference(T referent) {
        super(referent);
        this.timestamp = clock;
    }

    public SoftReference(T referent, ReferenceQueue<? super T> q) {
        super(referent, q);
        this.timestamp = clock;
    }

    // 和super.get的逻辑最大的不同，就在于每次调用get都会把上次发生GC的时间，也就是
    // clock 更新到 timestamp 中去。
    public T get() {
        T o = super.get();
        if (o != null && this.timestamp != clock)
            this.timestamp = clock;
        return o;
    }
}
```

好了。搞清楚这些变量，我们再回过头来看这行语句：

```
jlong interval = timestamp_clock - java_lang_ref_SoftReference::timestamp(p);
```

这行语句所对应的Java代码就是 ref.clock - ref.timestamp，只是在JVM中直接访问Java Object就是这种写法了。

最后一个问题，_max_interval是在哪里设定的呢？

```
// Capture state (of-the-VM) information needed to evaluate the policy
void LRUMaxHeapPolicy::setup() {
  size_t max_heap = MaxHeapSize;
  max_heap -= Universe::get_heap_used_at_last_gc();
  max_heap /= M;

  _max_interval = max_heap * SoftRefLRUPolicyMSPerMB;
  assert(_max_interval >= 0,"Sanity check");
}
```

哦，原来是计算了一下，上一次GC以后，堆里还有多少剩余空间，然后把这些空间通过一次乘法转换成_max_interval。也就是说，max_heap越小，那么_max_interval就会越小，SoftReference就会有越大的可能性被回收。很多人肯定都看过这句话：**SoftReference只会在内存空间不够用的情况下才会被回收。**但没有人能说清楚，什么情况算是堆内存不够用。我这里把代码show给大家看了。Hotspot里关于内存不够用可是有明确的定义的哦。

另外，这里还通过这代码向大家展示了一个JVM参数：SoftRefLRUPolicyMSPerMB。这个参数调得越小，SoftReference就会越倾向于尽快回收SoftReference。

好了。今天就把WeakReference和SoftReference全部介绍完了。后面我们再介绍finalize和Cleaner。

上一节课：[弱引用拾遗](https://zhuanlan.zhihu.com/p/29254258)

下一节课：[PhantomReference & Cleaner](https://zhuanlan.zhihu.com/p/29454205)

课程目录：[课程目录](https://zhuanlan.zhihu.com/p/24393775?refer=hinus)
