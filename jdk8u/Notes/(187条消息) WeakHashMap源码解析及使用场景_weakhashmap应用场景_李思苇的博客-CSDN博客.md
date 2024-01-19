---
source: https://blog.csdn.net/wenxindiaolong061/article/details/109290742?ops_request_misc=&request_id=&biz_id=102&utm_term=weakhashmap&utm_medium=distribute.pc_search_result.none-task-blog-2~all~sobaiduweb~default-9-109290742.142^v90^chatsearch,239^v3^control&spm=1018.2226.3001.4187
---
## 目的

让Map中不再使用的Entry被GC及时回收，释放内存空间

## 应用场景-缓存

应用场景：Map本身[生命周期](https://so.csdn.net/so/search?q=%E7%94%9F%E5%91%BD%E5%91%A8%E6%9C%9F&spm=1001.2101.3001.7020)很长，需要长期贮留内存中，但Map中的Entry可以删除，使用时可以从其它地方再次取得。  
实例：tomcat中的缓存有用到。

## 源码解析

先理解[HashMap源码](https://blog.csdn.net/wenxindiaolong061/article/details/105998634)  
以及WeakReference：[弱引用——WeakReference——所引用的对象的回收规则](https://blog.csdn.net/wenxindiaolong061/article/details/107193637)

WeahHashMap与HashMap在代码实现上的不同点：

1.  WeahHashMap类的 Entry<K, V> 继承了 WeakReference，并设置 referent = key。
2.  WeakHashMap类内定义了一个ReferenceQueue refQueue，每个entry创建时，都会绑定这个refQueue，当GC清理了entry的 referent 后，也就是说entry与自己的key断开引用了，会将entry入队到其绑定的 refQueue中去。 WeahkHashMap类内的任何操作执行前（如：get / size 等操作），都会先检查一遍这个refQueue，将已经被GC断开了对key的引用的entry全都从map中remove掉。

```
private final ReferenceQueue<Object> queue = new ReferenceQueue<>();// 定义一个队列，GC会自动将绑定到了此队列的weak实例入队到此队列，用户应当在每次访问weak实例前，都要检查实例是否已经被GC入队到此队列中，如果是，说明实例已经被GC，应当放弃使用。
...
private static class Entry<K,V> extends WeakHashMap<Object> implements Map.Entry<K,V>{
V value;
int hash;
Entry<K,V> next;

Entry(Object key, V value, ReferenceQueue queue, int hash, Entry<K,V> next){
super(key, queue);// 调用父类WeakReference的构造方法，设置referent = key, queue = refQueue;
this.value = value; 
this.hash = hash;
this.next = next;
}
}
```

具体如下：

1.  在WeakHashMap类中定义了一个实例域ReferenceQueue<Map.Entry> queue。

```
   /**
     * Reference queue for cleared WeakEntries
     */
    private final ReferenceQueue<Object> queue = new ReferenceQueue<>();
```

2.  定义了一个内部类WeakHashMap.Entry，直接继承了WeakReference，Entry中没有定义key字段，而是调用super(key,queue)，将 key 保存在Reference类的referent字段中。
3.  由于Entry本身对key是弱引用，因此GC会监测key，在某个Entry的key处于适当状态时，Entry会被加入到pending列表，然后由ReferenceHandler将Entry添加到queue队列。
4.  WeakHashMap中的许多操作，比如get(K key),size(),remove(K key)时，都会先调用expungeStaleEntries();方法，这个方法会将已经被添加到queue中的Entry从map中移除，同时会将entry的value变量的值置为null。
5.  经过步骤4，entry被从Map中移除后，不再有对此entry的引用，entry对key即referent的引用是弱引用，entry的value的值被赋值为null，原来的value的对象也不再被引用。GC就可以回收这些对象了。

## 代码详解

1.  自定义的内部类Entry<K,V>，实现了Map.Entry<K,V>，同时继承了WeakReference。其referent指向key。也就是说，WeakHahsMap中的每个Entry都是一个weakRefer实例。

可以看到代码中没有定义实例域key,而是调用WeakReference的构造函数super(key,queue)，使得weakRefer实例的referent变量指向了key。

Entry的getKey()方法，就是调用WeakReference的get()方法，返回referent引用的key。

put(k, v)方法执行，构造Entry时，会将给定的key赋值给referent。

get( k) 方法执行时，根据 k.hash == entry.hash && k.equals(entry.get()) 来比较和查找，其中entry.get()得到的就是referent引用的key。

```
private static class Entry<K,V> extends WeakReference<Object> implements Map.Entry<K,V> {

 V value;
        final int hash;
        Entry<K,V> next;

        /**
         * Creates new entry.
         */
        Entry(Object key, V value,
              ReferenceQueue<Object> queue,
              int hash, Entry<K,V> next) {
            super(key, queue);
            this.value = value;
            this.hash  = hash;
            this.next  = next;
        }

@SuppressWarnings("unchecked")
        public K getKey() {
            return (K) WeakHashMap.unmaskNull(get());
        }

        public int hashCode() {      /* 重写实现了Object类中的hashCode，此方法是计算整个Entry实例对象的hashCode，不是计算key的hashCode */
            K k = getKey();
            V v = getValue();
            return Objects.hashCode(k) ^ Objects.hashCode(v);
        }
}
```

3.  类中定义了一个声明的同时也初始化了的ReferenceQueue类型的变量： `private final ReferenceQueue<Object> queue = new ReferenceQueue<>();`  
    WeakHashMap中的所有Entry的key都会在super(key,queue)时，注册到此queue上。  
    GC线程会监测这些key的可达性的状态，在key处于一个特殊状态时，就会将引用key的WeakReference实例对象的状态设置为pending，并将WeakReference实例添加到pengding列表中去。  
    而Reference类创建的ReferenceHandler线程则会自旋处理pending列表中的所有处于pending状态的Reference实例，将它们enqueue()到queue中去，  
    最终GC会回收queue里的所有Reference实例，由于是Entry实现了WeakReference，因此最终是整个entry被回收。
4.  获取WeakHashMap的table[]数组时，会将已经被GC入队的key关联的entry从map中删除。

```
private Entry<K,V>[] getTable(){
Entry<K,V>[] table = expungeStaleEntries();
return table;
}

/**
     * Expunges stale entries from the table.
     */
    private void expungeStaleEntries() {
        for (Object x; (x = queue.poll()) != null; ) {
            synchronized (queue) {
                @SuppressWarnings("unchecked")
                    Entry<K,V> e = (Entry<K,V>) x;
                int i = indexFor(e.hash, table.length);

                Entry<K,V> prev = table[i];
                Entry<K,V> p = prev;
                while (p != null) {
                    Entry<K,V> next = p.next;
                    if (p == e) {
                        if (prev == e)
                            table[i] = next;
                        else
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

4.  get(K key)时，会调用Reference的get()获得Entry真正的key，与参数key做比较

```
   public V get(Object key) {
        Object k = maskNull(key);
        int h = hash(k);
        Entry<K,V>[] tab = getTable();
        int index = indexFor(h, tab.length);
        Entry<K,V> e = tab[index];
        while (e != null) {
            if (e.hash == h && eq(k, e.get()))
                return e.value;
            e = e.next;
        }
        return null;
    }
```

## 实例解析：[WeakHashMap](https://so.csdn.net/so/search?q=WeakHashMap&spm=1001.2101.3001.7020)在tomcat的缓存中的应用

```
public final class ConcurrentCache<K,V>{

private  final int size;

private  final Map<K,V> eden;    //新创建的，最近使用的，放在eden里。
private final Map<K,V> longTerm;    // 当eden满了后，将eden里的所有对象移动到longTerm里。longTerm是一个WeakHashMap，GC及时清理其中的数据。

public ConcurrentCache(int  size){
this.size=size;
eden= new ConcurrentHashMap(size);
longTerm = new WeakHashMap();
}

public V get(K k){     /* 被get，最新被使用了，必须要在eden中*/
V v = eden.get(k) ;
if(v==null){
synchronized(longTerm){
  v = longTerm.get(k);
}
if(v!=null){
eden.put(k,v);
}    
}
return v;
}

public V put(K K,V v){   /*最新创建的，放到eden中*/
if(eden.size()>=size){
synchronized(longTerm){
longTerm.putAll(eden);
}
eden.clear();
}
eden.put(k,v);

}
}
```

get时，如果eden中没有，而longTerm中有，则将数据取出后，再添加到eden中，保证最新最近使用的放在eden中。  
put时，如果eden已经满了，就将eden中的全部倒换到longTerm中去，将新创建的这个要put到eden中。  
如此，longTerm中就是长期未使用的、不常用的，因此用WeakHashMap以便GC回收，释放空间。  
缓存使用 ConcurrentHashMap 和 synchronized(longTerm) 很简单地实现了多线程安全的缓存。
