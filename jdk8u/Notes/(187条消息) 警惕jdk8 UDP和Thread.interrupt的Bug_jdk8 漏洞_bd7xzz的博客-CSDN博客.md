---
source: https://bd7xzz.blog.csdn.net/article/details/128125053
---
### 背景

线上业务在热点流量大的情况下（业务采用Java编程语言实现），单机偶发出现Hystrix熔断，接口无法提供服务。如下图所示：  
![在这里插入图片描述](https://img-blog.csdnimg.cn/5f585542ee824430bd14323887d1e59e.png)

```
Hystrix circuit short-circuited and is OPEN 代表Hystrix在一个窗口时间内失败了N次，发生短路。短路时间默认为 5 秒，5秒之内拒绝所有的请求，之后开始运行。
```

![在这里插入图片描述](https://img-blog.csdnimg.cn/1f46ed4b0def4fddba326cb1410a6de8.png)

统计了一下1个小时内居然有5w多条。  
最初怀疑依赖下游的某个rpc接口出现问题，但从监控来看，异常时间点多个rpc接口异常率都有所上升。所以考虑是自身单机存在什么问题，导致调用依赖的rpc接口失败，产生了大量的熔断。

这里还有很重要的一点：**在单机出现问题第一时间，摘掉了流量，过半小时后恢复流量，依旧熔断无法恢复**

-   若摘掉流量后，静置一段时间放开流量，服务恢复，证明可能是由于流量过载导致的问题，如：JVM内存不够，gc回收不过来，频繁gc。这样的话流量掐掉后，gc一段时间后内存够用，服务恢复正常。
-   若摘掉流量后，静置一段时间放开流量，服务依旧熔断，在单机出现问题的场景下，很可能是JVM内部的问题（包括应用自身问题或者JVM的一些问题，如：死锁）

### 排查

通常排查这种自身问题可从几个方面入手：

1.  CPU load、使用率、内存使用率是否过高
2.  网卡出入速率是否有抖动，包括TCP重传
3.  磁盘IOPS是否过高，导致一些sys call阻塞
4.  JVM是否出现stw动作，包括不限于：gc、JIT等
5.  是否有oom、stack overflow、死锁等现象
6.  代码出现bug
7.  虚拟化对应用进行stw动作

根据这些逐一分析：

1.  监控CPU和内存如下图所示：CPU有明显的抖动，内存使用率正常（18:30断崖式下滑是对服务进行了上线，可忽略不管），注意问题发生在19:00以后，可以看到CPU在出问题时间段内有明显的波动。  
    ![在这里插入图片描述](https://img-blog.csdnimg.cn/dd24055f40944f1a8127149581124d77.png)
    
2.  查看监控网卡如下图所示：在19:00之后出问题的时间段网卡出入速率略有波动，可忽略。  
    ![在这里插入图片描述](https://img-blog.csdnimg.cn/53a1be6090cf41d987a790db2748a5ab.png)
    
3.  查看磁盘IOS如下图所示：磁盘IOPS虽然比较高，但19:00前后都比较平均。  
    ![在这里插入图片描述](https://img-blog.csdnimg.cn/25aea1f6e6db4dd6a0a872f102528503.png)
    
4.  查看gclog如下图所示：虽然young gc比较频繁，但回收的比例还是比较大，耗时也不高。没有明显的其他原因导致stw占用时间长的迹象。  
    ![在这里插入图片描述](https://img-blog.csdnimg.cn/5611adc35da648599059c9ba9a6ec31b.png)
    
5.  打印线程栈如下图所示：jstack打出的线程栈就很明显了，最下面直接显示了死锁！  
    ![在这里插入图片描述](https://img-blog.csdnimg.cn/3e74705b1cb3455f896d7ca77527f015.png)
    
6.  review了最近上线的代码，改动很小没有问题。
    
7.  由于打印线程栈出现死锁，问题很明显，不用排查虚拟化了。
    

问题很明显了，JVM出现了死锁，导致Hystirx熔断。  
从死锁的日志来看，似乎跟DNS相关。难道Java的DNS有啥Bug？

### 分析

![在这里插入图片描述](https://img-blog.csdnimg.cn/b24184c728ed40059f2ff57a17582ffa.png)

在死锁线程栈中很明显地标记出了 `pool-486-thread-132` 在等 `0x00000006b77f2098` 这把锁，  
并且这把锁被 `hstirx-***` 线程持有着，同时 `hstirx-***` 在等待 `0x00000006b77f2128` 这把锁，并且这把锁被 `pool-486-thread-132` 线程持有着。  
![在这里插入图片描述](https://img-blog.csdnimg.cn/9d01eaf00e624e849a7bba10c450c664.png)

这俩个线程栈中同时出现了DNS和DatagramChannel相关的类，证明死锁很大一部分原因是由DNS在发UDP包的情况下产生了死锁。我们知道死锁是俩个线程对相同临界区资源竞争时，加锁不当产生的。复习下死锁的四个必要条件：

1.  互斥条件：进程要求对所分配的资源进行排它性控制，即在一段时间内某资源仅为一进程所占用。
2.  请求和保持条件：当进程因请求资源而阻塞时，对已获得的资源保持不放。
3.  不剥夺条件：进程已获得的资源在未使用完之前，不能剥夺，只能在使用完时由自己释放。
4.  环路等待条件：在发生死锁时，必然存在一个进程–资源的环形链。

那我们使用的DNS为啥会出现这样的问题？难道是JDK或者是dnsjava（我们使用了dnsjava）的bug？  
排查bug的最好方法就是阅读源码，我们可以通过死锁的线程栈自底向上逐层跟踪代码。

先看 `histirx-***`的死锁线程栈 ：  
![在这里插入图片描述](https://img-blog.csdnimg.cn/ef0fc5e112c9436bad7f7ac30bf7763b.png)

1.  从业务调用点开始，这里我们的业务向BatchExecutor（公司内部封装的线程池操作）提交了线程  
    ![在这里插入图片描述](https://img-blog.csdnimg.cn/c8cb8aa368a9402491ccaa3e0dbaa4a2.png)
    
2.  本质上调用AbstractExecutorService.invokeAll方法，该方法在线程池中的线程执行结束时，finally会触发FutureTask.cancel方法  
    ![在这里插入图片描述](https://img-blog.csdnimg.cn/ce659a0ba4cd479a975f55b8ebbaf696.png)  
    ![[外链图片转存失败,源站可能有防盗链机制,建议将图片保存下来直接上传(img-TS07UH2X-1669825910323)(resources/D72F2F916B826C3B1A9F3E87839BA5CC.jpg =563x184)]](https://img-blog.csdnimg.cn/405464b395e64a3e95b070f242ff7ba1.png)
    
3.  由于在调用cancel方法对mayInterruptIfRunning实参传递的是true，所以会执行Thread.interrupt方法  
    ![[外链图片转存失败,源站可能有防盗链机制,建议将图片保存下来直接上传(img-Sp08IWKv-1669825910323)(resources/81DA619959EFD816E3E5F8312AFD2BF3.jpg =623x366)]](https://img-blog.csdnimg.cn/8d99da8d6d864c48b3f8ba8522ef79c0.png)
    
4.  Thread.interrupt会对blockerlock加锁，根据线程栈，我们看到intterupt后会执行DatagramChannelImpl.implCloseSelectableChannel方法关闭UDP链接。  
    ![在这里插入图片描述](https://img-blog.csdnimg.cn/dd8fa211ed4d4e80920022d00947842d.png)
    
5.  而implCloseSelectableChannel方法要拿到stateLock的锁，根据线程栈，此时出现锁等待。  
    ![在这里插入图片描述](https://img-blog.csdnimg.cn/e778475d3ace4d55a814c2b40f0d2883.png)
    

再看`pool-486-thread-132` 的死锁线程栈：

1.  这个线程栈比较长，直接从 `at org.xbill.DNS.UDPClient.connect(UDPClient.java:107)` 开始看，可以从包名看出，这是做dnsjava向DNS server发起UDP请求。  
    ![在这里插入图片描述](https://img-blog.csdnimg.cn/8a2900f493ec4bb3bb2aaa1cd339f01c.png)
    
2.  connect方法通过DatagramChannel.connect开启UDP链接。  
    ![在这里插入图片描述](https://img-blog.csdnimg.cn/087bb2cd91b64a33b366631bef89a17c.png)
    
3.  DatagramChannel.connect中会锁住readLock、writeLock、stateLock，即线程栈中的：
    

-   readLock `locked <0x00000006b77f20f8> (a java.lang.Object)`
-   writeLock `locked <0x00000006b77f2110> (a java.lang.Object)`
-   stateLock `locked <0x00000006b77f2128> (a java.lang.Object)`

并锁住bloackingLock 即线程栈中的 `locked <0x00000006b77f20e0> (a java.lang.Object)`  
然后分配Buffer并执行receive方法。  
![[外链图片转存失败,源站可能有防盗链机制,建议将图片保存下来直接上传(img-DKx6gIOS-1669825910324)(resources/110FF85914FB4D9A75DD8F8F677F8B0A.jpg =817x682)]](https://img-blog.csdnimg.cn/b47ff70cd9574588bbed2f4802d1139f.png)

4.  在receive方法并根据线程栈逐层往下，recevie方法finally最后会执行end方法，end方法调用到最后实际上调用了Thread.blockedOn，bockedOn需要拿到blockerLock锁。此时这个锁`<0x00000006b77f2098>` 正在被`histirx-***`线程持有着。  
    ![在这里插入图片描述](https://img-blog.csdnimg.cn/50cf793873a047d6a92ac413f8968872.png)  
    ![在这里插入图片描述](https://img-blog.csdnimg.cn/51a48128e42c4575ba2434d351dcefc3.png)  
    ![在这里插入图片描述](https://img-blog.csdnimg.cn/a19aa94154a44d588391e466b51e79ce.png)

这个链路看起来，似乎是由一个线程建立UDP并发包，一个线程对该线程进行interrupt，就会产生死锁。  
这一点，也有人在openjdk的bug列表中提出， [JDK-8228765](https://bugs.openjdk.org/browse/JDK-8228765)  
![在这里插入图片描述](https://img-blog.csdnimg.cn/1b9a05ca824d44d7a135bcafe95531db.png)

那么线上为啥会出现hystrix线程对另外一个http请求正在DNS解析线程进行interrupt的动作？这里简单说明下：我们的业务中会通过BatchExecutor提交一个异步的业务逻辑，并且超时时间为300ms。若300ms以后未正常返回结果，自然会执行finally。  
![在这里插入图片描述](https://img-blog.csdnimg.cn/4813ae7604f64ea1b1c59e14ce35bce6.png)

由于在异步业务逻辑比较复杂，请求多个rpc或其他资源，若一个请求超过300ms，其他业务逻辑也会被超时终止，即被interrupt，当有DNS时，便会触发这个bug。业务流量大的情况下触发概率比较大。

### 复现

经过上面的分析并参考[JDK-8228765](https://bugs.openjdk.org/browse/JDK-8228765)，我们可以构造2个线程，一个发UDP包，一个对其进行interrupt操作，参考如下代码：

```
import java.io.IOException;
import java.net.InetSocketAddress;
import java.net.StandardSocketOptions;
import java.nio.channels.DatagramChannel;
import java.util.concurrent.ThreadLocalRandom;

public class UDPReceiver {

    public static void main(String[] args) throws IOException, InterruptedException {
        DatagramChannel receiver = DatagramChannel.open();
        receiver.setOption(StandardSocketOptions.SO_REUSEADDR, true);
        receiver.bind(new InetSocketAddress("localhost",8000));//监听UDP 8000端口

        UDPSender sender = new UDPSender();
        new Thread(sender).start(); //启动一个线程开始发UDP包
        while (true) {
            System.out.println("before interrupt");
            sender.interruptThis(); //这里会触发死锁
            System.out.println("after interrupt");
            Thread.sleep(ThreadLocalRandom.current().nextLong(0,100));
        }
    }
}

```

```
import java.net.InetSocketAddress;
import java.net.StandardSocketOptions;
import java.nio.ByteBuffer;
import java.nio.channels.DatagramChannel;
import java.util.concurrent.ThreadLocalRandom;

public class UDPSender implements Runnable {
    private static final InetSocketAddress target = new InetSocketAddress("localhost", 8000);
    private Thread thisThread;


    public void interruptThis() {
        if (null != thisThread) {
            thisThread.interrupt(); //引发死锁的源头2
        }
    }

    @Override
    public void run() {
        thisThread = Thread.currentThread(); //保存一下sender的当前线程
        while (true) {
            try {
                DatagramChannel sender = DatagramChannel.open();
                sender.setOption(StandardSocketOptions.SO_REUSEADDR, true);
                sender.connect(target); //连接本地8000端口发包
                System.out.println("sending...");
                sender.send(ByteBuffer.allocate(100).putInt(1), target);//引发死锁的源头1
                System.out.println("sending success...");
                Thread.sleep(ThreadLocalRandom.current().nextLong(0, 100));
            } catch (Exception e) {
            
            }

        }
    }
}

```

如下图所示，在跑几轮后before interrupt的时候出现死锁。  
![在这里插入图片描述](https://img-blog.csdnimg.cn/426dabbae9b546d883515c87e976cce2.png)

```
jstack -l <pid>
```

打印线程栈，最后可看到死锁信息。  
![[外链图片转存失败,源站可能有防盗链机制,建议将图片保存下来直接上传(img-hCx73oRV-1669825910326)(resources/0BCA07EE03E87EFAD3963670B36C5CB4.jpg =882x628)]](https://img-blog.csdnimg.cn/c4b45c2826264b788022c472dd04ab45.png)

线程Thread-0等待的锁对象 `0x000000076b0f9668`由主线程在执行UDPSender.interruptThis时锁住，即对Thread-0线程进行interrupt。根据Thread.interrupt方法的描述，若当前线程处于IO方法阻塞时，通道会被关闭，会抛出 ClosedByInterruptException 异常并设置中断状态为 true。  
![在这里插入图片描述](https://img-blog.csdnimg.cn/80b48eebb9b843a38b631e4f2ba4de0a.png)

死锁的整个过程如下：  
先看new Thread(sender).start()即Thread-0的执行。  
可以看死锁的线程栈调用链路执行情况，参考代码推导：  
![在这里插入图片描述](https://img-blog.csdnimg.cn/e4b2bc8908f84038aefbbc0994846767.png)

1.  DatagramChannelImpl实现了AbstractInterruptibleChannel。在DatagramChannelImpl.send方法中会拿到两把锁，注意其中一把为stateLock，即线程中的`- locked <0x000000076b1e2be8> (a java.lang.Object)`  
    ![在这里插入图片描述](https://img-blog.csdnimg.cn/59835b00314247039399454cf3e5de93.png)
    
2.  在write方法中，可以看到调用this.begin方法，即AbstracInterruptibleChannel.begin。在IOUtil.write真正发包完成后，会执行finally中的this.end方法。  
    ![在这里插入图片描述](https://img-blog.csdnimg.cn/c1665e471faa4bc48d3fa01059c18e16.png)
    

AbstracInterruptibleChannel.begin注册了收到interrupt后要执行的逻辑，注意这里，下面会用。  
![在这里插入图片描述](https://img-blog.csdnimg.cn/de6dd5ee61d9467d83a96a07c0b9a767.png)

3.  在AbstractInterruptibleChannel.end方法中通过blockedOn方法标记block状态  
    ![在这里插入图片描述](https://img-blog.csdnimg.cn/37dba4c70a3e4b6f81d982c5c7384a48.png)
    
4.  blockedOn会最终调到Thread.blockedOn方法，且实参为当前Thread-0线程。注意这里，要拿到blockerlock，即线程栈中的`- waiting to lock <0x000000076b0f9668> (a java.lang.Object)`。  
    ![在这里插入图片描述](https://img-blog.csdnimg.cn/3de9dcd57fe74d22972c0613efbebafe.png)
    

再看sender.interruptThis()的执行：  
可以看死锁的线程栈调用链路执行情况，参考代码推导：  
![在这里插入图片描述](https://img-blog.csdnimg.cn/f0390dc313f94090a7dfd1d37b8e1694.png)  
![在这里插入图片描述](https://img-blog.csdnimg.cn/5c204d3e149d4f7fa4d56b28fcf81e69.png)

1.  在Thread.interrupt方法中会调用b.interrupt，注意这里会拿到blokcerLock，即线程栈中的`- locked <0x000000076b0f9668> (a java.lang.Object)`  
    ![在这里插入图片描述](https://img-blog.csdnimg.cn/9d2c363521504994a8b9a4b056af75db.png)
    
2.  b.interrupt会触发AbstracInterruptibleChannel在begin方法中实现的interrupt方法  
    ![在这里插入图片描述](https://img-blog.csdnimg.cn/24afc2436d7545528c05fa22fb4d0e84.png)
    
3.  通过AbstracInterruptibleChannel.implCloseChannel关闭通道。  
    ![在这里插入图片描述](https://img-blog.csdnimg.cn/fc033a0d659a46bf8ecc490f7b86e75b.png)
    
4.  在DatagramChannelImpl.implCloseChannel的实现中，要拿到stateLock这把锁。注意这里就是线程栈中的`- waiting to lock <0x000000076b1e2be8> (a java.lang.Object)`  
    ![在这里插入图片描述](https://img-blog.csdnimg.cn/bbc01d7255b84cadb64bceeaf57d0119.png)
    

从上面的分析可以很明显的看出，主线程触发Thread-0的interrupt动作时，在执行interupt方法持有了blockerLock锁，关闭通道要拿stateLock锁，而Thread-0在发包时先拿到了stateLock锁，并且设置block状态时要拿blockerLock锁，此时出现死锁！

### 解决方法

1.  根据OpenJDK的bug列表中的说明 [JDK-8228765](https://bugs.openjdk.org/browse/JDK-8228765) ，这个bug从JDK-8039509就开始有了，一直到jdk13才修复。所以，要么升级jdk到13，要么死！我选择了死。
2.  我们项目中使用了dnsjava，在dnsjava的github上 [issues#69](https://github.com/dnsjava/dnsjava/issues/69) 里也是有报这个bug的。另外在logstash中也有类似的问题 [issues#117](https://github.com/mp911de/logstash-gelf/issues/117)。
3.  当然如果选择了死，那就是遇到了重启服务器了。

### 总结

1.  JVM的bug也很多，同时也很诡异，需要仔细观察运行时状态+阅读源码才能分析出原因，如果你不想分析原因，可以用google搜搜看，很可能已经有了答案。
2.  排查问题思路很重要，由于业务环境通常比较复杂，在大流量、大数据量的情况下会有各种千奇百怪的问题，需要有足够的耐心和较广的知识面。
