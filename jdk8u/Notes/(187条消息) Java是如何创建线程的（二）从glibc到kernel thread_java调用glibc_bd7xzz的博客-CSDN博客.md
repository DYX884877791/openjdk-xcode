---
source: https://bd7xzz.blog.csdn.net/article/details/128988650?ydreferer=aHR0cHM6Ly9ibG9nLmNzZG4ubmV0L2tpZF8yNDEyL2FydGljbGUvZGV0YWlscy8xMjg4OTA5MDg%3D
---
## Java是如何创建线程的（二）从glibc到kernel thread

## 背景

上一节我们讨论了java线程是如何创建的，看了看从java代码层面到jvm层面的源码里都干了什么。  
整个流程还是比较复杂的，我将上一节总结的调用时序图贴在下面，方便你回忆起整体调用流程。这一节，我们再来详细看看glibc到linux kernel是如何创建线程的。  
![在这里插入图片描述](https://img-blog.csdnimg.cn/fa5588d2c4f343b2a1817d5794f679b0.png)

这篇文章会涉及到一点汇编指令，因为我们要看看线程是如何进入内核态的。不用担心看不懂，我会写上注释，也不用死记硬背，只要知道线程进入内核态时是通过汇编指令进入的即可。

___

## 先验知识

基本概念：

-   nptl即 Native POSIX Threads Library 基于POSIX标准的本地线程库
-   POSIX即 Portable Operating System Interface of UNIX UNIX可移植操作系统接口，也是一套标准
-   pthread即POSIX thread 基于POSIX标准的线程库
-   rq即runqueue，LInux为每个CPU配备一个队列，会将线程放进去，调度器会基于这个队列进行调度

几个问题：

-   为什么是glibc实现的用户态线程？  
    glibc提供了具体的pthread线程库的用户态实现，可兼容主流的操作系统，这样确保了用户态线程的可移植性。程序员只要基于pthread库编写多线程代码，在不同操作系统平台上编译即可。
    
-   上下文到底是什么意思？  
    context（上下文），在Java中有，如spring context，同样在操作系统中也经常出现，如：多线程切换保存线程的上下文，系统调用进入内核态也要保存上下文。本质上，上下文代表着运行时刻所需要的数据，这些数据通过内存中的一个数据结构或者多个数据结构组成。对于操作系统来说，2个线程在一个CPU核心上执行，是需要保存当前线程的上下文，A执行一段CPU时间片后让出CPU，交给B线程执行，此时要保存上下文，在B线程分片时间执行完成后，会恢复A线程的上下文继续执行。对于系统调用，如创建线程，调度线程都需要通过系统调用进入内核态，具体的代码由内核线程执行，在系统调用时，需要保存用户态的上下文并进入内核态执行代码，在内核态执行完成后，恢复之前用户态保存的上下文，继续执行用户态的代码。上下文切换多，就会耗费较多的系统资源。
    
-   syscall该如何找到对应的处理函数？  
    syscall（系统调用）是用户态进入内核态的桥梁，Linux的系统调用由汇编代码实现。在Linux系统启动时，会注册好系统调用对应的内核态处理函数。我们可以通过 [64位syscall](https://syscalls64.paolostivanin.com/) 这个在线网站快速查看系统调用对应的处理函数。  
    _读者需要注意，Linux64位和32位的系统调用略有不同，[32位syscall](https://syscalls32.paolostivanin.com/)这个在线网站查看32位的系统调用处理函数。_
    

___

## 分析

### 从glibc到kernel thread

上一节我们分析到JVM会通过`pthread_create`这个函数创建线程，那么我们就从这里开始，看看glibc在用户态创建线程都干了啥。  
glibc2.36的源码可以在[bootlin glibc-2.36](https://elixir.bootlin.com/glibc/glibc-2.36/source)上直接查看。

`pthread_create`函数声明如下：

```
// sysdeps/nptl/pthread.h

extern int pthread_create (pthread_t *__restrict __newthread,
   const pthread_attr_t *__restrict __attr,
   void *(*__start_routine) (void *),
   void *__restrict __arg) __THROWNL __nonnull ((1, 3));
```

第一个参数为`pthread_t`结构的地址，第二个参数为`pthread_attr_t`线程属性的地址，第三个参数为执行业务代码的函数地址，第三个参数为函数参数，当然也是个地址。

`pthread_create`比较有趣，是由`pthread_create.c`这个文件实现的，并由通过一个versioned_symbol宏，将glibc的版本与pthread_create映射到了对应的函数实现上。

```
// nptl/pthread_create.c

versioned_symbol (libc, __pthread_create_2_1, pthread_create, GLIBC_2_34); 
```

versioned_symbol是由一串宏拼出来的。而最后的_set_symbol_version则是调用了链接指令.symver来实现最终在链接时把版本和函数实现进行绑定。 所以`pthread_create`在编译时，根据不同的glibc版本编译成`__pthread_create_2_1` 或`__pthread_create_2_0`。

```
// include/shlib-compat.h
# define versioned_symbol(lib, local, symbol, version) \
  versioned_symbol_1 (lib, local, symbol, version)
# define versioned_symbol_1(lib, local, symbol, version) \
  versioned_symbol_2 (local, symbol, VERSION_##lib##_##version)
# define versioned_symbol_2(local, symbol, name) \
  default_symbol_version (local, symbol, name)
  
// include/libc-symbols.h
# define default_symbol_version(real, name, version) \
     _default_symbol_version(real, name, version)
# ifdef __ASSEMBLER__
#  define _default_symbol_version(real, name, version) \
  _set_symbol_version (real, name@@version)
# else
#  define _default_symbol_version(real, name, version) \
  _set_symbol_version (real, #name "@@" #version)
# endif

// sysdeps/generic/libc-symver.h
# ifdef __ASSEMBLER__
#  define _set_symbol_version(real, name_version) \
  .symver real, name_version
# else
#  define _set_symbol_version(real, name_version) \
  __asm__ (".symver " #real "," name_version)
# endif
```

我们直接看`__pthread_create_2_1`函数实现，这个函数比较长，我留下比较重要的代码做分析，其他的请读者自行查看：

```
// nptl/pthread_create.c

int
__pthread_create_2_1 (pthread_t *newthread, const pthread_attr_t *attr,
      void *(*start_routine) (void *), void *arg)
{
  void *stackaddr = NULL;
  size_t stacksize = 0;
  
  const struct pthread_attr *iattr = (struct pthread_attr *) attr; 

  struct pthread *pd = NULL;
  int err = allocate_stack (iattr, &pd, &stackaddr, &stacksize); //分配线程栈空间，注意这里用到了之前设置的线程属性
  int retval = 0;

#if TLS_TCB_AT_TP //初始化线程控制块tcb
  pd->header.self = pd;
  pd->header.tcb = pd;
#endif

 pd->start_routine = start_routine; //设置业务处理函数
  pd->arg = arg; //设置处理函数的参数

  struct pthread *self = THREAD_SELF;
  pd->flags = ((iattr->flags & ~(ATTR_FLAG_SCHED_SET | ATTR_FLAG_POLICY_SET)) //根据线程属性中的设置优先级
       | (self->flags & (ATTR_FLAG_SCHED_SET | ATTR_FLAG_POLICY_SET)));

  pd->joinid = iattr->flags & ATTR_FLAG_DETACHSTATE ? pd : NULL; //根据线程属性设置joinid
  pd->eventbuf = self->eventbuf; //设置线程的event_buf
  pd->schedpolicy = self->schedpolicy; //设置调度策略
  pd->schedparam = self->schedparam; //设置调度参数

//拷贝栈保护区
#ifdef THREAD_COPY_STACK_GUARD 
  THREAD_COPY_STACK_GUARD (pd);
#endif

//拷贝指针保护区
#ifdef THREAD_COPY_POINTER_GUARD 
  THREAD_COPY_POINTER_GUARD (pd);
#endif

  tls_setup_tcbhead (pd); //设置tcb头节点

//根据线程属性中的设置，设置调度参数和策略
  if (__builtin_expect ((iattr->flags & ATTR_FLAG_NOTINHERITSCHED) != 0, 0)
      && (iattr->flags & (ATTR_FLAG_SCHED_SET | ATTR_FLAG_POLICY_SET)) != 0)
    {
      if (iattr->flags & ATTR_FLAG_POLICY_SET)
        {
          pd->schedpolicy = iattr->schedpolicy;
          pd->flags |= ATTR_FLAG_POLICY_SET;
        }
      if (iattr->flags & ATTR_FLAG_SCHED_SET)
        {
          pd->schedparam = iattr->schedparam;
          pd->flags |= ATTR_FLAG_SCHED_SET;
        }

      if ((pd->flags & (ATTR_FLAG_SCHED_SET | ATTR_FLAG_POLICY_SET))
          != (ATTR_FLAG_SCHED_SET | ATTR_FLAG_POLICY_SET))
        collect_default_sched (pd);
    }


  *newthread = (pthread_t) pd; //把pthread_t描述符传出去

  internal_sigset_t original_sigmask; 
  internal_signal_block_all (&original_sigmask); //屏蔽掉所有信号

  if (iattr->extension != NULL && iattr->extension->sigmask_set) //根据线程属性设置信号掩码
    internal_sigset_from_sigset (&pd->sigmask, &iattr->extension->sigmask);
  else
    {
      pd->sigmask = original_sigmask;
      internal_sigdelset (&pd->sigmask, SIGCANCEL);
    }

    retval = create_thread (pd, iattr, &stopped_start, stackaddr, //调用create_thread创建
    stacksize, &thread_ran);

  internal_signal_restore_set (&original_sigmask);//重置信号掩码

  if (__glibc_unlikely (retval != 0)) //异常处理
    {
      if (thread_ran)
       {
  assert (stopped_start);
  pd->setup_failed = 1;
  lll_unlock (pd->lock, LLL_PRIVATE);

  pid_t tid;
  while ((tid = atomic_load_acquire (&pd->tid)) != 0)
    __futex_abstimed_wait_cancelable64 ((unsigned int *) &pd->tid,
tid, 0, NULL, LLL_SHARED);
        }

      atomic_decrement (&__nptl_nthreads);

      __nptl_deallocate_stack (pd);

      if (retval == ENOMEM)
retval = EAGAIN;
    }
  else
    {
      if (stopped_start)
lll_unlock (pd->lock, LLL_PRIVATE);

      THREAD_SETMEM (THREAD_SELF, header.multiple_threads, 1);
    }

 out:
  if (destroy_default_attr)
    __pthread_attr_destroy (&default_attr.external);

  return retval;
}
```

从一坨代码中找到几个重点：

1.  通过`allocate_stack`分配线程栈空间
2.  初始化TCB线程控制块，TCB本质上就是`pthread`结构，并串成个链表方便操作系统管理线程
3.  拷贝了一堆东西：
    -   保护区，防止线程栈或地址非法访问
    -   线程属性，包括优先级、调度参数、joinid等等
4.  屏蔽信号，避免了线程启动时出现竞争
5.  通过`create_thread`创建线程

`allocate_stack`函数比较长，这里不摘出来了，有兴趣的读者可以自己看下，这里说一下主要做了什么：

1.  如果线程属性设置了线程栈大小，用用户设置的，否则用默认的线程栈大小，你可以通过ulimit -s查看，也可以通过修改/etc/scurity/limit.conf 来调整线程栈大小。
2.  在栈末尾放一个保护区，用户栈越界到这里抛出异常。
3.  在进程的堆内存中划分出栈，如果线程在频繁创建和销毁频繁的通过malloc或__mmap（用于分配大内存空间），分配栈内存空间成本比较大，所以会对分配过的栈空间进行缓存，并通过`get_cached_stack`检查是否有缓存过，缓存过直接拿来用，没有则进行分配。
4.  设置pthread结构体中的成员，并放在栈底部。注意线程栈是自顶向下的，所以栈底部是内存地址最高位。
5.  操作系统内存中维护了2个链表`stack_used`、`stack_cache`，`stack_used`表示栈正在被使用，`stack_cache`表示被缓存起来可以被复用。

我们重点看下`create_thread`这个函数：

```
// nptl/pthread_create.c

static int create_thread (struct pthread *pd, const struct pthread_attr *attr,
  bool *stopped_start, void *stackaddr,
  size_t stacksize, bool *thread_ran)
{
  bool need_setaffinity = (attr != NULL && attr->extension != NULL
   && attr->extension->cpuset != 0); //检查CPU亲和性
  if (attr != NULL
      && (__glibc_unlikely (need_setaffinity)
  || __glibc_unlikely ((attr->flags & ATTR_FLAG_NOTINHERITSCHED) != 0))) //设置了CPU亲和性标记stopped_start为true，创建新线程后停止，让CPU根据亲和性参数调度线程启动
    *stopped_start = true;

  pd->stopped_start = *stopped_start;
  if (__glibc_unlikely (*stopped_start))
    lll_lock (pd->lock, LLL_PRIVATE);

//设置clone标记
  const int clone_flags = (CLONE_VM | CLONE_FS | CLONE_FILES | CLONE_SYSVSEM
   | CLONE_SIGHAND | CLONE_THREAD 
   | CLONE_SETTLS | CLONE_PARENT_SETTID
   | CLONE_CHILD_CLEARTID
   | 0); 

  TLS_DEFINE_INIT_TP (tp, pd);

  struct clone_args args =
    {
      .flags = clone_flags,
      .pidfd = (uintptr_t) &pd->tid,
      .parent_tid = (uintptr_t) &pd->tid,
      .child_tid = (uintptr_t) &pd->tid,
      .stack = (uintptr_t) stackaddr,
      .stack_size = stacksize,
      .tls = (uintptr_t) tp,
    }; //clone参数
  int ret = __clone_internal (&args, &start_thread, pd); //执行clone
  if (__glibc_unlikely (ret == -1))
    return errno;

  *thread_ran = true; //标记线程直接启动

//设置cpu调度参数
  if (attr != NULL)
    {
      if (need_setaffinity)
{
  assert (*stopped_start);

  int res = INTERNAL_SYSCALL_CALL (sched_setaffinity, pd->tid, 
   attr->extension->cpusetsize,
   attr->extension->cpuset);
  if (__glibc_unlikely (INTERNAL_SYSCALL_ERROR_P (res)))
    return INTERNAL_SYSCALL_ERRNO (res);
}

      if ((attr->flags & ATTR_FLAG_NOTINHERITSCHED) != 0)
{
  assert (*stopped_start);

  int res = INTERNAL_SYSCALL_CALL (sched_setscheduler, pd->tid,
   pd->schedpolicy, &pd->schedparam);
  if (__glibc_unlikely (INTERNAL_SYSCALL_ERROR_P (res)))
    return INTERNAL_SYSCALL_ERRNO (res);
}
    }

  return 0;
}
```

`create_thread`中最重要的就是设置clone参数并调用`__clone_internal`，clone参数如下：

1.  clone_flags：
    -   CLONE_VM：与父进程共享内存空间
    -   CLONE_FS：与父进程共享文件系统，包括根文件系统、当前工作目录、unmask等
    -   CLONE_FILES：与父进程共享文件描述符
    -   CLONE_SYSVSEM：与父进程共享信号量
    -   CLONE_SIGHAND：与父进程共享信号处理函数
    -   CLONE_THREAD ：父进程detached，不用等待子进程
    -   CLONE_SETTLS：初始化thread_local_storage
    -   CLONE_PARENT_SETTID：子线程的标识符写到`parent_tidptr`中
    -   CLONE_CHILD_CLEARTID：子线程退出时自己清理线程标识符
2.  其他参数：
    -   pidfd：存放pid描述符
    -   parent_tid：同上
    -   child_tid：同上
    -   stack：指向栈最低字节的指针
    -   stack_size：栈空间大小
    -   tls：thread_local_storage

`__clone_internal`实现如下：

```
// sysdeps/unix/sysv/linux/clone-internal.c

int
__clone_internal (struct clone_args *cl_args,
  int (*func) (void *arg), void *arg)
{
  int ret;
#ifdef HAVE_CLONE3_WRAPPER //采用新的clone3系统调用
  int saved_errno = errno;
  ret = __clone3 (cl_args, sizeof (*cl_args), func, arg); 
  if (ret != -1 || errno != ENOSYS)
    return ret;

  __set_errno (saved_errno);
#endif

  int flags = cl_args->flags | cl_args->exit_signal;
  void *stack = cast_to_pointer (cl_args->stack);

#ifdef __ia64__ //ia64架构采用clone2系统调用
  ret = __clone2 (func, stack, cl_args->stack_size,
  flags, arg,
  cast_to_pointer (cl_args->parent_tid),
  cast_to_pointer (cl_args->tls),
  cast_to_pointer (cl_args->child_tid));
#else
# if !_STACK_GROWS_DOWN && !_STACK_GROWS_UP
#  error "Define either _STACK_GROWS_DOWN or _STACK_GROWS_UP"
# endif

# if _STACK_GROWS_DOWN
  stack += cl_args->stack_size;
# endif 
  ret = __clone (func, stack, flags, arg, //采用老的clone系统调用
 cast_to_pointer (cl_args->parent_tid),
 cast_to_pointer (cl_args->tls),
 cast_to_pointer (cl_args->child_tid));
#endif
  return ret;
}
```

上面的代码我们可以只关注`__clone3`和`__clone`，_clone3能够接受更多的clone参数，Linux5.3之后引入的，而clone是老版5.3以下版本的_。

这里直接看进入内核态的汇编指令，其他省略，先看`__clone3`的实现：

```
// sysdeps/unix/sysv/linux/x86_64/clone3.S

ENTRY (__clone3)
movl$-EINVAL, %eax //检查clone参数是否为空
test%RDI_LP, %RDI_LP
jzSYSCALL_ERROR_LABEL //检查回调函数指针是否为空
test%RDX_LP, %RDX_LP
jzSYSCALL_ERROR_LABEL

mov%RCX_LP, %R8_LP //把clone参数放到rcx寄存器中，内核会从这取参数

movl$SYS_ify(clone3), %eax //执行syscall clone3

cfi_endproc
syscall
...
```

再看`__clone`的实现：

```
//sysdeps/unix/sysv/linux/x86_64/clone.S

ENTRY (__clone)
movq$-EINVAL,%rax //检查回调函数指针是否为空
testq%rdi,%rdi
jzSYSCALL_ERROR_LABEL

andq$-16, %rsi //检查栈空间是否为空
jzSYSCALL_ERROR_LABEL

movq%rcx,-8(%rsi) //把clone参数放入栈中

subq$16,%rsi

movq%rdi,0(%rsi) //把回调函数指针放入栈中

 //执行syscall clone
movq%rdx, %rdi
movq%r8, %rdx
movq%r9, %r8
mov8(%rsp), %R10_LP
movl$SYS_ify(clone),%eax 

cfi_endproc;
syscall
   ...
```

可以看到`__clone3`用寄存器与内核态交互，`__clone`用栈与内核态交互。

这里给出我整理了一个表格，解释下每个寄存器的作用：

`__clone3`：

| 寄存器 | 作用 |
| --- | --- |
| rdi | 存放clone参数地址 |
| rsi | 存放参数长度 |
| rdx | 存放回调函数地址 |
| rdcx | 存放回调函数参数地址 |
| rax | 存放syscall编号 |

`__clone`：

| 寄存器 | 作用 |
| --- | --- |
| rdi | 存放回调函数地址 |
| rsi | 存放线程栈空间地址 |
| rax | 存放syscall编号 |
| rdx | 存放clone_flags |
| rcx | 存放回调参数地址 |
| r8 | 存放parent_tid地址 |
| r9 | 存放tls地址 |
| r10 | 存放child_tid地址 |

以上就是用用户态做的事（包括前一篇文章我们描述的JAVA到JVM），从这以下就正式进入内核态。  
这里我画了一个时序图，简述一下glibc调用syscall之前的过程：

![在这里插入图片描述](https://img-blog.csdnimg.cn/bbf0d18d09824e1b96c88f9866563143.png)

___

接下来我们看看内核做了什么。我们通过 [64位syscall](https://syscalls64.paolostivanin.com/) 这个在线网站可以找到`sys_clone3`和`sys_clone` 俩个syscall的实现源文件。

![在这里插入图片描述](https://img-blog.csdnimg.cn/50a32f66fa674fd3892fb70ddc83b116.png)

在 [bootlin linux-6.1.9](https://elixir.bootlin.com/linux/v6.1.9/source) 中找到`fork.c`中`sys_clone3`和`sys_clone`的实现

```
// kernel/fork.c
SYSCALL_DEFINE2(clone3, struct clone_args __user *, uargs, size_t, size)
{
int err;

struct kernel_clone_args kargs;
pid_t set_tid[MAX_PID_NS_LEVEL];

kargs.set_tid = set_tid;

err = copy_clone_args_from_user(&kargs, uargs, size); //从用户态拿到clone参数
if (err)
return err;

if (!clone3_args_valid(&kargs)) //参数校验
return -EINVAL;

return kernel_clone(&kargs); //执行clone
}

// kernel/fork.c
//根据不同宏配置，定义了不同的函数签名
#ifdef CONFIG_CLONE_BACKWARDS
SYSCALL_DEFINE5(clone, unsigned long, clone_flags, unsigned long, newsp,
 int __user *, parent_tidptr,
 unsigned long, tls,
 int __user *, child_tidptr)
#elif defined(CONFIG_CLONE_BACKWARDS2)
SYSCALL_DEFINE5(clone, unsigned long, newsp, unsigned long, clone_flags,
 int __user *, parent_tidptr,
 int __user *, child_tidptr,
 unsigned long, tls)
#elif defined(CONFIG_CLONE_BACKWARDS3)
SYSCALL_DEFINE6(clone, unsigned long, clone_flags, unsigned long, newsp,
int, stack_size,
int __user *, parent_tidptr,
int __user *, child_tidptr,
unsigned long, tls)
#else
SYSCALL_DEFINE5(clone, unsigned long, clone_flags, unsigned long, newsp,
 int __user *, parent_tidptr,
 int __user *, child_tidptr,
 unsigned long, tls)
#endif
{
struct kernel_clone_args args = {
.flags= (lower_32_bits(clone_flags) & ~CSIGNAL),
.pidfd= parent_tidptr,
.child_tid= child_tidptr,
.parent_tid= parent_tidptr,
.exit_signal= (lower_32_bits(clone_flags) & CSIGNAL),
.stack= newsp,
.tls= tls,
}; //从用户态拿到clone参数

return kernel_clone(&args); //执行clone
}
 
```

这里要注意的重点是：

1.  `clone3` 用寄存器与用户态交互，`clone`用栈空间交互，所以`clone`可以从函数参数中拿到用户态参数。我们知道寄存器要比内存中的栈操作起来要快，`clone3`自然比`clone`的性能高很多。
2.  `kernel_clone`最终实现了clone。

接下来看`kernel_clone`的实现：

```
// kernel/fork.c

pid_t kernel_clone(struct kernel_clone_args *args)
{
u64 clone_flags = args->flags;
struct completion vfork;
struct pid *pid;
struct task_struct *p; //代表了线程or进程的内核结构，很重要
int trace = 0;
pid_t nr;

if ((args->flags & CLONE_PIDFD) &&  //做一些clone参数校验
    (args->flags & CLONE_PARENT_SETTID) &&
    (args->pidfd == args->parent_tid))
return -EINVAL;

if (!(clone_flags & CLONE_UNTRACED)) { //用于ptrace跟踪
if (clone_flags & CLONE_VFORK)
trace = PTRACE_EVENT_VFORK;
else if (args->exit_signal != SIGCHLD)
trace = PTRACE_EVENT_CLONE;
else
trace = PTRACE_EVENT_FORK;

if (likely(!ptrace_event_enabled(current, trace)))
trace = 0;
}

p = copy_process(NULL, trace, NUMA_NO_NODE, args); //最重要的拷贝函数
add_latent_entropy();

if (IS_ERR(p))
return PTR_ERR(p);

trace_sched_process_fork(current, p);

pid = get_task_pid(p, PIDTYPE_PID); //获取子进程or子线程的p->pid
nr = pid_vnr(pid); //获取当前namepsapce内部看到的pid

if (clone_flags & CLONE_PARENT_SETTID) //将当前namepsapce得到的pid设置到用户态的parent_id中
put_user(nr, args->parent_tid);

if (clone_flags & CLONE_VFORK) { //vfork系统调用初始化
p->vfork_done = &vfork;
init_completion(&vfork);
get_task_struct(p);
}

if (IS_ENABLED(CONFIG_LRU_GEN) && !(clone_flags & CLONE_VM)) { //为子线程or子进程的内存地址空间配备LRU淘汰策略
task_lock(p);
lru_gen_add_mm(p->mm);
task_unlock(p);
}

wake_up_new_task(p); //将子线程放入runqueue中

if (unlikely(trace)) //子线程or子进程clone结束，通知ptrace
ptrace_event_pid(trace, pid);

   //等待vfork结束，通知ptrace 
if (clone_flags & CLONE_VFORK) { 
if (!wait_for_vfork_done(p, &vfork))
ptrace_event_pid(PTRACE_EVENT_VFORK_DONE, pid);
}

put_pid(pid); //释放所占用的cache
return nr; //返回当前namespace下能看到的pid
}
```

这里有几个重点：

1.  在linux下进程和线程有很多的共同点，线程被称为轻量级进程（LWP），也都用task_struct这个结构体管理。他们的差异在于： 创建子进程的clone_flag为SIGCHLD，而线程有很多`（CLONE_VM、CLONE_FS、CLONE_FILES、CLONE_SIGNAL、CLONE_SETTLS、CLONE_PARENT_SETTID、CLONE_CHILD_CLEARTID、CLONE_SYSVSEM）`  
    子线程共享了父进程的地址空间、文件系统、文件描述符、信号量等，而子进程是独立的，可以参考之前的clone_flags的描述。
2.  `copy_process`很重要，一会我们仔细看。
3.  pid描述符是有namespace定义的，当前namespace下只能看到自己的pid描述符。这就实现了像docker容器内看到的进程是新的进程pid，同时在物理机上也能看到容器所在物理机的进程pid。

在看`copy_process`之前，我们先来`task_struct`都有什么，这个结构体非常长，有很多预处理的分支，这里只摘出重要的：

```
// include/linux/sched.h

struct task_struct {
   ...
pid_tpid; //如果是进程表示进程的pid，如果是线程表示线程的id
pid_ttgid; //如果是进程与pid相同，如果是线程表示属于哪个进程

   //父、子、兄弟进程
struct task_struct __rcu*real_parent;
   struct list_headchildren;
struct list_headsibling;

    //调度的优先级
   intprio; //动态优先级
intstatic_prio; //静态优先级，即修改的nice值
intnormal_prio; //通过静态优先级和调度策略算出来的值
unsigned intrt_priority; //实时优先级

    //地址空间
struct mm_struct*mm;
struct mm_struct*active_mm;

   struct fs_struct*fs; //文件系统
struct files_struct*files; //打开的文件描述符

struct nsproxy*nsproxy; //namespace

   //信号处理函数
struct signal_struct*signal;
struct sighand_struct __rcu*sighand;
sigset_tblocked;
sigset_treal_blocked;
   ...
};

```

我们依次展开`struct mm_struct`、`struct fs_struct`、`struct files_sturct`、`struct nsproxy`看看都有什么：

```
// include/linux/mm_types.h
struct mm_struct{
     ...
     unsigned long mmap_base; //mmap基地址
     
     unsigned long task_size;  //栈空间大小
  pgd_t * pgd; //页表页目录基地址
     
     unsigned long start_code, end_code, start_data, end_data; //代码段、数据段的开始和结束地址
  unsigned long start_brk, brk, start_stack; //堆栈段的开始和结束地址
  ...
};

// include/linux/fs_struct.h
struct fs_struct {
   ...
spinlock_t lock; //自旋锁
seqcount_spinlock_t seq; 
struct path root, pwd; //根目录，当前目录
...
} __randomize_layout;

// include/linux/path.h
struct path {
struct vfsmount *mnt; //挂载点
struct dentry *dentry; //目录inode节点
} __randomize_layout;

// include/linux/fdtable.h
struct files_struct {
   ...
atomic_t count; //引用计数

struct fdtable __rcu *fdt; 
unsigned int next_fd; //下一个文件
...
};

// include/linux/nsproxy.h
struct nsproxy {
   ...
atomic_t count; //引用计数
struct mnt_namespace *mnt_ns; //挂载点namespace
struct net      *net_ns; //网络namepsace
struct time_namespace *time_ns; //时间namespace
struct cgroup_namespace *cgroup_ns; //cgroup namespace
...
};
```

在对`struct task_struct`有了基本的认识后，我们再看看`copy_process`的实现：

```
// kernel/fork.c

static __latent_entropy struct task_struct *copy_process(
struct pid *pid,
int trace,
int node,
struct kernel_clone_args *args)
{
   int pidfd = -1, retval;
struct task_struct *p;
struct multiprocess_signals delayed;
struct file *pidfile = NULL;
const u64 clone_flags = args->flags;
struct nsproxy *nsp = current->nsproxy;
   ....
   p = dup_task_struct(current, node); //生成新的task_struct结构

p->set_child_tid = (clone_flags & CLONE_CHILD_SETTID) ? args->child_tid : NULL;
p->clear_child_tid = (clone_flags & CLONE_CHILD_CLEARTID) ? args->child_tid : NULL;

retval = sched_fork(clone_flags, p); //初始化调度信息

retval = copy_files(clone_flags, p); //拷贝文件描述符

retval = copy_fs(clone_flags, p); //拷贝文件系统

retval = copy_sighand(clone_flags, p); //拷贝信号处理函数

retval = copy_signal(clone_flags, p); //拷贝信号量

retval = copy_mm(clone_flags, p); //拷贝内存地址空间

retval = copy_namespaces(clone_flags, p); //拷贝namespace

retval = copy_io(clone_flags, p); //拷贝io

retval = copy_thread(p, args); //拷贝线程信息

    if (pid != &init_struct_pid) {
pid = alloc_pid(p->nsproxy->pid_ns_for_children, args->set_tid, //分配pid
args->set_tid_size);
if (IS_ERR(pid)) {
retval = PTR_ERR(pid);
goto bad_fork_cleanup_thread;
}
}

p->pid = pid_nr(pid); //设置pid、tgid
if (clone_flags & CLONE_THREAD) {
p->group_leader = current->group_leader;
p->tgid = current->tgid; //线程的tgid和pid不同
} else {
p->group_leader = p;
p->tgid = p->pid; //子进程的pid和tgid相同
}
  ...

}
```

这里有几个重点：

1.  `copy_process`里会通过`dup_task_struct`生成子线程or子进程子集的`task_struct`结构
2.  通过`sched_fork`初始化调度信息，主要包括：
    -   设置状态`__state`为`TASK_NEW`
    -   设置调度策略，初始化runqueue
3.  通过一系列的`copy_xxx`函数拷贝文件描述符、文件系统、地址空间等，其中`copy_thread`会将用户态传来的回调函数、参数放入寄存器中等待CPU调度：
    -   回调函数fn，放到bx寄存器
    -   回调函数参数arg，如果是32位放到di寄存器，如果是64位放到r12寄存器
4.  通过`alloc_pid`函数分配pid，并通过`pid_nr`设置pid到新的`task_struct`结构中。在Linux4.15的版本pid分配开始采用redix树分配pid，之前版本采用bitmap分配pid。

在完成`copy_process`后，子线程的数据结构`task_struct`就全部构造完成，接下来会通过`wake_up_new_task(p);`将子线程加入就绪队列中，等待调度器调度执行。

```
// kernel/sched/core.c

void wake_up_new_task(struct task_struct *p)
{
struct rq_flags rf;
struct rq *rq;

raw_spin_lock_irqsave(&p->pi_lock, rf.flags); 
WRITE_ONCE(p->__state, TASK_RUNNING); //修改线程状态__state为TASK_RUNNING
#ifdef CONFIG_SMP
p->recent_used_cpu = task_cpu(p); //多核CPU进行绑核
rseq_migrate(p);
__set_task_cpu(p, select_task_rq(p, task_cpu(p), WF_FORK));
#endif
rq = __task_rq_lock(p, &rf); //为runqueue加锁
update_rq_clock(rq);
post_init_entity_util_avg(p);

activate_task(rq, p, ENQUEUE_NOCLOCK); //将线程加入到runqeue中
trace_sched_wakeup_new(p);
check_preempt_curr(rq, p, WF_FORK);
#ifdef CONFIG_SMP
if (p->sched_class->task_woken) {
rq_unpin_lock(rq, &rf);
p->sched_class->task_woken(rq, p);
rq_repin_lock(rq, &rf);
}
#endif
task_rq_unlock(rq, p, &rf); //解锁
}
```

以上就是Linux内核态创建线程的整个过程。比较复杂，中间会涉及到很多知识，希望你能仔细阅读。

___

## 小结

1.  glibc的pthread函数库实现了Linux下用户态线程，通过一个有趣的宏将pthread_create导出为__pthread_create_x_y（其中x和y对应了glibc的版本
2.  pthread会分配线程栈空间，并设置了线程栈的保护区（当用户访问线程栈越界后，抛出异常），同时设置了各种线程属性和和clone参数，最后调用`__clone_internal`进入内核态
3.  `__clone_internal`本质上调用了系统调用`clone3`、`clone`，而这俩个系统调用是通过汇编指令正式进入内核态
4.  内核态下的线程都是通过`fork.c`中的实现的：
    -   `SYSCALL_DEFINE2(clone3,...)`
    -   `SYSCALL_DEFINE5(clone,...)`
5.  这俩个函数实现，会构造内核态的clone参数`kernel_clone_args`并调用`kernel_clone`进行线程创建，同时我们知道了Linux下线程和进程几乎相同，线程被称为轻量级进程（LWP），是因为复用了进程的实现代码和数据结构，差异存在于共享的地址空间、文件描述符等。
6.  `kernel_clone`做了俩个重要的事情：
    1.  调用`copy_process`根据线程或进程的特性初始化`task_struct`结构
    2.  调用`wake_up_new_task`将子线程放入runqueue等待调度器调度
7.  到此Java是如何创建线程的，我们看清楚了，接下来的最后一篇文章中，我会站在全局的角度来说说线程的创建，并观测一下线程消耗，从而解决第一篇中提出的问题
