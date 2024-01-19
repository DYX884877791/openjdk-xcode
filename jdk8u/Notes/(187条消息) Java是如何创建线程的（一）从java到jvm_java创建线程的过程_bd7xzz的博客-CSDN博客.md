---
source: https://blog.csdn.net/kid_2412/article/details/128890908
---
## Java是如何[创建线程](https://so.csdn.net/so/search?q=%E5%88%9B%E5%BB%BA%E7%BA%BF%E7%A8%8B&spm=1001.2101.3001.7020)的（一）从java到jvm

## 背景

线程我们经常用，也会经常被面试或讨论到。我如果说Java如何创建线程的？你肯定回答下面三种：  
1.最基本的创建方式（ 不需要拿到返回值、异常信息）：

```
Thread thread =  new Thread(new Runnable() {  //创建线程并实现Runnable接口
            @Override
            public void run() { //在run方法中实现业务功能
               //do something 
            }
        });
        thread.start(); //启动线程
```

2.  通过FutureTask创建（可以拿到返回值、拿到异常信息）：

```
FutureTask<Integer> futureTask = new FutureTask<>(new Callable<Integer>() { //通过FutureTask并实现Callable接口
            @Override
            public Integer call() throws Exception { //在call方法中实现业务功能
                //do something
                return null; //返回值
            }
        });
        Thread thread = new Thread(futureTask); //创建线程对象，传递FutureTask对象
        thread.start(); //启动线程
        try {
           Integer result = futureTask.get(); //阻塞并拿到返回结果
        } catch (InterruptedException e) { //interrupt中断异常
        } catch (ExecutionException e) { //执行期间业务异常
        }
```

3.  通过线程池创建线程（包含第一种和第二种）：

```
//向线程池提交线程，无返回值、无异常
 Executors.newCachedThreadPool().submit(new Runnable() { 
            @Override
            public void run() {  //在run方法中实现业务功能
                //do something
            }
        });
      
  //向线程池提交线程，可以拿到返回值、异常信息
 Future<Integer> future = Executors.newCachedThreadPool().submit(new Callable<Integer>() {
            @Override
            public Integer call() throws Exception { //在call方法中实现业务功能
                //do something
                return null;
            }
        });
        try {
            Integer result = future.get();  //阻塞并拿到返回结果
        } catch (InterruptedException e) {  //interrupt中断异常
        } catch (ExecutionException e) { //执行期间业务异常
        }
```

你一样会想，这么简单的问题有什么好问的，简直侮辱我的智商。但，请仔细想想，你真的知道**Java是如何创建线程的吗？** 不知道你有没有想过下面的问题：

1.  为什么调用start方法启动线程？而不是调用run方法？（这里涉及到了start方法到底是如何执行的）
2.  既然有了最基本的创建线程方法就可以创建线程了，为什么还用线程池？你可能会说，创建线程耗费资源，那到底耗费了多少资源？耗费了什么资源？（这涉及到了start方法之后操作系统做了什么事）
3.  我们通过Thread创建完线程，并调用start，线程就会立刻启动执行吗？（这涉及到了线程创建后是如何调度的）

上面3个问题，都包含在一个问题中，Java是如何跟操作系统交互创建线程，并被操作系统调度的？

这3个问题，是我在2023年元旦放假休息的时候突然想到的，为什么会这么想？ **市面上的讲解多线程的文章、书籍，都是站在使用者的角度分析线程。** 这会带来2个问题：

1.  易于理解的是建立与高度抽象之上的，背后的细节和真相被隐藏。
2.  在抽象之上进行口口相传，信息传递出错的概率增大，信息会被误解。

所以，我想看看背后的真相，看看线程的源代码是怎么实现的。

在正式内容之前，我希望你抛弃之前对线程的认识，同时不要惧怕底层原理和代码，这篇文章也会很长。坚持看下去，这会让你对多线程有更清晰的认识，不会对多线程中的概念感到迷茫和不解。

___

## 先验知识

这篇文章会从Java 创建线程的API接口开始，到JVM的C++代码，再到[glibc](https://so.csdn.net/so/search?q=glibc&spm=1001.2101.3001.7020) pthread线程库的c代码，再到操作系统内核的c和少量汇编代码。所以：

1.  你可以不会汇编代码，我会解释
2.  你可以不会c和c++代码，我会解释
3.  你要是不会Java代码，那你还看这篇文章干啥？

阅读JVM的C++代码，可以下载clion编译器或vscode，并通过git clone OpenJDK的代码（Oracle HotSpot的多线程代码和OpenJDK的多线程代码没有区别）。

```
git clone https://github.com/openjdk/jdk.git
```

本篇文章采用jdk1.8的环境

```
$ java -version
java version "1.8.0_361"
Java(TM) SE Runtime Environment (build 1.8.0_361-b09)
Java HotSpot(TM) 64-Bit Server VM (build 25.361-b09, mixed mode)
```

所以将clone下来OpenJDK源码切换到jdk8-b09这个tag就可以了

```
git checkout jdk8-b09
```

阅读glibc和linux源码，可以通过 [bootlin](https://elixir.bootlin.com/) 这个在线网站  
Linux内核版本为6.1.7，可以看这里 [bootlin linux v6.1.7](https://elixir.bootlin.com/linux/v6.1.7/source)

```
$ uname -a
Linux bd7xzz 6.1.7-1-MANJARO #1 SMP PREEMPT_DYNAMIC Wed Jan 18 22:33:03 UTC 2023 x86_64 GNU/Linux
```

glibc版本为2.36，可以看这里 [bootlin glibc v2.36](https://elixir.bootlin.com/glibc/glibc-2.36/source)

```
$ ldd --version
ldd (GNU libc) 2.36
```

不同的Linux内核和glibc版本实现线程略有差异，希望你能参考这篇文章在自己机器的环境下阅读源码（这才是这篇文章最大的意义）。

不同的操作系统和语言实现线程的方式略有不同，基本分为三种：

1.  一对一（1:1）：即一个用户线程对应一个内核线程，这是Linux实现线程的方式
2.  一对多（N:1）：即多个用户线程对应一个内核线程，这种实现方式的代表是go语言的协程
3.  多对多（N:M）：即多个用户线程对应多个内核线程，这种实现方式的代表是Solaris操作系统

基本概念：

-   用户线程：程序员通过API创建的线程，即用户态表示线程的数据结构。
-   内核线程：用户态的线程后要创建内核态线程，CPU调度的是内核态线程。
-   用户态：老生常谈的概念，操作系统为了保护系统内核不被破坏，划分出用户态和内核态，程序员编写的代码都在用户态以较低的权限运行，放在系统被破坏。
-   内核态：当代码要执行诸如读写磁盘、网卡发送数据、内存申请时通过系统调用（syscall）进入内核态，由操作系统内核分配资源。
-   系统调用（syscall）：程序员在用户态的代码申请资源进入内核态时调用的API，调用syscall时用户态的上下文参数要保存起来，即用户态的代码执行权限被内核态的内核代码接管，用户态代码挂起了，内核执行完将结果返回给用户态，将之前保存好的上下文参数恢复过来，让用户态代码继续执行。在Linux操作系统中，系统调用切换进内核态是通过**汇编指令实现的**。  
    _用户态和内核态的概念网络上有很多解释，这里不做过多解释，有兴趣读者可以自行查找_
-   内核调度器：调度器是操作系统内核实现的，用来分配多任务（这里可能是进程或线程的执行指令）。将这些任务分配到 CPU 上执行，为每个任务分配合适的 CPU 运行时间。理论上调度器会尽量保证每个任务执行时间是公平的。

有了上面的概念基础，再看实现线程的三种方式：

1.  一对一：用户态创建一个线程，内核就创建一个线程，用户态不会对线程做任何调度管理，完全交给内核去调度，用户态只关注执行业务逻辑即可。缺点在于，创建线程成本比较大，内核态和用户态的切换要保存上下文。在Linux下，线程被称为轻量级进程（LWP），本质上复用了进程的代码，与进程相比内存空间略有不同。

![在这里插入图片描述](https://img-blog.csdnimg.cn/365879d11427408d803120049705a47d.png#pic_center)

2.  一对多：即只有 一个内核线程，多个用户态线程。这种方式的好处是不需要与内核进行过多打交道，内核态与用户态分别创建 一 对 一 的线程后，后续的轻量级线程（LWP) 由用户态的程序自身实现，模拟系统内核线程做的事情。缺点在于用户态需要考虑更多的东西，如：

-   模拟线程的代码比较复杂，要完全实现线程的创建、销毁、状态转换等。
-   业务代码要考虑执行耗时，耗时高会影响其他用户态线程的调度。
-   若存在资源竞争加锁的情况，可能导致线程被阻塞，所有用户态线程都会被阻塞。

![在这里插入图片描述](https://img-blog.csdnimg.cn/e31ee91571f94df8acef735ccce8b2b2.png#pic_center)

3.  多对多：即用户态线程有多个，同时也有多个内核态线程。且用户态线程映射到内核态线程的比例是自动分配的。这种方式的好处是降低了与内核态切换带来的开销，用户态线程依旧可以支撑大量的并发。同时也可降低因为阻塞和耗时高对所有用户态线程产生的影响，因为只影响在相同内核线程中映射的用户态线程。

## ![在这里插入图片描述](https://img-blog.csdnimg.cn/787bb15877954e149a3055b8b388b211.png#pic_center)

## 分析

在有了背景和先验知识之后，相信你对线程有了个基本的了解。对其中涉及到的技术框架有了个大体的认识。接下来，开始分析jdk8在linux下是如何创建线程的。注意这里，只介绍最基本的创建方式，对于支持返回值和异常信息的callable希望你能自己阅读源码。  
我会从源码中摘出重要的代码片段贴在文章中，并加以解释。你可以在源码中搜到这些代码，看看前后被省略的代码都做了什么。

我会围绕着背景中的问题梳理：

1.  调用start方法之后做了什么？
2.  JVM是怎么回调run方法的？
3.  jvm和glibc的线程实现是怎么挂钩的？
4.  glibc的线程实现又是怎么和Linux内核线程挂钩的？
5.  内核线程都创建了哪些数据结构，所谓的LWP与进程有什么区别？
6.  内核线程创建完成后，线程是直接执行吗？
7.  观测创建线程的创建，估算线程创建开销大概是多少？

### 从java到JVM

我们在Idea编译器中编写如下创建线程的方法：

```
 new Thread(new Runnable() {  
            @Override
            public void run() {
            }
        }).start();
```

这里new Thread的构造函数做了一些基本的初始化，包括：

1.  初始化线程名
2.  检查SecurityManager
3.  初始化线程组
4.  设置线程优先级（这里调用了本地方法setPriority0()）
5.  分配jvm线程tid（注意不是操作系统线程id）

我们重点关注线程是如何启动的，点击进入start()方法的源码实现。

```
// java/lang/Thread.java

public synchronized void start() {
        if (threadStatus != 0)
            throw new IllegalThreadStateException();

        group.add(this);

        boolean started = false;
        try {
            start0(); //调用本地方法start0启动线程
            started = true;
        } finally {
            try {
                if (!started) {
                    group.threadStartFailed(this);
                }
            } catch (Throwable ignore) {
            } 
        }
    }
```

`题外话：我在代码片段第一行注释上了源码所在路径，下同，你可以根据这个路径找到相关代码。`

这里有2个重点：

1.  start方法是被声明synchronized，即当前对象加锁
2.  调用了start0()这个本地方法

```
 private native void start0();
```

既然看到了native方法，那么我们就需要进入jvm源码看看了（有兴趣读者可以查查jni是如何调用native方法的）。通过clion打开OpenJDK1.8的源码，查找start0的实现。得到下面方法声明列表：

```
// jdk/src/share/native/java/lang/Thread.c

static JNINativeMethod methods[] = {
    {"start0",           "()V",        (void *)&JVM_StartThread}, //启动线程的方法
    {"stop0",            "(" OBJ ")V", (void *)&JVM_StopThread},
    ...
};

```

其中：

1.  start0为native方法名
2.  ()V为方法的返回值，表示返回void
3.  (void *)&JVM_StartThread，JVM中C++代码JVM_StartThread()这个方法实现了线程的启动

找到JVM_StartThread的入口

```
// hotspot/src/share/vm/prims/jvm.cpp

JVM_ENTRY(void, JVM_StartThread(JNIEnv* env, jobject jthread))
  JVMWrapper("JVM_StartThread");
  JavaThread *native_thread = NULL; 

  bool throw_illegal_thread_state = false;

  {
    MutexLocker mu(Threads_lock);  //对Threads_lock加锁，确保C++对象和操作系统线程结构不被清除。解锁，是在方法结束出栈后MutexLocker析构函数中进行的。

    if (java_lang_Thread::thread(JNIHandles::resolve_non_null(jthread)) != NULL) { //jthread对象如果是非空的，则证明线程已启动
      throw_illegal_thread_state = true;
    } else {

      jlong size =
             java_lang_Thread::stackSize(JNIHandles::resolve_non_null(jthread));  //取线程栈大小，注意在java/lang/Thread.java源码中，调用init方法时这个参数被设置成0，也就是OpenJDK（包括HotSpot）不支持传递线程栈大小！
      size_t sz = size > 0 ? (size_t) size : 0;
      native_thread = new JavaThread(&thread_entry, sz); //创建JavaThread对象

      if (native_thread->osthread() != NULL) { //当前系统线程对象不为空
        native_thread->prepare(jthread); //标记当前线程对象未被使用
      }
    }
  }

  if (throw_illegal_thread_state) {
    THROW(vmSymbols::java_lang_IllegalThreadStateException()); //状态异常直接抛出
  }

  assert(native_thread != NULL, "Starting null thread?");

  if (native_thread->osthread() == NULL) { //操作系统线程对象为空，无法启动线程了，清理资源并抛出异常
    delete native_thread; 
    if (JvmtiExport::should_post_resource_exhausted()) {
      JvmtiExport::post_resource_exhausted(
        JVMTI_RESOURCE_EXHAUSTED_OOM_ERROR | JVMTI_RESOURCE_EXHAUSTED_THREADS,
        "unable to create new native thread");
    }
    THROW_MSG(vmSymbols::java_lang_OutOfMemoryError(),
              "unable to create new native thread");
  }

  Thread::start(native_thread); //启动线程

JVM_END

```

在上面的代码中，我们看到3个重点：

1.  创建JavaThread代表C++的线程对象
2.  线程栈大小stackSize 被设置为0
3.  Thread::start用来启动线程

先看看`native_thread = new JavaThread(&thread_entry, sz);` 创建线程对象，找到JavaThread的构造函数

```
// hotspot/src/share/vm/runtime/thread.cpp

JavaThread::JavaThread(ThreadFunction entry_point, size_t stack_sz) :
  Thread()
#ifndef SERIALGC //定义serial gc的内存布局
  , _satb_mark_queue(&_satb_mark_queue_set),
  _dirty_card_queue(&_dirty_card_queue_set)
#endif // !SERIALGC
{
  if (TraceThreadEvents) { //跟踪线程栈
    tty->print_cr("creating thread %p", this);
  }
  initialize(); //一顿初始化成员变量默认值
  _is_attaching = false;
  set_entry_point(entry_point); //设置入口点，用于回调java代码中的run方法
  os::ThreadType thr_type = os::java_thread;
  thr_type = entry_point == &compiler_thread_entry ? os::compiler_thread :  //确定是否是JIT编译线程，显然我们传递的参数是oss:java_thread
                                                     os::java_thread;
  os::create_thread(this, thr_type, stack_sz); //创建操作系统线程
  
}
```

这里有2个重点：

1.  `set_entry_point(entry_point);` 设置入口点，用于回调java代码中的run方法
2.  `os::create_thread(this, thr_type, stack_sz);` 创建操作系统线程

先看设置入口点：

```
// hotspot/src/share/vm/runtime/thread.hpp

 private:
  void set_entry_point(ThreadFunction entry_point) { _entry_point = entry_point; }
```

其实set_entry_point只是设置了个成员属性_entry_point，核心的东西都在`ThreadFunction entry_point中`

```
// hotspot/src/share/vm/runtime/thread.hpp
typedef void (*ThreadFunction)(JavaThread*, TRAPS);
```

ThreadFunction本质上是个函数指针，经过上面分析可以看到entry_point是在构建JavaThread时传入的：

```
// hotspot/src/share/vm/prims/jvm.cpp

  native_thread = new JavaThread(&thread_entry, sz);
```

可以看到thread_entry这个函数：

```
static void thread_entry(JavaThread* thread, TRAPS) {
  HandleMark hm(THREAD);
  Handle obj(THREAD, thread->threadObj());
  JavaValue result(T_VOID);
  JavaCalls::call_virtual(&result,
                          obj,
                          KlassHandle(THREAD, SystemDictionary::Thread_klass()),
                          vmSymbols::run_method_name(),
                          vmSymbols::void_method_signature(),
                          THREAD);
}
```

这里重点是`JavaCalls::call_virtual` 干的事：

1.  调用java方法
2.  调用哪个方法？如下定义：

```
// hotspot/src/share/vm/classfile/systemDictionary.hpp
  template(Thread_klass,                 java_lang_Thread,               Pre) \

// hotspot/src/share/vm/classfile/vmSymbols.hpp
  template(run_method_name,                           "run")                                      \
  template(void_method_signature,                     "()V")                                      \
```

即 `java/lang/Thread`下的

```
 void run(){
    // do something...
 }
```

那么谁来调用这个成员变量_entry_point？后面会说。  
接下来我们看下`os::create_thread`是如何创建系统线程的：

```
// hotspot/src/os/linux/vm/os_linux.cpp

bool os::create_thread(Thread* thread, ThreadType thr_type, size_t stack_size) {
  assert(thread->osthread() == NULL, "caller responsible");

 OSThread* osthread = new OSThread(NULL, NULL); //分配系统线程对象
  if (osthread == NULL) {
    return false;
  }

  osthread->set_thread_type(thr_type); //设置线程类型

  osthread->set_state(ALLOCATED); //设置线程状态

  thread->set_osthread(osthread); //关联系统线程对象到C++线程对象上

  pthread_attr_t attr;
  pthread_attr_init(&attr); //初始化线程属性
  pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED); //设置线程属性为detached

  if (os::Linux::supports_variable_stack_size()) { //支持环境变量配置线程栈大小
    if (stack_size == 0) { //参数中stack_size如果是0 则使用默认栈大小
      stack_size = os::Linux::default_stack_size(thr_type);

      switch (thr_type) { //根据线程类型计算线程栈空间大小
      case os::java_thread: //java线程，注意这里就是取的-Xss参数的值
        assert (JavaThread::stack_size_at_create() > 0, "this should be set");
        stack_size = JavaThread::stack_size_at_create();
        break;
      case os::compiler_thread: //JIT编译线程
        if (CompilerThreadStackSize > 0) {
          stack_size = (size_t)(CompilerThreadStackSize * K);
          break;
        } 
      case os::vm_thread:
      case os::pgc_thread:
      case os::cgc_thread:
      case os::watcher_thread: //虚拟机线程
        if (VMThreadStackSize > 0) stack_size = (size_t)(VMThreadStackSize * K);
        break;
      }
    }

    stack_size = MAX2(stack_size, os::Linux::min_stack_allowed); //确定最终的线程栈大小，
    pthread_attr_setstacksize(&attr, stack_size);
  } else {
  //由pthread的默认值决定线程栈大小
  }

  pthread_attr_setguardsize(&attr, os::Linux::default_guard_size(thr_type)); //设置保护区

  ThreadState state;

  {
    bool lock = os::Linux::is_LinuxThreads() && !os::Linux::is_floating_stack(); //如果使用固定堆栈运行Linux线程，则串行化创建线程
    if (lock) {
      os::Linux::createThread_lock()->lock_without_safepoint_check();
    }

    pthread_t tid;
    int ret = pthread_create(&tid, &attr, (void* (*)(void*)) java_start, thread); //调用glibc创建线程

    pthread_attr_destroy(&attr); //创建完成后，销毁线程属性

    if (ret != 0) {
      if (PrintMiscellaneous && (Verbose || WizardMode)) { //创建失败打印异常，销毁资源
        perror("pthread_create()");
      }
      thread->set_osthread(NULL);
      delete osthread;
      if (lock) os::Linux::createThread_lock()->unlock();
      return false;
    }

    osthread->set_pthread_id(tid); //创建成功设置操作系统线程信息

    { //操作系统线程挂起等待线程初始化完成或者中止
      Monitor* sync_with_child = osthread->startThread_lock();
      MutexLockerEx ml(sync_with_child, Mutex::_no_safepoint_check_flag);
      while ((state = osthread->get_state()) == ALLOCATED) {
        sync_with_child->wait(Mutex::_no_safepoint_check_flag);
      }
    }

    if (lock) { //解锁
      os::Linux::createThread_lock()->unlock();
    }
  }

  if (state == ZOMBIE) { //由于达到线程现在而中止创建
      thread->set_osthread(NULL);
      delete osthread;
      return false;
  }

  assert(state == INITIALIZED, "race condition");
  return true;
}

```

上面一坨中有4个重点：

1.  设置线程状态为已分配
    
2.  设置线程属性为detached，线程有2种detach state：
    
    1.  PTHREAD_CREATE_JOINABLE：对于该状态下的线程，主线程需要用 join() 函数来等待子线程执行完毕获得返回值，并释放资源  
        2.PTHREAD_CREATE_DETACHED：对于该状态下的线程，不需要主线程等待，会自动释放资源；PTHREAD_CREATE_JOINABLE 的线程可以转换成 PTHREAD_CREATE_DETACHED 的线程，但不能反向转换
3.  计算线程栈大小，对于Java的用户线程来说规则如下：
    
    1.  64位Linux操作系统下，没有指定栈空间大小，默认为1M，参考代码：`size_t os::Linux::default_stack_size(os::ThreadType thr_type)`
    2.  32位Linux操作系统下，没有指定栈空间大小，默认为512K，参考代码：size_t os::Linux::default_stack_size(os::ThreadType thr_type)
    3.  如果指定-Xss参数，用指定的，参考代码：`stack_size_at_create()`
    4.  64位Linux操作系统允许的最小栈空间为64K，32位最小为48K，参考代码：`size_t os::Linux::min_stack_allowed`
    5.  最后的栈空间大小不会低于系统允许的最小值
4.  调用glibc的pthread_create创建线程，pthread_create有四个参数：
    
    1.  &tid，pthread_t局部变量的地址，线程创建成功后会将线程id设置到这个局部变量中
    2.  &attr，pthread_attr_t局部变量地址，线程属性用于创建线程（包括detach状态、线程栈大小等信息）
    3.  java_start，是一个函数指针，用于回调java中的run方法，具体实现如下：

```
// hotspot/src/os/linux/vm/os_linux.cpp

static void *java_start(Thread *thread) {
 static int counter = 0;
 int pid = os::current_process_id(); 
 alloca(((pid ^ counter++) & 7) * 128); //填充cpu缓存行，提高线程运行效率

 ThreadLocalStorage::set_thread(thread); //设置当前C++的线程对象，放到栈顶

 OSThread* osthread = thread->osthread();
 Monitor* sync = osthread->startThread_lock();

 if (!_thread_safety_check(thread)) { //检查当前能否安全的创建线程
   MutexLockerEx ml(sync, Mutex::_no_safepoint_check_flag);
   osthread->set_state(ZOMBIE); //如果失败则设置状态为ZOMBIE（僵尸进程）
   sync->notify_all();
   return NULL;
 }

 osthread->set_thread_id(os::Linux::gettid()); //为操作系统线程对象设置pid

 if (UseNUMA) { //NUMA架构下设置lgrp_id
   int lgrp_id = os::numa_get_group_id();
   if (lgrp_id != -1) {
     thread->set_lgrp_id(lgrp_id);
   }
 }
 os::Linux::hotspot_sigmask(thread); //设置线程的信号掩码，用户创建的线程屏蔽掉BREAK_SIGNAL信号

 os::Linux::init_thread_fpu_state(); //初始化浮点控制寄存器

 {
   MutexLockerEx ml(sync, Mutex::_no_safepoint_check_flag);

   osthread->set_state(INITIALIZED); //设置线程状态为初始化
   sync->notify_all(); //唤醒父线程

   while (osthread->get_state() == INITIALIZED) { 
     sync->wait(Mutex::_no_safepoint_check_flag); //等待直到调用了oss:start_thread()
   }
 }

 thread->run(); //调用run方法回调java的run方法

 return 0;
}
```

这里有几个细节：

1.  线程创建失败会被设置成ZOMBIE状态，因为Linux操作LWP作为线程，所以个进程的状态是一致的，也会出现僵尸进程
2.  `osthread->set_thread_id(os::Linux::gettid());` 这里设置了内核线程给出的id，通过syscall在内核中获取
3.  设置信号掩码，屏蔽掉BREAK_SIGNAL，这个信号是用在JVM线程的
4.  线程的状态会被设置为INITIALIZED
5.  重点1**在INITIALIZED状态下会等待，直到oss:start_thread()方法被调用**
6.  重点2**通过调用C++线程对象中的run方法，进行回调java的run方法**

`oss:start_thread()`在启动线程的流程放一边中看，看`thread->run();`是怎么回调java的run方法的：

```
// hotspot/src/share/vm/runtime/thread.cpp

void JavaThread::run() {
  this->initialize_tlab(); //初始化tlab

  this->record_base_of_stack_pointer(); //记录线程栈栈顶指针

  this->record_stack_base_and_size(); //记录基地址和线程栈大小

  this->initialize_thread_local_storage(); //初始化thread_local

  this->create_stack_guard_pages(); //创建保护区来支持栈溢出错误处理

  this->cache_global_variables(); //缓存全局变量，注意这个函数什么也没干

  ThreadStateTransition::transition_and_fence(this, _thread_new, _thread_in_vm); //此时，线程已经基本初始化完毕，可以让safe_point接手了，这里修改线程的状态从_thread_new到_thread_in_vm

  assert(JavaThread::current() == this, "sanity check");
  assert(!Thread::current()->owns_locks(), "sanity check");

  DTRACE_THREAD_PROBE(start, this); 

  this->set_active_handles(JNIHandleBlock::allocate_block()); //分配JNIHandleBlock对象，这个调用会阻塞当前线程，所以要在safe_point之后

  if (JvmtiExport::should_post_thread_life()) { //用于JVMTI跟踪线程生命周期
    JvmtiExport::post_thread_start(this);
  }

  thread_main_inner(); //执行java.lang.Thread中run的回调
}

```

这里的重点在于`thread_main_inner`：

```
// hotspot/src/share/vm/runtime/thread.cpp

void JavaThread::thread_main_inner() {
  assert(JavaThread::current() == this, "sanity check");
  assert(this->threadObj() != NULL, "just checking");

  if (!this->has_pending_exception() &&
      !java_lang_Thread::is_stillborn(this->threadObj())) {
    HandleMark hm(this);
    this->entry_point()(this, this); //执行java.lang.Thread的run方法
  }

  DTRACE_THREAD_PROBE(stop, this);

  this->exit(false);
  delete this;
}
```

`this->entry_point()(this, this);`这一行其实就是调用了之前设置的成员变量`_entry_point`，该变量被赋值为`thread_entry`的函数地址，`thread_entry`通过`JavaCalls::call_virtual`回调`java.lang.Thread.java`的`run()`方法。

在看完jvm如何通过pthread_create创建线程对象后，再看看 `Thread::start(native_thread);`启动线程的流程：

```
// hotspot/src/share/vm/runtime/thread.cpp

void Thread::start(Thread* thread) {
  trace("start", thread);
  if (!DisableStartThread) {
    if (thread->is_Java_thread()) { //如果是用户创建的java线程，在启动线程之前，将线程状态初始化为 RUNNABLE
      java_lang_Thread::set_thread_status(((JavaThread*)thread)->threadObj(),
                                          java_lang_Thread::RUNNABLE);
    }
    os::start_thread(thread); //启动线程
  }
}
```

这里有2个重点：

1.  如果是用户创建的java线程，设置线程状态为RUNNABLE，注意这里设置的是java.lang.Thread.java中的threadStatus属性。
2.  `oss::start_thread(thread);`启动线程

```
// hotspot/src/share/vm/runtime/os.cpp

void os::start_thread(Thread* thread) {
  MutexLockerEx ml(thread->SR_lock(), Mutex::_no_safepoint_check_flag); //唤醒java_start中初始化线程时的等待
  OSThread* osthread = thread->osthread();
  osthread->set_state(RUNNABLE); //设置操作系统线程状态为RUNNABLE
  pd_start_thread(thread); //启动线程
}
```

```
// hotspot/src/os/linux/vm/os_linux.cpp

void os::pd_start_thread(Thread* thread) {
  OSThread * osthread = thread->osthread();
  assert(osthread->get_state() != INITIALIZED, "just checking");
  Monitor* sync_with_child = osthread->startThread_lock();
  MutexLockerEx ml(sync_with_child, Mutex::_no_safepoint_check_flag);
  sync_with_child->notify(); //唤醒之前在create_thread中分配并挂起等待的操作系统线程
}
```

以上这么多，是创建线程时候java到jvm虚拟机做的事情，我整理了一个时序图，方便你跟踪流程做归纳总结。接下来，我们要进入操作系统线程glibc干的事了。

![在这里插入图片描述](https://img-blog.csdnimg.cn/4f59e15441724dd1b2066f58f5f85213.png#pic_center)

___

## 小结

1.  java创建线程需要与jvm进行交互，通过jni调用native方法start0()启动线程
2.  在java与jvm交互时，线程对象分为三个：java.lang.Thread对象、C++的JavaThread对象、操作系统的OSThread对象
3.  jvm在创建线程时会设置很多线程属性，如：
    -   线程栈空间大小
    -   线程detach状态
    -   线程状态（包括操作系统和）
    -   线程的回调`java.lang.Thread`的`run()`方法
    -   线程的tid
4.  创建线程时创建了C++的JavaThread对象，OSThread对象，并挂起操作系统对象，直到调用`Thread::start()`启动线程进行唤醒。
5.  调用start()后，线程的状态由INITIALIZED会转为RUNNABLE。如果创建失败，会设置为ZOMBIE状态。
6.  线程的创建，最终都会调用glibc的pthread库，接下来的文章我会详细介绍pthread干了什么，如何进入linux内核，创建内核线程的。
