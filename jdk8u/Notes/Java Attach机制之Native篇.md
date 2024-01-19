---
source: https://www.jianshu.com/p/76bb2ccadd07
---
## Java Attach机制之Native篇

[![](https://cdn2.jianshu.io/assets/default_avatar/14-0651acff782e7a18653d7530d6b27661.jpg)](https://www.jianshu.com/u/8e688d954134)

2019.05.18 00:03:49字数 1,836阅读 432

在《Java Attach机制之Java篇》中主要跟踪到了Java层的代码，下面来围绕上篇中以下3个问题去跟踪Native中的代码：  
1.这个.attach_pid<pid>文件是干嘛的？  
2.为什么还会向<pid>这个进程发送SIGQUIT命令呢，怎么看出来是发送SIGQUIT命令的呢？  
3.为什么要去等待这个socket文件的生成，这个文件生成了会怎么样？

其实问题1和3两个问题目前还没有明显的迹象，特别是问题1，这个.attach_pid文件看上去没有什么用处，而问题3，这个socket文件的生成，如果仔细跟进去看是这样的：

```
private String findSocketFile(int var1) {
        String var2 = ".java_pid" + var1;
        File var3 = new File(tmpdir, var2);
        return var3.exists()?var3.getPath():null;
    }
```

这个文件其实是用作后面跟jvm的通信用的；而问题2，跟进去后，其实是一段native代码：

```
 static native void sendQuitTo(int var0) throws IOException;
```

这里不得不再提一个初次看源码比较容易忽略掉的点，就是跨平台代码的异同，其实在《Java Attach机制之Java篇》中的代码是基于Mac下的源码，因为作者是在Mac下的IntelliJ IDEA直接查看class文件，所以就直接跳转到了Mac下的源码BsdVirtualMachine这个类，而如果是Linux的话，其实应该是LinuxVirtualMachine这个类，代码如下：

```
LinuxVirtualMachine(AttachProvider provider, String vmid)
        throws AttachNotSupportedException, IOException
    {
        super(provider, vmid);

        // This provider only understands pids
        int pid;
        try {
            pid = Integer.parseInt(vmid);
        } catch (NumberFormatException x) {
            throw new AttachNotSupportedException("Invalid process identifier");
        }

        // Find the socket file. If not found then we attempt to start the
        // attach mechanism in the target VM by sending it a QUIT signal.
        // Then we attempt to find the socket file again.
        path = findSocketFile(pid);
        if (path == null) {
            File f = createAttachFile(pid);
            try {
                // On LinuxThreads each thread is a process and we don't have the
                // pid of the VMThread which has SIGQUIT unblocked. To workaround
                // this we get the pid of the "manager thread" that is created
                // by the first call to pthread_create. This is parent of all
                // threads (except the initial thread).
                if (isLinuxThreads) {
                    int mpid;
                    try {
                        mpid = getLinuxThreadsManager(pid);
                    } catch (IOException x) {
                        throw new AttachNotSupportedException(x.getMessage());
                    }
                    assert(mpid >= 1);
                    sendQuitToChildrenOf(mpid);
                } else {
                    sendQuitTo(pid);
                }

                // give the target VM time to start the attach mechanism
                int i = 0;
                long delay = 200;
                int retries = (int)(attachTimeout() / delay);
                do {
                    try {
                        Thread.sleep(delay);
                    } catch (InterruptedException x) { }
                    path = findSocketFile(pid);
                    i++;
                } while (i <= retries && path == null);
                if (path == null) {
                    throw new AttachNotSupportedException(
                        "Unable to open socket file: target process not responding " +
                        "or HotSpot VM not loaded");
                }
            } finally {
                f.delete();
            }
        }

        // Check that the file owner/permission to avoid attaching to
        // bogus process
        checkPermissions(path);

        // Check that we can connect to the process
        // - this ensures we throw the permission denied error now rather than
        // later when we attempt to enqueue a command.
        int s = socket();
        try {
            connect(s, path);
        } finally {
            close(s);
        }
    }
```

所以如果需要根据运行的系统来看正确的代码，才能正确理解运行过程，虽然这里就attach机制而言的话，偏差不会太大，但是方法还是很重要的；  
下面主要会以Linux系统为主去解析Native的源码，截取上面的关键代码来分析：

```
// On LinuxThreads each thread is a process and we don't have the
                // pid of the VMThread which has SIGQUIT unblocked. To workaround
                // this we get the pid of the "manager thread" that is created
                // by the first call to pthread_create. This is parent of all
                // threads (except the initial thread).
                if (isLinuxThreads) {
                    int mpid;
                    try {
                        mpid = getLinuxThreadsManager(pid);
                    } catch (IOException x) {
                        throw new AttachNotSupportedException(x.getMessage());
                    }
                    assert(mpid >= 1);
                    sendQuitToChildrenOf(mpid);
                } else {
                    sendQuitTo(pid);
                }
```

上面的代码的IF分支，会让人感到疑惑，其实主要是判断isLinuxThreads这个变量，代码是这样的：

```

    static native boolean isLinuxThreads();
    static {
        System.loadLibrary("attach");
        isLinuxThreads = isLinuxThreads();
    }
```

很显然这又牵扯到一个native方法，那么有个简单的办法就是在linux系统上去，去反射调用isLinuxThreads这个方法来获取它的值，你就会获取到false这个结果，但是起初获取到这个值的时候，我也很疑惑，为什么呢，这个难道不是linux的线程吗？最后通过跟踪isLinuxThreads()这个方法的源码实现，终于找到结果：

```
JNIEXPORT jboolean JNICALL Java_sun_tools_attach_LinuxVirtualMachine_isLinuxThreads
  (JNIEnv *env, jclass cls)
{
# ifndef _CS_GNU_LIBPTHREAD_VERSION
# define _CS_GNU_LIBPTHREAD_VERSION 3
# endif
    size_t n;
    char* s;
    jboolean res;

    n = confstr(_CS_GNU_LIBPTHREAD_VERSION, NULL, 0);
    if (n <= 0) {
       /* glibc before 2.3.2 only has LinuxThreads */
       return JNI_TRUE;
    }

    s = (char *)malloc(n);
    if (s == NULL) {
        JNU_ThrowOutOfMemoryError(env, "malloc failed");
        return JNI_TRUE;
    }
    confstr(_CS_GNU_LIBPTHREAD_VERSION, s, n);

    /*
     * If the LIBPTHREAD version include "NPTL" then we know we
     * have the new threads library and not LinuxThreads
     */
    res = (jboolean)(strstr(s, "NPTL") == NULL);
    free(s);
    return res;
}
```

简单理解下，就是根据linux系统的glibc的版本来判断的，NPTL已经是非常古老的linux内核架构下的线程模型了，所以如今的系统都是最新的线程模型，从而到这里就可以确定结果了；  
所以我们跳出刚才的层层分析，还是回到正题：

```
 static native void sendQuitTo(int var0) throws IOException;
```

它的实现是怎样的？

```
/*
 * Class:     sun_tools_attach_LinuxVirtualMachine
 * Method:    sendQuitTo
 * Signature: (I)V
 */
JNIEXPORT void JNICALL Java_sun_tools_attach_LinuxVirtualMachine_sendQuitTo
  (JNIEnv *env, jclass cls, jint pid)
{
    if (kill((pid_t)pid, SIGQUIT)) {
        JNU_ThrowIOExceptionWithLastError(env, "kill");
    }
}
```

看到这里，对kill这个linux命令熟悉的就已经知道在干什么了，就是向这个pid的java进程发送了SIGQUIT这个信号量,它是3号信号量,其实就是一个类似终止的命令，可以参考这个链接：  
[[https://www.gnu.org/software/libc/manual/html_node/Termination-Signals.html]](https://links.jianshu.com/go?to=https%3A%2F%2Fwww.gnu.org%2Fsoftware%2Flibc%2Fmanual%2Fhtml_node%2FTermination-Signals.html%255D)

其实看到这里又产生了更大的疑惑，attach机制到最后竟然成了杀死java进程的凶手？  
那么很显然，从实践来看java进程非但没死，而且仍然活的好好的，所以这就需要了解  
JVM对这个系统的3号信号到底做了怎样的操作呢，这里先引出两个参数-Xrs、-XX:+ReduceSignalUsage,其实这两个参数是同一个意思，如果在jvm启动的时候加上了，就代表需要关闭jvm对于系统信号量的处理，而默认情况下jvm对系统的一些特殊信号量是有特殊处理的，这也就解释了，为什么向jvm进程发送3号信号量也没有被杀死了，下面来看具体的jvm是怎么处理这个信号量的：  
在os.cpp中有下面一段代码就是用来处理信号量的：

```
static void signal_thread_entry(JavaThread* thread, TRAPS) {
  os::set_priority(thread, NearMaxPriority);
  while (true) {
    int sig;
    {
      // FIXME : Currently we have not decieded what should be the status
      //         for this java thread blocked here. Once we decide about
      //         that we should fix this.
      sig = os::signal_wait();
    }
    if (sig == os::sigexitnum_pd()) {
       // Terminate the signal thread
       return;
    }

    switch (sig) {
      case SIGBREAK: {
        // Check if the signal is a trigger to start the Attach Listener - in that
        // case don't print stack traces.
        if (!DisableAttachMechanism && AttachListener::is_init_trigger()) {
          continue;
        }
        // Print stack traces
        // Any SIGBREAK operations added here should make sure to flush
        // the output stream (e.g. tty->flush()) after output.  See 4803766.
        // Each module also prints an extra carriage return after its output.
        VM_PrintThreads op;
        VMThread::execute(&op);
        VM_PrintJNI jni_op;
        VMThread::execute(&jni_op);
        VM_FindDeadlocks op1(tty);
        VMThread::execute(&op1);
        Universe::print_heap_at_SIGBREAK();
        if (PrintClassHistogram) {
          VM_GC_HeapInspection op1(gclog_or_tty, true /* force full GC before heap inspection */,
                                   true /* need_prologue */);
          VMThread::execute(&op1);
        }
        if (JvmtiExport::should_post_data_dump()) {
          JvmtiExport::post_data_dump();
        }
        break;
      }
      default: {
        // Dispatch the signal to java
        HandleMark hm(THREAD);
        klassOop k = SystemDictionary::resolve_or_null(vmSymbols::sun_misc_Signal(), THREAD);
        KlassHandle klass (THREAD, k);
        if (klass.not_null()) {
          JavaValue result(T_VOID);
          JavaCallArguments args;
          args.push_int(sig);
          JavaCalls::call_static(
            &result,
            klass,
            vmSymbols::dispatch_name(),
            vmSymbols::int_void_signature(),
            &args,
            THREAD
          );
        }
        if (HAS_PENDING_EXCEPTION) {
          // tty is initialized early so we don't expect it to be null, but
          // if it is we can't risk doing an initialization that might
          // trigger additional out-of-memory conditions
          if (tty != NULL) {
            char klass_name[256];
            char tmp_sig_name[16];
            const char* sig_name = "UNKNOWN";
            instanceKlass::cast(PENDING_EXCEPTION->klass())->
              name()->as_klass_external_name(klass_name, 256);
            if (os::exception_name(sig, tmp_sig_name, 16) != NULL)
              sig_name = tmp_sig_name;
            warning("Exception %s occurred dispatching signal %s to handler"
                    "- the VM may need to be forcibly terminated",
                    klass_name, sig_name );
          }
          CLEAR_PENDING_EXCEPTION;
        }
      }
    }
  }
}

```

代码中的SIGBREAK在这里就相当于是SIGQUIT，是以一段宏定义来声明的：

```
// SIGBREAK is sent by the keyboard to query the VM state
#ifndef SIGBREAK
#define SIGBREAK SIGQUIT
#endif
```

下面首先分析这段关键代码：

```
 if (!DisableAttachMechanism && AttachListener::is_init_trigger()) {
          continue;
        }
```

这里的DisableAttachMechanism其实也是JVM的一个参数，默认情况下这个是Enable的，进入  
attachListener_linux.cpp文件中的AttachListener::is_init_trigger()这个函数:

```
// If the file .attach_pid<pid> exists in the working directory
// or /tmp then this is the trigger to start the attach mechanism
bool AttachListener::is_init_trigger() {
  if (init_at_startup() || is_initialized()) {
    return false;               // initialized at startup or already initialized
  }
  char fn[PATH_MAX+1];
  sprintf(fn, ".attach_pid%d", os::current_process_id());
  int ret;
  struct stat64 st;
  RESTARTABLE(::stat64(fn, &st), ret);
  if (ret == -1) {
    snprintf(fn, sizeof(fn), "%s/.attach_pid%d",
             os::get_temp_directory(), os::current_process_id());
    RESTARTABLE(::stat64(fn, &st), ret);
  }
  if (ret == 0) {
    // simple check to avoid starting the attach mechanism when
    // a bogus user creates the file
    if (st.st_uid == geteuid()) {
      init();
      return true;
    }
  }
  return false;
}
```

那么其实到这里，就和.attach_pid<pid>文件串联起来了，对，终于串联起来了，这里的逻辑是这样的，如果已经初始化过了，或者这个文件不存在或者这个文件存在，但是文件的权限不对，那么统一返回false，只有在第一次初始化，并且文件的权限正确的情况下，才会返回true；  
回到os.cpp中的代码：

```
// Check if the signal is a trigger to start the Attach Listener - in that
        // case don't print stack traces.
        if (!DisableAttachMechanism && AttachListener::is_init_trigger()) {
          continue;
        }
```

如果IF分支为true，则继续回到第二次循环，而如果为false，则会执行下面代码：

```
// Print stack traces
        // Any SIGBREAK operations added here should make sure to flush
        // the output stream (e.g. tty->flush()) after output.  See 4803766.
        // Each module also prints an extra carriage return after its output.
        VM_PrintThreads op;
        VMThread::execute(&op);
        VM_PrintJNI jni_op;
        VMThread::execute(&jni_op);
        VM_FindDeadlocks op1(tty);
        VMThread::execute(&op1);
        Universe::print_heap_at_SIGBREAK();
        if (PrintClassHistogram) {
          VM_GC_HeapInspection op1(gclog_or_tty, true /* force full GC before heap inspection */,
                                   true /* need_prologue */);
          VMThread::execute(&op1);
        }
        if (JvmtiExport::should_post_data_dump()) {
          JvmtiExport::post_data_dump();
        }
        break;
```

这里就会对线程进行一些扫描打印的操作，有个很关键的点需要注意下，如果JVM启动的时候加了PrintClassHistogram这个参数，那么还会引起一次FULL GC行为；  
到这里已经解决了问题1、2，剩下的问题3就在眼前了，还是看AttachListener::is_init_trigger()  
这个函数中的下面这一段：

```
if (ret == 0) {
    // simple check to avoid starting the attach mechanism when
    // a bogus user creates the file
    if (st.st_uid == geteuid()) {
      init();
      return true;
    }
  }
```

当.attach_pid文件存在，并且权限也正确（文件权限和jvm进程的权限一致），这时候就会执行  
attachListener.cpp文件中的init() 函数：

```
// Starts the Attach Listener thread
void AttachListener::init() {
  EXCEPTION_MARK;
  klassOop k = SystemDictionary::resolve_or_fail(vmSymbols::java_lang_Thread(), true, CHECK);
  instanceKlassHandle klass (THREAD, k);
  instanceHandle thread_oop = klass->allocate_instance_handle(CHECK);

  const char thread_name[] = "Attach Listener";
  Handle string = java_lang_String::create_from_str(thread_name, CHECK);

  // Initialize thread_oop to put it into the system threadGroup
  Handle thread_group (THREAD, Universe::system_thread_group());
  JavaValue result(T_VOID);
  JavaCalls::call_special(&result, thread_oop,
                       klass,
                       vmSymbols::object_initializer_name(),
                       vmSymbols::threadgroup_string_void_signature(),
                       thread_group,
                       string,
                       CHECK);

  KlassHandle group(THREAD, SystemDictionary::ThreadGroup_klass());
  JavaCalls::call_special(&result,
                        thread_group,
                        group,
                        vmSymbols::add_method_name(),
                        vmSymbols::thread_void_signature(),
                        thread_oop,             // ARG 1
                        CHECK);

  { MutexLocker mu(Threads_lock);
    JavaThread* listener_thread = new JavaThread(&attach_listener_thread_entry);

    // Check that thread and osthread were created
    if (listener_thread == NULL || listener_thread->osthread() == NULL) {
      vm_exit_during_initialization("java.lang.OutOfMemoryError",
                                    "unable to create new native thread");
    }

    java_lang_Thread::set_thread(thread_oop(), listener_thread);
    java_lang_Thread::set_daemon(thread_oop());

    listener_thread->set_threadObj(thread_oop());
    Threads::add(listener_thread);
    Thread::start(listener_thread);
  }
}
```

开始创建Attach Listener 这个线程，执行的关键在attach_listener_thread_entry这个函数中：

```
// The Attach Listener threads services a queue. It dequeues an operation
// from the queue, examines the operation name (command), and dispatches
// to the corresponding function to perform the operation.

static void attach_listener_thread_entry(JavaThread* thread, TRAPS) {
  os::set_priority(thread, NearMaxPriority);

  thread->record_stack_base_and_size();

  if (AttachListener::pd_init() != 0) {
    return;
  }
  AttachListener::set_initialized();

  for (;;) {
    AttachOperation* op = AttachListener::dequeue();
    if (op == NULL) {
      return;   // dequeue failed or shutdown
    }

    ResourceMark rm;
    bufferedStream st;
    jint res = JNI_OK;

    // handle special detachall operation
    if (strcmp(op->name(), AttachOperation::detachall_operation_name()) == 0) {
      AttachListener::detachall();
    } else {
      // find the function to dispatch too
      AttachOperationFunctionInfo* info = NULL;
      for (int i=0; funcs[i].name != NULL; i++) {
        const char* name = funcs[i].name;
        assert(strlen(name) <= AttachOperation::name_length_max, "operation <= name_length_max");
        if (strcmp(op->name(), name) == 0) {
          info = &(funcs[i]);
          break;
        }
      }

      // check for platform dependent attach operation
      if (info == NULL) {
        info = AttachListener::pd_find_operation(op->name());
      }

      if (info != NULL) {
        // dispatch to the function that implements this operation
        res = (info->func)(op, &st);
      } else {
        st.print("Operation %s not recognized!", op->name());
        res = JNI_ERR;
      }
    }

    // operation complete - send result and output to client
    op->complete(res, &st);
  }
}
```

先来看AttachListener::pd_init() 这个方法,在attachListener_linux.cpp文件中,

```
int AttachListener::pd_init() {
  JavaThread* thread = JavaThread::current();
  ThreadBlockInVM tbivm(thread);

  thread->set_suspend_equivalent();
  // cleared by handle_special_suspend_equivalent_condition() or
  // java_suspend_self() via check_and_wait_while_suspended()

  int ret_code = LinuxAttachListener::init();

  // were we externally suspended while we were waiting?
  thread->check_and_wait_while_suspended();

  return ret_code;
}
```

然后看LinuxAttachListener::init()这个函数：

```
// Initialization - create a listener socket and bind it to a file

int LinuxAttachListener::init() {
  char path[UNIX_PATH_MAX];          // socket file
  char initial_path[UNIX_PATH_MAX];  // socket file during setup
  int listener;                      // listener socket (file descriptor)

  // register function to cleanup
  ::atexit(listener_cleanup);

  int n = snprintf(path, UNIX_PATH_MAX, "%s/.java_pid%d",
                   os::get_temp_directory(), os::current_process_id());
  if (n < (int)UNIX_PATH_MAX) {
    n = snprintf(initial_path, UNIX_PATH_MAX, "%s.tmp", path);
  }
  if (n >= (int)UNIX_PATH_MAX) {
    return -1;
  }

  // create the listener socket
  listener = ::socket(PF_UNIX, SOCK_STREAM, 0);
  if (listener == -1) {
    return -1;
  }

  // bind socket
  struct sockaddr_un addr;
  addr.sun_family = AF_UNIX;
  strcpy(addr.sun_path, initial_path);
  ::unlink(initial_path);
  int res = ::bind(listener, (struct sockaddr*)&addr, sizeof(addr));
  if (res == -1) {
    RESTARTABLE(::close(listener), res);
    return -1;
  }

  // put in listen mode, set permissions, and rename into place
  res = ::listen(listener, 5);
  if (res == 0) {
      RESTARTABLE(::chmod(initial_path, S_IREAD|S_IWRITE), res);
      if (res == 0) {
          res = ::rename(initial_path, path);
      }
  }
  if (res == -1) {
    RESTARTABLE(::close(listener), res);
    ::unlink(initial_path);
    return -1;
  }
  set_path(path);
  set_listener(listener);

  return 0;
}
```

踏尽千山万水，终于找到关键指出，没错这里就是我们开头提到的问题3，socket文件的创建：初始化套接字，开启监听（其实这里并不是tcp的方式，而是unix domain socket的连接），由jvm作为server端,再回到attach_listener_thread_entry这个函数中,看下面这段for循环代码：

```
for (;;) {
    AttachOperation* op = AttachListener::dequeue();
    if (op == NULL) {
      return;   // dequeue failed or shutdown
    }

    ResourceMark rm;
    bufferedStream st;
    jint res = JNI_OK;

    // handle special detachall operation
    if (strcmp(op->name(), AttachOperation::detachall_operation_name()) == 0) {
      AttachListener::detachall();
    } else {
      // find the function to dispatch too
      AttachOperationFunctionInfo* info = NULL;
      for (int i=0; funcs[i].name != NULL; i++) {
        const char* name = funcs[i].name;
        assert(strlen(name) <= AttachOperation::name_length_max, "operation <= name_length_max");
        if (strcmp(op->name(), name) == 0) {
          info = &(funcs[i]);
          break;
        }
      }

      // check for platform dependent attach operation
      if (info == NULL) {
        info = AttachListener::pd_find_operation(op->name());
      }

      if (info != NULL) {
        // dispatch to the function that implements this operation
        res = (info->func)(op, &st);
      } else {
        st.print("Operation %s not recognized!", op->name());
        res = JNI_ERR;
      }
    }

    // operation complete - send result and output to client
    op->complete(res, &st);
  }
```

这一段代码稍微拆解下，其实就是从socket的套接字中不断等待客户端连接，然后取出发过来的操作内容，最后进行处理，可以看AttachListener::dequeue()这个操作：

```
// Dequeue an operation
//
// In the Linux implementation there is only a single operation and clients
// cannot queue commands (except at the socket level).
//
LinuxAttachOperation* LinuxAttachListener::dequeue() {
  for (;;) {
    int s;

    // wait for client to connect
    struct sockaddr addr;
    socklen_t len = sizeof(addr);
    RESTARTABLE(::accept(listener(), &addr, &len), s);
    if (s == -1) {
      return NULL;      // log a warning?
    }

    // get the credentials of the peer and check the effective uid/guid
    // - check with jeff on this.
    struct ucred cred_info;
    socklen_t optlen = sizeof(cred_info);
    if (::getsockopt(s, SOL_SOCKET, SO_PEERCRED, (void*)&cred_info, &optlen) == -1) {
      int res;
      RESTARTABLE(::close(s), res);
      continue;
    }
    uid_t euid = geteuid();
    gid_t egid = getegid();

    if (cred_info.uid != euid || cred_info.gid != egid) {
      int res;
      RESTARTABLE(::close(s), res);
      continue;
    }

    // peer credential look okay so we read the request
    LinuxAttachOperation* op = read_request(s);
    if (op == NULL) {
      int res;
      RESTARTABLE(::close(s), res);
      continue;
    } else {
      return op;
    }
  }
}
```

最后来看下客户端具体包含了哪些操作，可以看到AttachOperationFunctionInfo这个结构体：

```
// Table to map operation names to functions.

// names must be of length <= AttachOperation::name_length_max
static AttachOperationFunctionInfo funcs[] = {
  { "agentProperties",  get_agent_properties },
  { "datadump",         data_dump },
  { "dumpheap",         dump_heap },
  { "load",             JvmtiExport::load_agent_library },
  { "properties",       get_system_properties },
  { "threaddump",       thread_dump },
  { "inspectheap",      heap_inspection },
  { "setflag",          set_flag },
  { "printflag",        print_flag },
  { "jcmd",             jcmd },
  { NULL,               NULL }
};
```

Native篇的Attach机制差不多就列举到这里，下一篇开始就会具体去看看里面的这些操作到底在干了一些什么事情，以及分析jstack、jmap这些工具类到底是做了什么。

最后要说明一点，这篇的记载也是在参照了一些大神的文章基础上去整理的，并没有任何创新点，只是供自己和别人以后用到的时候可以参考一下  
Ref：  
[http://lovestblog.cn/blog/2014/06/18/jvm-attach/](https://links.jianshu.com/go?to=http%3A%2F%2Flovestblog.cn%2Fblog%2F2014%2F06%2F18%2Fjvm-attach%2F)  
[https://www.cnblogs.com/scofield-1987/p/9347586.html](https://links.jianshu.com/go?to=https%3A%2F%2Fwww.cnblogs.com%2Fscofield-1987%2Fp%2F9347586.html)

最后编辑于

：2019.05.18 00:07:51

更多精彩内容，就在简书APP

![](https://upload.jianshu.io/images/js-qrc.png)

"小礼物走一走，来简书关注我"

还没有人赞赏，支持一下

[![  ](https://cdn2.jianshu.io/assets/default_avatar/14-0651acff782e7a18653d7530d6b27661.jpg)](https://www.jianshu.com/u/8e688d954134)

-   序言：七十年代末，一起剥皮案震惊了整个滨河市，随后出现的几起案子，更是在滨河造成了极大的恐慌，老刑警刘岩，带你破解...
    
-   1. 周嘉洛拥有一个不好说有没有用的异能。 异能管理局的人给他的异能起名为「前方高能预警」。 2. 周嘉洛第一次发...
    
-   序言：滨河连续发生了三起死亡事件，死亡现场离奇诡异，居然都是意外死亡，警方通过查阅死者的电脑和手机，发现死者居然都...
    
-   文/潘晓璐 我一进店门，熙熙楼的掌柜王于贵愁眉苦脸地迎上来，“玉大人，你说我怎么就摊上这事。” “怎么了？”我有些...
    
-   文/不坏的土叔 我叫张陵，是天一观的道长。 经常有香客问我，道长，这世上最难降的妖魔是什么？ 我笑而不...
    
-   正文 为了忘掉前任，我火速办了婚礼，结果婚礼上，老公的妹妹穿的比我还像新娘。我一直安慰自己，他们只是感情好，可当我...
    
    [![](https://upload.jianshu.io/users/upload_avatars/4790772/388e473c-fe2f-40e0-9301-e357ae8f1b41.jpeg?imageMogr2/auto-orient/strip|imageView2/1/w/48/h/48/format/webp)茶点故事](https://www.jianshu.com/u/0f438ff0a55f)阅读 12,886评论 0赞 101
    
-   文/花漫 我一把揭开白布。 她就那样静静地躺着，像睡着了一般。 火红的嫁衣衬着肌肤如雪。 梳的纹丝不乱的头发上，一...
    
-   那天，我揣着相机与录音，去河边找鬼。 笑死，一个胖子当着我的面吹牛，可吹牛的内容都是我干的。 我是一名探鬼主播，决...
    
-   文/苍兰香墨 我猛地睁开眼，长吁一口气：“原来是场噩梦啊……” “哼！你这毒妇竟也来了？” 一声冷哼从身侧响起，我...
    
-   想象着我的养父在大火中拼命挣扎，窒息，最后皮肤化为焦炭。我心中就已经是抑制不住地欢快，这就叫做以其人之道，还治其人...
    
-   序言：老挝万荣一对情侣失踪，失踪者是张志新（化名）和其女友刘颖，没想到半个月后，有当地人在树林里发现了一具尸体，经...
    
-   正文 独居荒郊野岭守林人离奇死亡，尸身上长有42处带血的脓包…… 初始之章·张勋 以下内容为张勋视角 年9月15日...
    
    [![](https://upload.jianshu.io/users/upload_avatars/4790772/388e473c-fe2f-40e0-9301-e357ae8f1b41.jpeg?imageMogr2/auto-orient/strip|imageView2/1/w/48/h/48/format/webp)茶点故事](https://www.jianshu.com/u/0f438ff0a55f)阅读 5,602评论 1赞 91
    
-   正文 我和宋清朗相恋三年，在试婚纱的时候发现自己被绿了。 大学时的朋友给我发了我未婚夫和他白月光在一起吃饭的照片。...
    
    [![](https://upload.jianshu.io/users/upload_avatars/4790772/388e473c-fe2f-40e0-9301-e357ae8f1b41.jpeg?imageMogr2/auto-orient/strip|imageView2/1/w/48/h/48/format/webp)茶点故事](https://www.jianshu.com/u/0f438ff0a55f)阅读 6,028评论 0赞 80
    
-   白月光回国，霸总把我这个替身辞退。还一脸阴沉的警告我。[不要出现在思思面前， 不然我有一百种方法让你生不如死。]我...
    
-   序言：一个原本活蹦乱跳的男人离奇死亡，死状恐怖，灵堂内的尸体忽然破棺而出，到底是诈尸还是另有隐情，我是刑警宁泽，带...
    
-   正文 年R本政府宣布，位于F岛的核电站，受9级特大地震影响，放射性物质发生泄漏。R本人自食恶果不足惜，却给世界环境...
    
    [![](https://upload.jianshu.io/users/upload_avatars/4790772/388e473c-fe2f-40e0-9301-e357ae8f1b41.jpeg?imageMogr2/auto-orient/strip|imageView2/1/w/48/h/48/format/webp)茶点故事](https://www.jianshu.com/u/0f438ff0a55f)阅读 6,251评论 3赞 77
    
-   文/蒙蒙 一、第九天 我趴在偏房一处隐蔽的房顶上张望。 院中可真热闹，春花似锦、人声如沸。这庄子的主人今日做“春日...
    
-   文/苍兰香墨 我抬头看了看天上的太阳。三九已至，却和暖如春，着一层夹袄步出监牢的瞬间，已是汗流浃背。 一阵脚步声响...
    
-   我被黑心中介骗来泰国打工， 没想到刚下飞机就差点儿被人妖公主榨干…… 1. 我叫王不留，地道东北人。 一个月前我还...
    
-   正文 我出身青楼，却偏偏与公主长得像，于是被迫代替她去往敌国和亲。 传闻我的和亲对象是个残疾皇子，可洞房花烛夜当晚...
    
    [![](https://upload.jianshu.io/users/upload_avatars/4790772/388e473c-fe2f-40e0-9301-e357ae8f1b41.jpeg?imageMogr2/auto-orient/strip|imageView2/1/w/48/h/48/format/webp)茶点故事](https://www.jianshu.com/u/0f438ff0a55f)阅读 7,509评论 1赞 99
