---
source: https://blog.csdn.net/m0_63437643/article/details/122622739?ops_request_misc=%257B%2522request%255Fid%2522%253A%2522168994945516800188549628%2522%252C%2522scm%2522%253A%252220140713.130102334.pc%255Fall.%2522%257D&request_id=168994945516800188549628&biz_id=0&utm_medium=distribute.pc_search_result.none-task-blog-2~all~first_rank_ecpm_v1~rank_v31_ecpm-4-122622739-null-null.142^v90^chatsearch,239^v3^control&utm_term=GCTaskManager&spm=1018.2226.3001.4187
---
## Parallel GC

**多线程垃圾回收**

Parallel GC即并行[垃圾回收器](https://so.csdn.net/so/search?q=%E5%9E%83%E5%9C%BE%E5%9B%9E%E6%94%B6%E5%99%A8&spm=1001.2101.3001.7020)，它是面向吞吐量的垃圾回收器，使用-XX:+UseParallelGC开启。Parallel GC是基于分代堆模型的垃圾回收器，其YGC和FGC的逻辑与Serial GC基本一致，只是在垃圾回收过程中不再是单线程扫描、复制对象等，而是用GCTaskManager创建GCTask并放入GCTaskQueue，然后由多个GC线程从队列中获取GCTask并行执行。相比单线程的Serial GC，它的显著优势是当处理器是多核时，多个GC线程使得STW时间大幅减少。下面代码清单10-13展示了Parallel GC的YGC过程：

代码清单**10-13 Parallel GC**的**YGC**

```
bool PSScavenge::invoke_no_policy() {...{// GC任务队列GCTaskQueue* q = GCTaskQueue::create();// 扫描跨代引用if (!old_gen->object_space()->is_empty()) {uint stripe_total = active_workers;for(uint i=0; i < stripe_total; i++) {q->enqueue(new OldToYoungRootsTask(...));}}// 扫描各种GC Rootq->enqueue(new ScavengeRootsTask(universe));q->enqueue(new ScavengeRootsTask(jni_handles));PSAddThreadRootsTaskClosure cl(q);Threads::java_threads_and_vm_thread_do(&cl);q->enqueue(new ScavengeRootsTask(object_synchronizer));q->enqueue(new ScavengeRootsTask(management));q->enqueue(new ScavengeRootsTask(system_dictionary));q->enqueue(new ScavengeRootsTask(class_loader_data));q->enqueue(new ScavengeRootsTask(jvmti));q->enqueue(new ScavengeRootsTask(code_cache));TaskTerminator terminator(...);// 如果active_workers大于1，添加一个StealTaskif (gc_task_manager()->workers() > 1) {for (uint j = 0; j < active_workers; j++) {q->enqueue(new StealTask(terminator.terminator()));}}// 停止继续执行，直到上述Task执行完成gc_task_manager()->execute_and_wait(q);}// 处理非GC Root直达、成员字段可达的对象PSKeepAliveClosure keep_alive(promotion_manager);PSEvacuateFollowersClosure evac_followers(promotion_manager);...// YGC结束，交换From和To空间if (!promotion_failure_occurred) {young_gen->eden_space()->clear(SpaceDecorator::Mangle);young_gen->from_space()->clear(SpaceDecorator::Mangle);young_gen->swap_spaces();...}return !promotion_failure_occurred;}
```

Serial GC使用young_process_roots()扫描GC Root，而Parallel GC是将GC Root扫描工作包装成一个个GC任务，放入GC任务队列等待GC任务管理器一起处理；Serial GC使用  
FastEvacuateFollowersClosure处理对象成员字段可达对象，而Parallel GC使用PSEvacuateFollowersClosure多线程处理；不过，YGC完成后Serial GC和Parllel GC都会交换From和To空间。从算法上看，两个垃圾回收器并无太大区别，只是Parallel GC充分利用了多核处理器。

**GC任务管理器**

Parallel GC使用ScavengeRootsTask表示GC Root扫描任务。

ScavengeRootsTask实际上继承自GCTask，它会被放入GCTaskQueue，然后由GCTaskManager统一执行，如图10-6所示。

![](https://img-blog.csdnimg.cn/img_convert/5d940d7d280b3e1985d356ef41ee6418.png)

图10-6 动态GC任务分配

如代码清单10-14所示，垃圾回收器会向GCTaskQueue投递OldToYoungRootTask、ScavengeRootsTask、ThreadRootsTask和StealTask，然后execute_and_wait()会阻塞[垃圾回收](https://so.csdn.net/so/search?q=%E5%9E%83%E5%9C%BE%E5%9B%9E%E6%94%B6&spm=1001.2101.3001.7020)过程，直到所有GCTask被GC线程执行完毕，这也是并发垃圾回收器和并行垃圾回收器的显著区别：并发垃圾回收器（几乎）不会阻塞垃圾回收过程，而并行垃圾回收器会阻塞整个GC过程。实际上execute_and_wait()也创建了一个GC Task。

代码清单**10-14 WaitForBarrierGCTask**

```
GCTaskManager::execute_and_wait(GCTaskQueue* list) {WaitForBarrierGCTask* fin = WaitForBarrierGCTask::create();list->enqueue(fin);OrderAccess::storestore();add_list(list);fin->wait_for(true /* reset */);WaitForBarrierGCTask::destroy(fin);}
```

GCTaskManager相当于一个任务调度中心，实际执行任务的是GCTaskThread，即GC线程。当投递了一个WaitForBarrierGCTask任务后，当前垃圾回收线程一直阻塞，直到GC任务管理器发现没有工作线程在执行GCTask。

每个GCTask的工作量各不相同，如果一个GC线程快速完成了任务，另一个GC线程仍然在执行需要消耗大量算力的任务，此时虽然其他线程空闲，但垃圾回收[STW](https://so.csdn.net/so/search?q=STW&spm=1001.2101.3001.7020)时间并不会减少，因为在执行下一步操作前必须保证所有GCTask都已经执行完成。这是任务调度的一个常见问题。

为了负载均衡，GC线程可以将GCTask分割为更细粒度的GCTask然后放入队列，比如一个指定GC Root类型扫描任务可以使用BFS（Breadth First Searching，广度优先搜索）算法，将GC Root可达的对象放入BFS队列，搜索BFS队列中对象及其成员字段以构成一个更细粒度的GCTask，这些细粒度任务可被其他空闲GC线程窃取，这种方法也叫作工作窃取（Work Stealing）。

工作窃取是Parallel GC性能优化的关键，它实现了动态任务负载（Dynamic Load Balancing，DLB），可以确保其他线程IDLE时任务线程不会过度负载。工作窃取算法对应StealTask，它的核心逻辑如代码清单10-15所示：

代码清单**10-15 Steal Task**

```
template<class T, MEMFLAGS F> boolGenericTaskQueueSet<T, F>::steal_best_of_2(...) {// 如果任务队列多于2个if (_n > 2) {// 随机选择两个队列uint k1 = ...;uint k2 = ...;uint sz1 = _queues[k1]->size();uint sz2 = _queues[k2]->size();uint sel_k = 0;bool suc = false;// 在随机选择的k1和k2队列中选择GCTask个数较多的那个，窃取一个GCTaskif (sz2 > sz1) {sel_k = k2;suc = _queues[k2]->pop_global(t);} else if (sz1 > 0) {sel_k = k1;suc = _queues[k1]->pop_global(t);}...// 窃取成功return suc;} else if (_n == 2) {// 如果任务队列只有两个，那么随机窃取一个任务队列的GCTaskuint k = (queue_num + 1) % 2;return _queues[k]->pop_global(t);} else {return false;}}
```

简单来说，垃圾回收器将随机选择两个任务队列（如果有的话），再在其中选择一个更长的队列，并从中窃取一个任务。任务窃取不总是成功的，如果一个GC线程尝试窃取但是失败了2*_N_次，_N_等于(ncpus<=8)?ncpus:3+((ncpus*5)/8))，那么当前GC线程将会终止运行。

工作窃取使用GenericTaskQueue，这是一个ABP（Aurora-Blumofe-Plaxton）风格的双端队列，队列的操作无须阻塞，持有队列的线程会在队列的一端指向push或pop_local，其他线程可以对队列使用如代码清单10-15所示的pop_global完成窃取任务。

有了动态任务负载后，GC线程的终止机制也需要对应改变。具体来说，GC线程执行完GCTask后不会简单停止，而是查看能否从其他线程任务队列中窃取一个任务队列，如果所有线程的任务队列都没有任务，再进入终结模式。终结模式包含三个阶段，首先指定次数的自旋，接着GC线程调用操作系统的yield让出CPU时间，最后睡眠1ms。如果GC线程这三个小阶段期间发现有可窃取的任务，则立即退出终结模式，继续窃取任务并执行。

## 并行与并发

在垃圾回收领域中，并发（Concurrent）和并行（Parallel）有区别于通用编程概念中的并发和并行：并发意味着垃圾回收过程中（绝大部分时间）Mutator线程可以和多个GC线程一起工作，几乎可以认为在垃圾回收进行时，Mutator也可以继续执行而无须暂停；并行是指垃圾回收过程中允许多个GC线程一同工作来完成某些任务，但是Mutator线程仍然需要暂停，即垃圾回收过程中应用程序需要一直暂停。

Parallel GC为减少STW时间付出了努力，它的解决方式是暂停Mutator线程，使用多线程进行垃圾回收，最后唤醒所有Mutator，如图10-7所示。

![](https://img-blog.csdnimg.cn/img_convert/f01091d30c6a0a3758ab13e669ce737e.png)

图10-7 Parallel GC

其中，并发垃圾回收线程数目由-XX:ConcGCThreads=<val>控制，并行垃圾回收线程数目由-XX:ParallelGCThreads=<val>控制。但多线程并行化垃圾回收工作过程中Mutator线程仍然需要暂停，所以人们期待一种在垃圾回收阶段Mutator线程仍然能继续运行的垃圾回收器，或者至少在垃圾回收过程中大部分时间Mutator线程可以继续运行的垃圾回收器。等等，这不就是前面提到的并发垃圾回收器的概念吗？是的，并发垃圾回收器CMS GC可以解决这个问题（虽然它的解决方案并不完美）。

## 本文给大家讲解的内容是深入解析java虚拟机：垃圾回收，Parallel GC

1.  **下篇文章给大家讲解的是深入解析java虚拟机：垃圾回收，CMS GC；**
2.  **觉得文章不错的朋友可以转发此文关注小编；**
3.  **感谢大家的支持!**
