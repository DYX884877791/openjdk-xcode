---
source: https://www.zhihu.com/question/46648593/answer/102318391
---
## **前言**

大家好，我是小彭。

**[在之前的文章里](https://link.zhihu.com/?target=https%3A//juejin.cn/post/7163301919316934693)**，我们聊到了 Java 标准库中 **[HashMap](https://link.zhihu.com/?target=https%3A//juejin.cn/post/7163985718417555487)** 与 **[LinkedHashMap](https://link.zhihu.com/?target=https%3A//juejin.cn/post/7164348785512939551)** 的实现原理。HashMap 是一个标准的散列表数据结构，而 LinkedHashMap 是在 HashMap 的基础上实现的哈希链表。

今天，我们来讨论 WeakHashMap，其中的 “Weak” 是指什么，与前两者的使用场景有何不同？我们就围绕这些问题展开。

提示： 本文源码基于 JDK 1.2 WeakHashMap。

> **本文已收录到 [AndroidFamily](https://link.zhihu.com/?target=https%3A//github.com/pengxurui/AndroidFamily)，技术和职场问题，请关注公众号 [彭旭锐] 提问。**

___

小彭的 Android 交流群 02 群已经建立啦，扫描文末二维码进入~

___

**[思维导图](https://www.zhihu.com/search?q=%E6%80%9D%E7%BB%B4%E5%AF%BC%E5%9B%BE&search_source=Entity&hybrid_search_source=Entity&hybrid_search_extra=%7B%22sourceType%22%3A%22answer%22%2C%22sourceId%22%3A2784386555%7D)：**

![](https://pic1.zhimg.com/50/v2-260937ceef61ce5292ad707d0eba924c_720w.jpg?source=1940ef5c)

___

## **1. 回顾 HashMap 和 LinkedHashMap**

其实，WeakHashMap 与 HashMap 和 LinkedHashMap 的数据结构大同小异，所以我们先回顾后者的实现原理。

### **1.1 说一下 HashMap 的实现结构**

HashMap 是基于分离链表法解决散列冲突的动态散列表。

-   1、HashMap 在 Java 7 中使用的是 “数组 + 链表”，发生散列冲突的键值对会用头插法添加到单链表中；
-   2、HashMap 在 Java 8 中使用的是 “数组 + 链表 + [红黑树](https://www.zhihu.com/search?q=%E7%BA%A2%E9%BB%91%E6%A0%91&search_source=Entity&hybrid_search_source=Entity&hybrid_search_extra=%7B%22sourceType%22%3A%22answer%22%2C%22sourceId%22%3A2784386555%7D)”，发生散列冲突的键值对会用尾插法添加到单链表中。如果链表的长度大于 8 时且散列表容量大于 64，会将链表树化为红黑树。

`HashMap 实现示意图`

![](https://pic1.zhimg.com/50/v2-93e1d142a832b0cbdf7064269431c3ad_720w.jpg?source=1940ef5c)

### **1.2 说一下 LinkedHashMap 的实现结构**

LinkedHashMap 是继承于 HashMap 实现的哈希链表。

-   1、LinkedHashMap 同时具备[双向链表](https://www.zhihu.com/search?q=%E5%8F%8C%E5%90%91%E9%93%BE%E8%A1%A8&search_source=Entity&hybrid_search_source=Entity&hybrid_search_extra=%7B%22sourceType%22%3A%22answer%22%2C%22sourceId%22%3A2784386555%7D)和散列表的特点。当 LinkedHashMap 作为散列表时，主要体现出 O(1) 时间复杂度的查询效率。当 LinkedHashMap 作为双向链表时，主要体现出有序的特性；
-   2、LinkedHashMap 支持 FIFO 和 LRU 两种排序模式，默认是 FIFO 排序模式，即按照插入顺序排序。Android 中的 LruCache 内存缓存和 DiskLruCache [磁盘缓存](https://www.zhihu.com/search?q=%E7%A3%81%E7%9B%98%E7%BC%93%E5%AD%98&search_source=Entity&hybrid_search_source=Entity&hybrid_search_extra=%7B%22sourceType%22%3A%22answer%22%2C%22sourceId%22%3A2784386555%7D)也是直接复用 LinkedHashMap 提供的缓存管理能力。

`LinkedHashMap 示意图`

![](https://picx.zhimg.com/50/v2-db2d7346a09a3febedda1ef3a143aadf_720w.jpg?source=1940ef5c)

___

### **2.1 WeakReference [弱引用](https://www.zhihu.com/search?q=%E5%BC%B1%E5%BC%95%E7%94%A8&search_source=Entity&hybrid_search_source=Entity&hybrid_search_extra=%7B%22sourceType%22%3A%22answer%22%2C%22sourceId%22%3A2784386555%7D)的特点**

WeakHashMap 中的 “Weak” 指键 Key 是弱引用，也叫弱键。弱引用是 Java 四大引用类型之一，一共有四种引用类型，分别是强引用、软引用、弱引用和虚引用。我将它们的区别概括为 3 个维度：

-   **维度 1 - 对象可达性状态的区别：** 强引用指向的对象是强可达的，只有强可达的对象才会认为是存活的对象，才能保证在[垃圾收集](https://www.zhihu.com/search?q=%E5%9E%83%E5%9C%BE%E6%94%B6%E9%9B%86&search_source=Entity&hybrid_search_source=Entity&hybrid_search_extra=%7B%22sourceType%22%3A%22answer%22%2C%22sourceId%22%3A2784386555%7D)的过程中不会被回收；
-   **维度 2 - [垃圾回收](https://www.zhihu.com/search?q=%E5%9E%83%E5%9C%BE%E5%9B%9E%E6%94%B6&search_source=Entity&hybrid_search_source=Entity&hybrid_search_extra=%7B%22sourceType%22%3A%22answer%22%2C%22sourceId%22%3A2784386555%7D)策略的区别：** 不同的引用类型的回收激进程度不同，

-   强引用指向的对象不会被回收；
-   软引用指向的对象在内存充足时不会被回收，在内存不足时会被回收；
-   弱引用和虚引用指向的对象无论在内存是否充足的时候都会被回收；

  
-   **维度 3 - 感知垃圾回收时机：** 当引用对象关联的实际对象被垃圾回收时，引用对象会进入关联的引用队列，程序可以通过观察引用队列的方式，感知对象被垃圾回收的时机。

`感知垃圾回收示意图`

![](https://picx.zhimg.com/50/v2-3b543c3e3afbe99c0efa841d25826f85_720w.jpg?source=1940ef5c)

> **提示：** 关于 “Java 四种引用类型” 的区别，在小彭的 Java 专栏中深入讨论过 **[《说一下 Java 的四种引用类型》](https://link.zhihu.com/?target=https%3A//mp.weixin.qq.com/s/PM5HKygFIlBVbYFnH3QUuw)**，去看看。  

### **2.2 WeakHashMap 的特点**

**WeakHashMap 是使用弱键的动态散列表，用于实现 “自动清理” 的内存缓存。**

-   1、WeakHashMap 使用与 Java 7 HashMap 相同的 “数组 + 链表” 解决散列冲突，发生散列冲突的键值对会用头插法添加到单链表中；  
    
-   2、WeakHashMap 依赖于 Java 垃圾收集器自动清理不可达对象的特性。当 Key 对象不再被持有强引用时，垃圾收集器会按照弱引用策略自动回收 Key 对象，并在下次访问 WeakHashMap 时清理全部无效的键值对。因此，WeakHashMap 特别适合实现 “自动清理” 的内存活动缓存，当键值对有效时保留，在键值对无效时自动被垃圾收集器清理；  
    
-   3、需要注意，因为 WeakHashMap 会持有 Value 对象的强引用，所以在 Value 对象中一定不能持有 key 的强引用。否则，会阻止垃圾收集器回收 “本该不可达” 的 Key 对象，使得 WeakHashMap 失去作用。  
    
-   4、与 HashMap 相同，LinkedHashMap 也不考虑[线程同步](https://www.zhihu.com/search?q=%E7%BA%BF%E7%A8%8B%E5%90%8C%E6%AD%A5&search_source=Entity&hybrid_search_source=Entity&hybrid_search_extra=%7B%22sourceType%22%3A%22answer%22%2C%22sourceId%22%3A2784386555%7D)，也会存在线程安全问题。可以使用 Collections.synchronizedMap 包装类，其原理也是在所有方法上增加 synchronized 关键字。  
    

`WeakHashMap 示意图`

![](https://picx.zhimg.com/50/v2-b88551923026fc3befa926572ca55c40_720w.jpg?source=1940ef5c)

`自动清理数据`

![](https://picx.zhimg.com/50/v2-8325e029eac4ce4d1916ada8bacd23e1_720w.jpg?source=1940ef5c)

### **2.3 说一下 WeakHashMap 与 HashMap 和 LinkedHashMap 的区别？**

WeakHashMap 与 HashMap 都是基于分离链表法解决散列冲突的动态散列表，两者的主要区别在 **键 Key 的引用类型上：**

-   HashMap 会持有键 Key 的强引用，除非手动移除，否则键值对会长期存在于散列表中；  
    
-   WeakHashMap 只持有键 Key 的弱引用，当 Key 对象不再被外部持有强引用时，键值对会被自动被清理。  
    

WeakHashMap 与 LinkedHashMap 都有自动清理的能力，两者的主要区别在于 **淘汰数据的策略上：**

-   LinkedHashMap 会按照 FIFO 或 LRU 的策略 “尝试” 淘汰数据，需要[开发者](https://www.zhihu.com/search?q=%E5%BC%80%E5%8F%91%E8%80%85&search_source=Entity&hybrid_search_source=Entity&hybrid_search_extra=%7B%22sourceType%22%3A%22answer%22%2C%22sourceId%22%3A2784386555%7D)重写 `removeEldestEntry()` 方法实现是否删除最早节点的判断逻辑；  
    
-   WeakHashMap 会按照 Key 对象的可达性淘汰数据，当 Key 对象不再被持有强引用时，会自动清理无效数据。  
    

### **2.4 重建 Key 对象不等价的问题**

WeakHashMap 的 Key 使用弱引用，也就是以 Key 作为清理数据的判断锚点，当 Key 变得不可达时会自动清理数据。此时，如果使用多个 `equals` 相等的 Key 对象访问键值对，就会出现第 1 个 Key 对象不可达导致键值对被回收，而第 2 个 Key 查询键值对为 null 的问题。 **这说明 `equals` 相等的 Key 对象在 HashMap 等散列表中是等价的，但是在 WeakHashMap 散列表中是不等价的。**

因此，如果 Key 类型没有重写 equals 方法，那么 WeakHashMap 就表现良好，否则会存在歧义。例如下面这个 Demo 中，首先创建了指向 `image_url1` 的图片 Key1，再重建了同样指向 `image_url1` 的图片 Key2。在 HashMap 中，Key1 和 Key2 等价，但在 WeakHashMap 中，Key1 和 Key2 不等价。

`Demo`

```
class ImageKey {
    private String url;

    ImageKey(String url) {
        this.url = url;
    }

    public boolean equals(Object obj) {
        return (obj instanceOf ImageKey) && Objects.equals(((ImageKey)obj).url, this.url);
    }
}

WeakHashMap<ImageKey, Bitmap> map = new WeakHashMap<>();
ImageKey key1 = new ImageKey("image_url1");
ImageKey key2 = new ImageKey("image_url2");
// key1 equalsTo key3
ImageKey key3 = new ImageKey("image_url1");

map.put(key1, bitmap1);
map.put(key2, bitmap2);

System.out.println(map.get(key1)); // 输出 bitmap1
System.out.println(map.get(key2)); // 输出 bitmap2
System.out.println(map.get(key3)); // 输出 bitmap1

// 使 key1 不可达，key3 保持
key1 = null;

// 说明重建 Key 与原始 Key 不等价
System.out.println(map.get(key1)); // 输出 null
System.out.println(map.get(key2)); // 输出 bitmap2
System.out.println(map.get(key3)); // 输出 null
```

默认的 `Object#equals` 是判断两个变量是否指向同一个对象：

`Object.java`

```
public boolean equals(Object obj) {
    return (this == obj);
}
```

### **2.5 Key 弱引用和 Value 弱引用的区别**

不管是 Key 还是 Value 使用弱引用都可以实现自动清理，至于使用哪一种方法各有优缺点，适用场景也不同。

-   **Key 弱引用：** 以 Key 作为清理数据的判断锚点，当 Key 不可达时清理数据。优点是容器外不需要持有 Value 的强引用，缺点是重建的 Key 与原始 Key 不等价，重建 Key 无法阻止数据被清理；  
    
-   **Value 弱引用：** 以 Value 作为清理数据的判断锚点，当 Value 不可达时清理数据。优点是重建 Key 与与原始 Key 等价，缺点是容器外需要持有 Value 的强引用。  
    

| 类型 | 优点 | 缺点 | 场景 |
| --- | --- | --- | --- |
| Key 弱引用 | 外部不需要持有 Value 的强引用，使用更简单 | 重建 Key 不等价 | 未重写 equals |
| Value 弱引用 | 重建 Key 等价 | 外部需要持有 Value 的强引用 | 重写 equals |

**举例 1：** 在 Android Glide 图片框架的多级缓存中，因为图片的 EngineKey 是可重建的，存在多个 EngineKey 对象指向同一个图片 Bitmap，所以 Glide 最顶层的活动缓存采用的是 Value 弱引用。

`EngineKey.java`

```
class EngineKey implements Key {

    // 重写 equals
    @Override
    public boolean equals(Object o) {
        if (o instanceof EngineKey) {
            EngineKey other = (EngineKey) o;
            return model.equals(other.model)
                && signature.equals(other.signature)
                && height == other.height
                && width == other.width
                && transformations.equals(other.transformations)
                && resourceClass.equals(other.resourceClass)
                && transcodeClass.equals(other.transcodeClass)
                && options.equals(other.options);
        }
        return false;
    }
}
```

**举例 2：** 在 ThreadLocal 的 ThreadLocalMap 线程本地存储中，因为 ThreadLocal 没有重写 equals，不存在多个 ThreadLocal 对象指向同一个键值对的情况，所以 ThreadLocal 采用的是 Key 弱引用。

`ThreadLocal.java`

```
static class Entry extends WeakReference<ThreadLocal<?>> {
    /** The value associated with this ThreadLocal. */
    Object value;

    Entry(ThreadLocal<?> k, Object v) {
        super(k);
        value = v;
    }
    
    // 未重写 equals
}
```

___

## **3. WeakHashMap 源码分析**

这一节，我们来分析 WeakHashMap 中主要流程的源码。

事实上，WeakHashMap 就是照着 Java 7 版本的 HashMap [依葫芦画瓢](https://www.zhihu.com/search?q=%E4%BE%9D%E8%91%AB%E8%8A%A6%E7%94%BB%E7%93%A2&search_source=Entity&hybrid_search_source=Entity&hybrid_search_extra=%7B%22sourceType%22%3A%22answer%22%2C%22sourceId%22%3A2784386555%7D)的，没有树化的逻辑。考虑到我们已经对 HashMap 做过详细分析，所以我们没有必要重复分析 WeakHashMap 的每个细节，而是把重心放在 WeakHashMap 与 HashMap 不同的地方。

### **3.1 WeakHashMap 的属性**

先用一个表格整理 WeakHashMap 的属性：

| 版本 | 数据结构 | 节点实现类 | 属性 |
| --- | --- | --- | --- |
| Java 7 HashMap | 数组 + 链表 | Entry（单链表） | 1、table（数组）  
2、size（尺寸）  
3、threshold（扩容阈值）  
4、loadFactor（装载因子上限）  
5、modCount（修改计数）  
6、默认数组容量 16  
7、最大数组容量 2^30  
8、默认负载因子 0.75 |
| WeakHashMap | 数组 + 链表 | Entry（单链表，弱引用的子类型） | 9、queue（引用队列） |

`WeakHashMap.java`

```
public class WeakHashMap<K,V> extends AbstractMap<K,V> implements Map<K,V> {

    // 默认数组容量
    private static final int DEFAULT_INITIAL_CAPACITY = 16;

    // 数组最大容量：2^30（高位 0100，低位都是 0）
    private static final int MAXIMUM_CAPACITY = 1 << 30;

    // 默认装载因子上限：0.75
    private static final float DEFAULT_LOAD_FACTOR = 0.75f;

    // 底层数组
    Entry<K,V>[] table;

    // 键值对数量
    private int size;

    // 扩容阈值（容量 * 装载因子）
    private int threshold;

    // 装载因子上限
    private final float loadFactor;

    // 引用队列
    private final ReferenceQueue<Object> queue = new ReferenceQueue<>();

    // 修改计数
    int modCount;

    // 链表节点（一个 Entry 等于一个键值对）
    private static class Entry<K,V> extends WeakReference<Object> implements Map.Entry<K,V> {
        // Key：与 HashMap 和 LinkedHashMap 相比，少了 key 的强引用
        // final K key;
        // Value（强引用）
        V value;
        // 哈希值
        final int hash;
        Entry<K,V> next;

        Entry(Object key, V value, ReferenceQueue<Object> queue, int hash, Entry<K,V> next) {
            super(key /*注意：只有 Key 是弱引用*/, queue);
            this.value = value;
            this.hash  = hash;
            this.next  = next;
        }
    }
}
```

WeakHashMap 与 HashMap 的属性几乎相同，主要区别有 2 个：

-   **1、ReferenceQueue：** WeakHashMap 的属性里多了一个 queue 引用队列；  
    
-   **2、Entry：** `WeakHashMap#Entry` 节点继承于 `WeakReference`，表面看是 WeakHashMap 持有了 Entry 的强引用，其实不是。注意看 Entry 的构造方法，WeakReference 关联的实际对象是 Key。 **所以，WeakHashMap 依然持有 Entry 和 Value 的强引用，仅持有 Key 的弱引用。**  
    

`引用关系示意图`

![](https://pic1.zhimg.com/50/v2-9c8f26664756220a1ec56c639e4e198a_720w.jpg?source=1940ef5c)

不出意外的话又有小朋友出来举手提问了 **♀️**：

-   ♀️**疑问 1：说一下 ReferenceQueue queue 的作用?**

ReferenceQueue 与 Reference 配合能够实现感知对象被垃圾回收的能力。在创建引用对象时可以关联一个实际对象和一个引用队列，当实现对象被垃圾回收后，引用对象会被添加到这个引用队列中。在 WeakHashMap 中，就是根据这个引用队列来自动清理无效键值对。

-   ♀️**疑问 2：为什么 Key 是弱引用，而不是 Entry 或 Value 是弱引用？**

首先，Entry 一定要持有强引用，而不能持有弱引用。这是因为 Entry 是 WeakHashMap 内部维护数据结构的实现细节，并不会暴露到 WeakHashMap 外部，即除了 WeakHashMap 本身之外没有其它地方持有 Entry 的强引用。所以，如果持有 Entry 的弱引用，即使 WeakHashMap 外部依然在使用 Key 对象，WeakHashMap 内部依然会回收键值对，这与预期不符。

其次，不管是 Key 还是 Value 使用弱引用都可以实现自动清理。至于使用哪一种方法各有优缺点，适用场景也不同，这个在前文分析过了。

### **3.2 WeakHashMap 如何清理无效数据？**

在通过 put / get / size 等方法访问 WeakHashMap 时，其内部会调用 `expungeStaleEntries()` 方法清理 Key 对象已经被回收的无效键值对。其中会遍历 ReferenceQueue 中持有的弱引用对象（即 Entry 节点），并将该结点从散列表中移除。

```
private final ReferenceQueue<Object> queue = new ReferenceQueue<>();

// 添加键值对
public V put(K key, V value) {
    ...
    // 间接 expungeStaleEntries()
    Entry<K,V>[] tab = getTable();
    ...
}

// 扩容
void resize(int newCapacity) {
    // 间接 expungeStaleEntries()
    Entry<K,V>[] oldTable = getTable();
    ...
}

// 获取键值对
public V get(Object key) {
    ...
    // 间接 expungeStaleEntries()
    Entry<K,V>[] tab = getTable();
    ...
}

private Entry<K,V>[] getTable() {
    // 清理无效键值对
    expungeStaleEntries();
    return table;
}

// ->清理无效键值对
private void expungeStaleEntries() {
    // 遍历引用队列
    for (Object x; (x = queue.poll()) != null; ) {
        // 疑问 3：既然 WeakHashMap 不考虑线程同步，为什么这里要做加锁，岂不是突兀？
        synchronized (queue) {
            Entry<K,V> e = (Entry<K,V>) x;
            // 根据散列值定位数组下标
            int i = indexFor(e.hash /*散列值*/, table.length);
            // 遍历桶寻找节点 e 的前驱结点
            Entry<K,V> prev = table[i];
            Entry<K,V> p = prev;
            while (p != null) {
                Entry<K,V> next = p.next;
                if (p == e) {
                    // 删除节点 e
                    if (prev == e)     
                        // 节点 e 是根节点
                        table[i] = next;
                    else
                        // 节点 e 是中间节点
                        prev.next = next;
                    // Must not null out e.next;
                    // stale entries may be in use by a HashIterator
                    e.value = null; // Help GC
                    size--;
                    break;
                }
                prev = p;
                p = next;
            }
        }
    }
}
```

___

## **4. 总结**

-   1、WeakHashMap 使用与 Java 7 HashMap 相同的 “数组 + 链表” 解决散列冲突，发生散列冲突的键值对会用头插法添加到单链表中；  
    
-   2、WeakHashMap 能够实现 “自动清理” 的内存缓存，其中的 “Weak” 指键 Key 是弱引用。当 Key 对象不再被持有强引用时，垃圾收集器会按照弱引用策略自动回收 Key 对象，并在下次访问 WeakHashMap 时清理全部无效的键值对；  
    
-   3、WeakHashMap 和 LinkedHashMap 都具备 “自动清理” 的 能力，WeakHashMap 根据 Key 对象的可达性淘汰数据，而 LinkedHashMap 根据 FIFO 或 LRU 策略尝试淘汰数据；  
    
-   4、WeakHashMap 使用 Key 弱引用，会存在重建 Key 对象不等价问题。  
    

___

## **小彭的 Android 交流群 02 群**
