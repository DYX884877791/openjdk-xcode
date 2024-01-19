---
source: https://blog.csdn.net/qq502130297/article/details/121067380?ops_request_misc=%257B%2522request%255Fid%2522%253A%2522168974081616800211582750%2522%252C%2522scm%2522%253A%252220140713.130102334.pc%255Fall.%2522%257D&request_id=168974081616800211582750&biz_id=0&utm_medium=distribute.pc_search_result.none-task-blog-2~all~first_rank_ecpm_v1~rank_v31_ecpm-8-121067380-null-null.142^v89^control_2,239^v2^insert_chatgpt&utm_term=ReferenceQueue&spm=1018.2226.3001.4187
---
## ReferenceQueue理解

在检测到适当的可达性更改后，垃圾收集器会将注册的引用对象附加到该[队列](https://so.csdn.net/so/search?q=%E9%98%9F%E5%88%97&spm=1001.2101.3001.7020)中。  
这句话什么意思呢？

```
ReferenceQueue<Object> queue = new ReferenceQueue<>();



byte[] key = new byte[1024*10];
WeakReference<byte[]> reference = new WeakReference<>(key,queue);

//当垃圾回收之后实际上会将reference对象放进引用队列中。
//而key就是也就是byte数组对象回收掉。通常引用队列就是作为一个通知的信号，表明这个对象被回收掉了。

```

验证代码 这里设置堆的大小为-Xmx20m （20m）

```
@Test
public void test() throws InterruptedException {
    ReferenceQueue<Object> queue = new ReferenceQueue<>();
    
    new Thread(()->{
        HashMap<Object,Object> map = new HashMap<>();
        for(int i=0;i<100;i++){
            WeakReference<byte[]> reference = new WeakReference<>(new byte[1024*100],queue);
            map.put(reference,"a");
        }
        System.out.println(map.size());
    }).start();

    new Thread(()->{
        int cnt = 0;
        WeakReference<byte[]> k;
        try {
        //ReferenceQueue.remove是阻塞的。poll()方法是不阻塞的。
            while((k = (WeakReference) queue.remove()) != null) {
                System.out.println("byte对象地址" + k.get());
                System.out.println("WeakReference的地址" + k);
            }
        }catch (Exception e){

        }
    }).start();
    Thread.sleep(2000);
}
```

测试结果  
WeakReference的地址java.lang.ref.WeakReference@6930026c  
byte对象地址null  
WeakReference的地址java.lang.ref.WeakReference@561e343  
byte对象地址null

map.size()=100

实际上只有map数组中key对象中的byte数组被回收掉了。

**HashMap<Object,WeakReference<byte[]>>与WeakHashMap()的区别**  
主要有两点  
1、通过map.size()=100我们了解到，前者key中的byte数组被回收掉，当并不会移除掉在map中的值。而WeakHashMap()则会移除掉  
2、在WeakHashMap()实际上是将整个Entry<K,V>继承了WeakReference  
也就是说传进去的K，V对象会被GC掉。而这个Entry节点本身不会受到影响，可以理解为它的两个属性被回收掉了。

```
private static class Entry<K,V> extends WeakReference<Object> implements Map.Entry<K,V> {...}
```

那WeakHashMap()是怎么样清理掉这些Entry节点（KV被GC掉的）  
这就用到了本文的标题ReferenceQueue。  
我们到知道一旦发生GC那些KV被GC掉的Entry对象就会被放进ReferenceQueue。而我们就可以遍历这个队列来移除掉Map中的值。

**WeakHashMap**部分源码

```
public class WeakHashMap<K,V> extends AbstractMap<K,V> implements Map<K,V> {
...
private final ReferenceQueue<Object> queue = new ReferenceQueue<>();
...

//重点

    /**
     * Expunges stale entries from the table.
     * 从表中删除陈旧的条目。
     */
    private void expungeStaleEntries() {
    //需要注意queue.poll()是不阻塞的,remove(long time)方法是阻塞的
        for (Object x; (x = queue.poll()) != null; ) {
            synchronized (queue) {
                @SuppressWarnings("unchecked")
                    Entry<K,V> e = (Entry<K,V>) x;
                int i = indexFor(e.hash, table.length);

                Entry<K,V> prev = table[i];
                Entry<K,V> p = prev;
                //同时在这里我们也可以看出WeakHashMap底层采用数组+链表并未使用红黑树
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
}
```

**ReferenceQueue**的remove与poll方法

```
//并未阻塞
public Reference<? extends T> poll() {
  if (head == null)
         return null;
     synchronized (lock) {
         return reallyPoll();
     }
}

//通过lock.wait(timeout);阻塞调用remove方法的线程
public Reference<? extends T> remove(long timeout)
throws IllegalArgumentException, InterruptedException
{
    ...
    synchronized (lock) {
        Reference<? extends T> r = reallyPoll();
        if (r != null) return r;
        long start = (timeout == 0) ? 0 : System.nanoTime();
        for (;;) {
            lock.wait(timeout);
            r = reallyPoll();
            if (r != null) return r;
            if (timeout != 0) {
                long end = System.nanoTime();
                timeout -= (end - start) / 1000_000;
                if (timeout <= 0) return null;
                start = end;
            }
        }
    }
}
```
