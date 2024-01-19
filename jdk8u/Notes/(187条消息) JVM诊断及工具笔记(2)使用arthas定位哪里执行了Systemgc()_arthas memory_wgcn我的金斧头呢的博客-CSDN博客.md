---
source: https://blog.csdn.net/m0_37053048/article/details/125958822?ops_request_misc=&request_id=&biz_id=102&utm_term=GCTaskManager&utm_medium=distribute.pc_search_result.none-task-blog-2~all~sobaiduweb~default-3-125958822.nonecase&spm=1018.2226.3001.4187
---
![封面图片不要使用微信打开文章，可以使用手机/电脑浏览器](https://img-blog.csdnimg.cn/img_convert/85a04017392ab91cab6cb4897898da22.png)

### 前言

这篇文章是之前解决一个[Flink](https://so.csdn.net/so/search?q=Flink&spm=1001.2101.3001.7020)任务在线上发生fullgc

![image-20211023220400506](https://img-blog.csdnimg.cn/img_convert/cc8181fa2d68ffd2ee6c7e4606da5027.png)

当时的想法就是fullgc发生在:

-   [堆内存](https://so.csdn.net/so/search?q=%E5%A0%86%E5%86%85%E5%AD%98&spm=1001.2101.3001.7020): 通过堆内存监控和dump堆内存这两种方式 都发现堆占用不大 ,排除
    
-   metaspace: gc日志里并没有堆metaspace日志且metaspace占用很小 ,排除
    

因此主要把排除重点放在直接内存。接下来老规矩，我们长话短说会夹带一点点心路历程。

### 步骤

#### 1.配置jvm参数打印gc

```
 clusterConf.env.java.opts=-XX:+PrintGCDetails -XX:+PrintGCDateStamps -Xloggc:${LOG_DIRS}/gc.log  
```

#### 2.在gc日志里发现有程序主动调用System.gc()

![image-20211023220520256](https://img-blog.csdnimg.cn/img_convert/af13e86a95bce173c8b045cd0dca5d36.png)

当时猜测是使用的flink11版本代码里面会有调用

![image-20211023220653384](https://wgcn.oss-cn-beijing.aliyuncs.com/img/image-20211023220653384.png)

加了些日志，发现并没有调用。

#### 3.使用arthas

##### 3.1.下载 arthas，并attach需要监听的jvm进程

```
curl -O https://arthas.aliyun.com/arthas-boot.jar
java -jar arthas-boot.jar
```

![image-20211023182623635](https://img-blog.csdnimg.cn/img_convert/4897fc4b05c7e74e5f9321a722e904d2.png)

##### 3.2.监听 System.gc()方法并把日志打到磁盘上

```
options unsafe true

stack java.lang.System gc -n 1  >> /tmp/gc.log &
```

![image-20211023183103082](https://img-blog.csdnimg.cn/img_convert/05a4ed5fa0e4af1190006491eb2863df.png)

##### 3.3 观察日志

过了一段时间 观察下日志发现

![image-20220103230624856](https://img-blog.csdnimg.cn/img_convert/60fbd86c8dbeb9b4bd4bb7c2d6a25af3.png)

是在[Kafka](https://so.csdn.net/so/search?q=Kafka&spm=1001.2101.3001.7020) connector 调用java的nio的时候调用的System.gc()

#### 4.分析

##### 4.1讲一下这块代码的大致**背景**

java的直接内存(direct memory)是不会被gc回收的，**而是通过监听持有直接内存资源的对象被回收的时候才把直接内存释放掉的**。主要原理使用到虚引用和引用队列:当jvm发现对象已经没有强引用，仅剩虚引用时会将其存放在Reference的**discovered**的列表中 —>随后变成**Pending**状态 (准备加入引动队列) ---->随后进入**Enqueued**状态(加入队列，监听者可以获取从而回收资源)

##### 4.2背景介绍完毕，回到刚才找到的方法栈，看倒数第二个方法

![image-20211023200715246](https://img-blog.csdnimg.cn/img_convert/b4aae3be6c0f327093831f1c8926b755.png)

栈里 DirectByteBuffer 这个类就是直接内存资源的持有者。

![DirectByteBuffer](https://img-blog.csdnimg.cn/img_convert/f98c455b84e7af0b2beca3310d057f32.png)

DirectByteBuffer 在构造时便被Cleaner监听回收资源。束于篇幅稍微文字介绍下Cleaner, Cleaner 就是 PhantomReference(虚引用)的子类。 1.Cleaner#create的方法就是将DirectByteBuffer对象虚引用且监听引用队列，2.当在队列中接收到DirectByteBuffer(此时对象已经被jvm标记discovered) 执行Deallocator的回收资源操作。

##### 4.3 倒数第一个方法

![image-20211023203728028](https://img-blog.csdnimg.cn/img_convert/ac982298ddafe5342dbb86219823405c.png)

![image-20211023204240359](https://img-blog.csdnimg.cn/img_convert/4f63959798bea6607468e81e21265022.png)

来到倒数第一个方法，打开实现大家应该就能明白，这个方法是给DirectByteBuffer预留资源的。如果资源充足，万事大吉。如果资源充足: 大家回忆下刚才说讲的被虚引用DirectByteBuffer的discovered,pending,enqueued状态, 图中我用箭头标记的jlra.tryHandlePendingReference() 是尽快将pending状态的对象尽快转换成enqueued状态被好被Cleaner回收掉。一顿操作过后发现资源依旧不够。那么只能调用System.gc() 方法执行fullgc了。

注: 这套实现中 使用fullgc的好处是有部分 DirectByteBuffer对象很多次回收不掉进入老年代之后，这个时候young gc是没有办法回收掉的。

#### 5.总结

刚才分析那么多，其实总结起来就一句话:直接内存不够了。回头又看了下用户任务的特点。发现数据消费量很大，但是代码可用的直接内存不多，反倒是框架可用的直接内存(taskmanager.memory.framework.off-heap.size)设得很大。合理调节完资源后，这个任务就再也没有发生oom了。
