---
source: https://www.cnblogs.com/yungyu16/p/13166189.html#%E6%96%87%E4%BB%B6-socket-%E9%80%9A%E4%BF%A1%E7%9A%84%E9%80%9A%E9%81%93%E7%9A%84%E5%88%9B%E5%BB%BA
---
> 本文转载自[JVM Attach实现原理剖析](https://www.cnblogs.com/scofield-1987/p/9347586.html)

## 前言

本文旨在从理论上分析JVM 在 Linux 环境下 Attach 操作的前因后果，以及 JVM 为此而设计并实现的解决方案，通过本文，我希望能够讲述清楚如下三个主要方面的内容。

## Attach 为什么而出现

**Attach的出现究其根本原因，应该就是为了实现 Java 进程（A）与进程（B）之间的本地通信**。一旦这个通信通道能够成功建立，那么进程 A 就能通知进程 B 去执行某些操作，从而达到监控进程 B 或者控制进程 B 的某些行为的目的。如 jstack、jmap等 JDK 自带的工具，基本都是通过 Attach 机制去达成各自想要的目的的。至于 jstack、jmap 能做什么、怎么做，就不再本文的讨论范围了，请自行百度或者 Google。

## Attach 在 JVM 底层实现的根本原理是什么

Attach 实现的根本原理就是使用了 Linux 下是文件 Socket 通信（详情可以自行百度或 Google）。有人也许会问，为什么要采用文件 socket 而不采用网络 socket？我个人认为也许一方面是为了效率（避免了网络协议的解析、数据包的封装和解封装等），另一方面是为了减少对系统资源的占用（如网络端口占用）。采用文件 socket 通信，就好比两个进程通过事先约定好的协议，对同一个文件进行读写操作，以达到信息的交互和共享。简单理解成如下图所示的模型

通过/tmp/.java.pid2345这个文件，实现客户进程与目标进程2345的通信。

## Attach 在 JVM 中实现的源码分析

源码的分析主要分三阶段进行，这里要达到的目的是，弄 Attach 的清楚来龙去脉，本文的所有源码都是基于 Open JDK 1.8的，大家可以自行去下载 Open JDK 1.8 的源码。

## 4.1. 目标JVM 对OS信号监听的实现

或许你会想，在最开始的时候，目标 JVM 是怎么知道有某个进程想 attach 它自己的？答案很简单，就是目标 JVM 在启动的时候，在 JVM 内部启动了一个监听线程，这个线程的名字叫“Signal Dispatcher”，该线程的作用是，监听并处理 OS 的信号。至于什么是 OS 的信号（可以自行百度或 Google），简单理解就是，Linux系统允许进程与进程之间通过过信号的方式进行通信，如触发某个操作（操作由接受到信号的进程自定义）。如平常我们用的最多的就是 kill -9 ${pid}来杀死某个进程，kill进程通过向${pid}的进程发送一个编号为“9”号的信号，来通知系统强制结束${pid}的生命周期。

接下来我们就通过源码截图的方式来呈现一下“Signal Dispatcher”线程的创建过程。

首先进入 JVM 的启动类：**/jdk/src/share/bin/main.c**

```
 1 int
 2 main(int argc, char **argv)
 3 {
 4     int margc;
 5     char** margv;
 6     const jboolean const_javaw = JNI_FALSE;
 7 #endif /* JAVAW */
 8 #ifdef _WIN32
 9     {
10         int i = 0;
11         if (getenv(JLDEBUG_ENV_ENTRY) != NULL) {
12             printf("Windows original main args:\n");
13             for (i = 0 ; i < __argc ; i++) {
14                 printf("wwwd_args[%d] = %s\n", i, __argv[i]);
15             }
16         }
17     }
18     JLI_CmdToArgs(GetCommandLine());
19     margc = JLI_GetStdArgc();
20     // add one more to mark the end
21     margv = (char **)JLI_MemAlloc((margc + 1) * (sizeof(char *)));
22     {
23         int i = 0;
24         StdArg *stdargs = JLI_GetStdArgs();
25         for (i = 0 ; i < margc ; i++) {
26             margv[i] = stdargs[i].arg;
27         }
28         margv[i] = NULL;
29     }
30 #else /* *NIXES */
31     margc = argc;
32     margv = argv;
33 #endif /* WIN32 */
34     return JLI_Launch(margc, margv,
35                    sizeof(const_jargs) / sizeof(char *), const_jargs,
36                    sizeof(const_appclasspath) / sizeof(char *), const_appclasspath,
37                    FULL_VERSION,
38                    DOT_VERSION,
39                    (const_progname != NULL) ? const_progname : *margv,
40                    (const_launcher != NULL) ? const_launcher : *margv,
41                    (const_jargs != NULL) ? JNI_TRUE : JNI_FALSE,
42                    const_cpwildcard, const_javaw, const_ergo_class);
43 }
```

这个类里边最重要的一个方法就是最后的JLI_Launch，这个方法的实现存在于**jdk/src/share/bin/java.c** 中（大家应该都不陌生平时我们运行 java 程序时，都是采用 java com.***.Main来启动的吧）。

```
  1 /*
  2  * Entry point.
  3  */
  4 int
  5 JLI_Launch(int argc, char ** argv,              /* main argc, argc */
  6         int jargc, const char** jargv,          /* java args */
  7         int appclassc, const char** appclassv,  /* app classpath */
  8         const char* fullversion,                /* full version defined */
  9         const char* dotversion,                 /* dot version defined */
 10         const char* pname,                      /* program name */
 11         const char* lname,                      /* launcher name */
 12         jboolean javaargs,                      /* JAVA_ARGS */
 13         jboolean cpwildcard,                    /* classpath wildcard*/
 14         jboolean javaw,                         /* windows-only javaw */
 15         jint ergo                               /* ergonomics class policy */
 16 )
 17 {
 18     int mode = LM_UNKNOWN;
 19     char *what = NULL;
 20     char *cpath = 0;
 21     char *main_class = NULL;
 22     int ret;
 23     InvocationFunctions ifn;
 24     jlong start, end;
 25     char jvmpath[MAXPATHLEN];
 26     char jrepath[MAXPATHLEN];
 27     char jvmcfg[MAXPATHLEN];
 28 
 29     _fVersion = fullversion;
 30     _dVersion = dotversion;
 31     _launcher_name = lname;
 32     _program_name = pname;
 33     _is_java_args = javaargs;
 34     _wc_enabled = cpwildcard;
 35     _ergo_policy = ergo;
 36 
 37     InitLauncher(javaw);
 38     DumpState();
 39     if (JLI_IsTraceLauncher()) {
 40         int i;
 41         printf("Command line args:\n");
 42         for (i = 0; i < argc ; i++) {
 43             printf("argv[%d] = %s\n", i, argv[i]);
 44         }
 45         AddOption("-Dsun.java.launcher.diag=true", NULL);
 46     }
 47 
 48     /*
 49      * Make sure the specified version of the JRE is running.
 50      *
 51      * There are three things to note about the SelectVersion() routine:
 52      *  1) If the version running isn't correct, this routine doesn't
 53      *     return (either the correct version has been exec'd or an error
 54      *     was issued).
 55      *  2) Argc and Argv in this scope are *not* altered by this routine.
 56      *     It is the responsibility of subsequent code to ignore the
 57      *     arguments handled by this routine.
 58      *  3) As a side-effect, the variable "main_class" is guaranteed to
 59      *     be set (if it should ever be set).  This isn't exactly the
 60      *     poster child for structured programming, but it is a small
 61      *     price to pay for not processing a jar file operand twice.
 62      *     (Note: This side effect has been disabled.  See comment on
 63      *     bugid 5030265 below.)
 64      */
 65     SelectVersion(argc, argv, &main_class);
 66 
 67     CreateExecutionEnvironment(&argc, &argv,
 68                                jrepath, sizeof(jrepath),
 69                                jvmpath, sizeof(jvmpath),
 70                                jvmcfg,  sizeof(jvmcfg));
 71 
 72     ifn.CreateJavaVM = 0;
 73     ifn.GetDefaultJavaVMInitArgs = 0;
 74 
 75     if (JLI_IsTraceLauncher()) {
 76         start = CounterGet();
 77     }
 78 
 79     if (!LoadJavaVM(jvmpath, &ifn)) {
 80         return(6);
 81     }
 82 
 83     if (JLI_IsTraceLauncher()) {
 84         end   = CounterGet();
 85     }
 86 
 87     JLI_TraceLauncher("%ld micro seconds to LoadJavaVM\n",
 88              (long)(jint)Counter2Micros(end-start));
 89 
 90     ++argv;
 91     --argc;
 92 
 93     if (IsJavaArgs()) {
 94         /* Preprocess wrapper arguments */
 95         TranslateApplicationArgs(jargc, jargv, &argc, &argv);
 96         if (!AddApplicationOptions(appclassc, appclassv)) {
 97             return(1);
 98         }
 99     } else {
100         /* Set default CLASSPATH */
101         cpath = getenv("CLASSPATH");
102         if (cpath == NULL) {
103             cpath = ".";
104         }
105         SetClassPath(cpath);
106     }
107 
108     /* Parse command line options; if the return value of
109      * ParseArguments is false, the program should exit.
110      */
111     if (!ParseArguments(&argc, &argv, &mode, &what, &ret, jrepath))
112     {
113         return(ret);
114     }
115 
116     /* Override class path if -jar flag was specified */
117     if (mode == LM_JAR) {
118         SetClassPath(what);     /* Override class path */
119     }
120 
121     /* set the -Dsun.java.command pseudo property */
122     SetJavaCommandLineProp(what, argc, argv);
123 
124     /* Set the -Dsun.java.launcher pseudo property */
125     SetJavaLauncherProp();
126 
127     /* set the -Dsun.java.launcher.* platform properties */
128     SetJavaLauncherPlatformProps();
129 
130     return JVMInit(&ifn, threadStackSize, argc, argv, mode, what, ret);
131 }
```

这个方法中，进行了一系列必要的操作，如libjvm.so的加载、参数解析、Classpath 的获取和设置、系统属性的设置、JVM 初始化等等，不过和本文相关的主要是130行的 JVMInit 方法，接下来我们看下这个方法的实现（位于**/jdk/src/solaris/bin/java_md_solinux.c**）。

```
1 int
2 JVMInit(InvocationFunctions* ifn, jlong threadStackSize,
3         int argc, char **argv,
4         int mode, char *what, int ret)
5 {
6     ShowSplashScreen();
7     return ContinueInNewThread(ifn, threadStackSize, argc, argv, mode, what, ret);
8 }
```

这里请关注两个点，**ContinueInNewThread方法** 和 **ifn 入参**。ContinueInNewThread位于 java.c中，而 ifn 则携带了libjvm.so中的几个非常重要的函数(CreateJavaVM/GetDefaultJavaVMInitArgs/GetCreatedJavaVMs)，这里我们重点关注CreateJavaVM

```
 1 int
 2 ContinueInNewThread(InvocationFunctions* ifn, jlong threadStackSize,
 3                     int argc, char **argv,
 4                     int mode, char *what, int ret)
 5 {
 6 
 7     /*
 8      * If user doesn't specify stack size, check if VM has a preference.
 9      * Note that HotSpot no longer supports JNI_VERSION_1_1 but it will
10      * return its default stack size through the init args structure.
11      */
12     if (threadStackSize == 0) {
13       struct JDK1_1InitArgs args1_1;
14       memset((void*)&args1_1, 0, sizeof(args1_1));
15       args1_1.version = JNI_VERSION_1_1;
16       ifn->GetDefaultJavaVMInitArgs(&args1_1);  /* ignore return value */
17       if (args1_1.javaStackSize > 0) {
18          threadStackSize = args1_1.javaStackSize;
19       }
20     }
21 
22     { /* Create a new thread to create JVM and invoke main method */
23       JavaMainArgs args;
24       int rslt;
25 
26       args.argc = argc;
27       args.argv = argv;
28       args.mode = mode;
29       args.what = what;
30       args.ifn = *ifn;
31 
32       rslt = ContinueInNewThread0(JavaMain, threadStackSize, (void*)&args);
33       /* If the caller has deemed there is an error we
34        * simply return that, otherwise we return the value of
35        * the callee
36        */
37       return (ret != 0) ? ret : rslt;
38     }
39 }
```

可以看出，这里进行了 JavaMainArgs 参数设置，设置完成之后，在32行处调用了 **ContinueInNewThread0 （位于/jdk/src/solaris/bin/java_md_solinux.c）**方法，该方法中传入了 **JavaMain** 函数指针和 **args** 参数，这二者至关重要。接下来看下其源码

```
 1 /*
 2  * Block current thread and continue execution in a new thread
 3  */
 4 int
 5 ContinueInNewThread0(int (JNICALL *continuation)(void *), jlong stack_size, void * args) {
 6     int rslt;
 7 #ifdef __linux__
 8     pthread_t tid;
 9     pthread_attr_t attr;
10     pthread_attr_init(&attr);
11     pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE);
12 
13     if (stack_size > 0) {
14       pthread_attr_setstacksize(&attr, stack_size);
15     }
16 
17     if (pthread_create(&tid, &attr, (void *(*)(void*))continuation, (void*)args) == 0) {
18       void * tmp;
19       pthread_join(tid, &tmp);
20       rslt = (int)tmp;
21     } else {
22      /*
23       * Continue execution in current thread if for some reason (e.g. out of
24       * memory/LWP)  a new thread can't be created. This will likely fail
25       * later in continuation as JNI_CreateJavaVM needs to create quite a
26       * few new threads, anyway, just give it a try..
27       */
28       rslt = continuation(args);
29     }
30 
31     pthread_attr_destroy(&attr);
32 #else /* ! __linux__ */
33     thread_t tid;
34     long flags = 0;
35     if (thr_create(NULL, stack_size, (void *(*)(void *))continuation, args, flags, &tid) == 0) {
36       void * tmp;
37       thr_join(tid, NULL, &tmp);
38       rslt = (int)tmp;
39     } else {
40       /* See above. Continue in current thread if thr_create() failed */
41       rslt = continuation(args);
42     }
43 #endif /* __linux__ */
44     return rslt;
45 }
```

这里最关键的点在于，如果是 linux 环境下，则创建了一个 pthread_t 的线程来运行传入的 JavaMain 函数，并且将 args 参数也一并传入了。这时候，我们唯一要关注的便是 JavaMain （**在jdk/src/share/bin/java.c** ）函数，请看源码

```
  1 int JNICALL
  2 JavaMain(void * _args)
  3 {
  4     JavaMainArgs *args = (JavaMainArgs *)_args;
  5     int argc = args->argc;
  6     char **argv = args->argv;
  7     int mode = args->mode;
  8     char *what = args->what;
  9     InvocationFunctions ifn = args->ifn;
 10 
 11     JavaVM *vm = 0;
 12     JNIEnv *env = 0;
 13     jclass mainClass = NULL;
 14     jclass appClass = NULL; // actual application class being launched
 15     jmethodID mainID;
 16     jobjectArray mainArgs;
 17     int ret = 0;
 18     jlong start, end;
 19 
 20     RegisterThread();
 21 
 22     /* Initialize the virtual machine */
 23     start = CounterGet();
 24     if (!InitializeJVM(&vm, &env, &ifn)) {
 25         JLI_ReportErrorMessage(JVM_ERROR1);
 26         exit(1);
 27     }
 28 
 29     if (showSettings != NULL) {
 30         ShowSettings(env, showSettings);
 31         CHECK_EXCEPTION_LEAVE(1);
 32     }
 33 
 34     if (printVersion || showVersion) {
 35         PrintJavaVersion(env, showVersion);
 36         CHECK_EXCEPTION_LEAVE(0);
 37         if (printVersion) {
 38             LEAVE();
 39         }
 40     }
 41 
 42     /* If the user specified neither a class name nor a JAR file */
 43     if (printXUsage || printUsage || what == 0 || mode == LM_UNKNOWN) {
 44         PrintUsage(env, printXUsage);
 45         CHECK_EXCEPTION_LEAVE(1);
 46         LEAVE();
 47     }
 48 
 49     FreeKnownVMs();  /* after last possible PrintUsage() */
 50 
 51     if (JLI_IsTraceLauncher()) {
 52         end = CounterGet();
 53         JLI_TraceLauncher("%ld micro seconds to InitializeJVM\n",
 54                (long)(jint)Counter2Micros(end-start));
 55     }
 56 
 57     /* At this stage, argc/argv have the application's arguments */
 58     if (JLI_IsTraceLauncher()){
 59         int i;
 60         printf("%s is '%s'\n", launchModeNames[mode], what);
 61         printf("App's argc is %d\n", argc);
 62         for (i=0; i < argc; i++) {
 63             printf("    argv[%2d] = '%s'\n", i, argv[i]);
 64         }
 65     }
 66 
 67     ret = 1;
 68 
 69     /*
 70      * Get the application's main class.
 71      *
 72      * See bugid 5030265.  The Main-Class name has already been parsed
 73      * from the manifest, but not parsed properly for UTF-8 support.
 74      * Hence the code here ignores the value previously extracted and
 75      * uses the pre-existing code to reextract the value.  This is
 76      * possibly an end of release cycle expedient.  However, it has
 77      * also been discovered that passing some character sets through
 78      * the environment has "strange" behavior on some variants of
 79      * Windows.  Hence, maybe the manifest parsing code local to the
 80      * launcher should never be enhanced.
 81      *
 82      * Hence, future work should either:
 83      *     1)   Correct the local parsing code and verify that the
 84      *          Main-Class attribute gets properly passed through
 85      *          all environments,
 86      *     2)   Remove the vestages of maintaining main_class through
 87      *          the environment (and remove these comments).
 88      *
 89      * This method also correctly handles launching existing JavaFX
 90      * applications that may or may not have a Main-Class manifest entry.
 91      */
 92     mainClass = LoadMainClass(env, mode, what);
 93     CHECK_EXCEPTION_NULL_LEAVE(mainClass);
 94     /*
 95      * In some cases when launching an application that needs a helper, e.g., a
 96      * JavaFX application with no main method, the mainClass will not be the
 97      * applications own main class but rather a helper class. To keep things
 98      * consistent in the UI we need to track and report the application main class.
 99      */
100     appClass = GetApplicationClass(env);
101     NULL_CHECK_RETURN_VALUE(appClass, -1);
102     /*
103      * PostJVMInit uses the class name as the application name for GUI purposes,
104      * for example, on OSX this sets the application name in the menu bar for
105      * both SWT and JavaFX. So we'll pass the actual application class here
106      * instead of mainClass as that may be a launcher or helper class instead
107      * of the application class.
108      */
109     PostJVMInit(env, appClass, vm);
110     /*
111      * The LoadMainClass not only loads the main class, it will also ensure
112      * that the main method's signature is correct, therefore further checking
113      * is not required. The main method is invoked here so that extraneous java
114      * stacks are not in the application stack trace.
115      */
116     mainID = (*env)->GetStaticMethodID(env, mainClass, "main",
117                                        "([Ljava/lang/String;)V");
118     CHECK_EXCEPTION_NULL_LEAVE(mainID);
119 
120     /* Build platform specific argument array */
121     mainArgs = CreateApplicationArgs(env, argv, argc);
122     CHECK_EXCEPTION_NULL_LEAVE(mainArgs);
123 
124     /* Invoke main method. */
125     (*env)->CallStaticVoidMethod(env, mainClass, mainID, mainArgs);
126 
127     /*
128      * The launcher's exit code (in the absence of calls to
129      * System.exit) will be non-zero if main threw an exception.
130      */
131     ret = (*env)->ExceptionOccurred(env) == NULL ? 0 : 1;
132     LEAVE();
133 }
```

和本小节相关的函数为**InitializeJVM**函数，在这个函数中，调用CreateJavaVM方法，这个方法就是之前在加载 libjvm.so 的时候，从动态库中获取的，首先看InitializeJVM的源码

```
 1 /*
 2  * Initializes the Java Virtual Machine. Also frees options array when
 3  * finished.
 4  */
 5 static jboolean
 6 InitializeJVM(JavaVM **pvm, JNIEnv **penv, InvocationFunctions *ifn)
 7 {
 8     JavaVMInitArgs args;
 9     jint r;
10 
11     memset(&args, 0, sizeof(args));
12     args.version  = JNI_VERSION_1_2;
13     args.nOptions = numOptions;
14     args.options  = options;
15     args.ignoreUnrecognized = JNI_FALSE;
16 
17     if (JLI_IsTraceLauncher()) {
18         int i = 0;
19         printf("JavaVM args:\n    ");
20         printf("version 0x%08lx, ", (long)args.version);
21         printf("ignoreUnrecognized is %s, ",
22                args.ignoreUnrecognized ? "JNI_TRUE" : "JNI_FALSE");
23         printf("nOptions is %ld\n", (long)args.nOptions);
24         for (i = 0; i < numOptions; i++)
25             printf("    option[%2d] = '%s'\n",
26                    i, args.options[i].optionString);
27     }
28 
29     r = ifn->CreateJavaVM(pvm, (void **)penv, &args);
30     JLI_MemFree(options);
31     return r == JNI_OK;
32 }
```

29行处，调用 CreateJavaVM（定义在**hotspot/src/share/vm/prims/jni.cpp**） 方法，来进行 JVM 虚拟机的真正创建过程，源码如下

```
  1 _JNI_IMPORT_OR_EXPORT_ jint JNICALL JNI_CreateJavaVM(JavaVM **vm, void **penv, void *args) {
  2 #ifndef USDT2
  3   HS_DTRACE_PROBE3(hotspot_jni, CreateJavaVM__entry, vm, penv, args);
  4 #else /* USDT2 */
  5   HOTSPOT_JNI_CREATEJAVAVM_ENTRY(
  6                                  (void **) vm, penv, args);
  7 #endif /* USDT2 */
  8 
  9   jint result = JNI_ERR;
 10   DT_RETURN_MARK(CreateJavaVM, jint, (const jint&)result);
 11 
 12   // We're about to use Atomic::xchg for synchronization.  Some Zero
 13   // platforms use the GCC builtin __sync_lock_test_and_set for this,
 14   // but __sync_lock_test_and_set is not guaranteed to do what we want
 15   // on all architectures.  So we check it works before relying on it.
 16 #if defined(ZERO) && defined(ASSERT)
 17   {
 18     jint a = 0xcafebabe;
 19     jint b = Atomic::xchg(0xdeadbeef, &a);
 20     void *c = &a;
 21     void *d = Atomic::xchg_ptr(&b, &c);
 22     assert(a == (jint) 0xdeadbeef && b == (jint) 0xcafebabe, "Atomic::xchg() works");
 23     assert(c == &b && d == &a, "Atomic::xchg_ptr() works");
 24   }
 25 #endif // ZERO && ASSERT
 26 
 27   // At the moment it's only possible to have one Java VM,
 28   // since some of the runtime state is in global variables.
 29 
 30   // We cannot use our mutex locks here, since they only work on
 31   // Threads. We do an atomic compare and exchange to ensure only
 32   // one thread can call this method at a time
 33 
 34   // We use Atomic::xchg rather than Atomic::add/dec since on some platforms
 35   // the add/dec implementations are dependent on whether we are running
 36   // on a multiprocessor, and at this stage of initialization the os::is_MP
 37   // function used to determine this will always return false. Atomic::xchg
 38   // does not have this problem.
 39   if (Atomic::xchg(1, &vm_created) == 1) {
 40     return JNI_EEXIST;   // already created, or create attempt in progress
 41   }
 42   if (Atomic::xchg(0, &safe_to_recreate_vm) == 0) {
 43     return JNI_ERR;  // someone tried and failed and retry not allowed.
 44   }
 45 
 46   assert(vm_created == 1, "vm_created is true during the creation");
 47 
 48   /**
 49    * Certain errors during initialization are recoverable and do not
 50    * prevent this method from being called again at a later time
 51    * (perhaps with different arguments).  However, at a certain
 52    * point during initialization if an error occurs we cannot allow
 53    * this function to be called again (or it will crash).  In those
 54    * situations, the 'canTryAgain' flag is set to false, which atomically
 55    * sets safe_to_recreate_vm to 1, such that any new call to
 56    * JNI_CreateJavaVM will immediately fail using the above logic.
 57    */
 58   bool can_try_again = true;
 59 
 60   result = Threads::create_vm((JavaVMInitArgs*) args, &can_try_again);
 61   if (result == JNI_OK) {
 62     JavaThread *thread = JavaThread::current();
 63     /* thread is thread_in_vm here */
 64     *vm = (JavaVM *)(&main_vm);
 65     *(JNIEnv**)penv = thread->jni_environment();
 66 
 67     // Tracks the time application was running before GC
 68     RuntimeService::record_application_start();
 69 
 70     // Notify JVMTI
 71     if (JvmtiExport::should_post_thread_life()) {
 72        JvmtiExport::post_thread_start(thread);
 73     }
 74 
 75     EventThreadStart event;
 76     if (event.should_commit()) {
 77       event.set_javalangthread(java_lang_Thread::thread_id(thread->threadObj()));
 78       event.commit();
 79     }
 80 
 81 #ifndef PRODUCT
 82   #ifndef TARGET_OS_FAMILY_windows
 83     #define CALL_TEST_FUNC_WITH_WRAPPER_IF_NEEDED(f) f()
 84   #endif
 85 
 86     // Check if we should compile all classes on bootclasspath
 87     if (CompileTheWorld) ClassLoader::compile_the_world();
 88     if (ReplayCompiles) ciReplay::replay(thread);
 89 
 90     // Some platforms (like Win*) need a wrapper around these test
 91     // functions in order to properly handle error conditions.
 92     CALL_TEST_FUNC_WITH_WRAPPER_IF_NEEDED(test_error_handler);
 93     CALL_TEST_FUNC_WITH_WRAPPER_IF_NEEDED(execute_internal_vm_tests);
 94 #endif
 95 
 96     // Since this is not a JVM_ENTRY we have to set the thread state manually before leaving.
 97     ThreadStateTransition::transition_and_fence(thread, _thread_in_vm, _thread_in_native);
 98   } else {
 99     if (can_try_again) {
100       // reset safe_to_recreate_vm to 1 so that retrial would be possible
101       safe_to_recreate_vm = 1;
102     }
103 
104     // Creation failed. We must reset vm_created
105     *vm = 0;
106     *(JNIEnv**)penv = 0;
107     // reset vm_created last to avoid race condition. Use OrderAccess to
108     // control both compiler and architectural-based reordering.
109     OrderAccess::release_store(&vm_created, 0);
110   }
111 
112   return result;
113 }
```

这里只关注最核心的方法是60行的**Threads::create_vm（hotspot/src/share/vm/runtime/Thread.cpp）** 方法，在这个方法中，进行了大量的初始化操作，不过，这里我们只关注其中的一个点，就是 os::signal_init() 方法的调用，这就是启动“Signal Dispatcher”线程的地方。先看 create_vm 的源码

```
  1 jint Threads::create_vm(JavaVMInitArgs* args, bool* canTryAgain) {
  2 
  3   extern void JDK_Version_init();
  4 
  5   // Check version
  6   if (!is_supported_jni_version(args->version)) return JNI_EVERSION;
  7 
  8   // Initialize the output stream module
  9   ostream_init();
 10 
 11   // Process java launcher properties.
 12   Arguments::process_sun_java_launcher_properties(args);
 13 
 14   // Initialize the os module before using TLS
 15   os::init();
 16 
 17   // Initialize system properties.
 18   Arguments::init_system_properties();
 19 
 20   // So that JDK version can be used as a discrimintor when parsing arguments
 21   JDK_Version_init();
 22 
 23   // Update/Initialize System properties after JDK version number is known
 24   Arguments::init_version_specific_system_properties();
 25 
 26   // Parse arguments
 27   jint parse_result = Arguments::parse(args);
 28   if (parse_result != JNI_OK) return parse_result;
 29 
 30   os::init_before_ergo();
 31 
 32   jint ergo_result = Arguments::apply_ergo();
 33   if (ergo_result != JNI_OK) return ergo_result;
 34 
 35   if (PauseAtStartup) {
 36     os::pause();
 37   }
 38 
 39 #ifndef USDT2
 40   HS_DTRACE_PROBE(hotspot, vm__init__begin);
 41 #else /* USDT2 */
 42   HOTSPOT_VM_INIT_BEGIN();
 43 #endif /* USDT2 */
 44 
 45   // Record VM creation timing statistics
 46   TraceVmCreationTime create_vm_timer;
 47   create_vm_timer.start();
 48 
 49   // Timing (must come after argument parsing)
 50   TraceTime timer("Create VM", TraceStartupTime);
 51 
 52   // Initialize the os module after parsing the args
 53   jint os_init_2_result = os::init_2();
 54   if (os_init_2_result != JNI_OK) return os_init_2_result;
 55 
 56   jint adjust_after_os_result = Arguments::adjust_after_os();
 57   if (adjust_after_os_result != JNI_OK) return adjust_after_os_result;
 58 
 59   // intialize TLS
 60   ThreadLocalStorage::init();
 61 
 62   // Bootstrap native memory tracking, so it can start recording memory
 63   // activities before worker thread is started. This is the first phase
 64   // of bootstrapping, VM is currently running in single-thread mode.
 65   MemTracker::bootstrap_single_thread();
 66 
 67   // Initialize output stream logging
 68   ostream_init_log();
 69 
 70   // Convert -Xrun to -agentlib: if there is no JVM_OnLoad
 71   // Must be before create_vm_init_agents()
 72   if (Arguments::init_libraries_at_startup()) {
 73     convert_vm_init_libraries_to_agents();
 74   }
 75 
 76   // Launch -agentlib/-agentpath and converted -Xrun agents
 77   if (Arguments::init_agents_at_startup()) {
 78     create_vm_init_agents();
 79   }
 80 
 81   // Initialize Threads state
 82   _thread_list = NULL;
 83   _number_of_threads = 0;
 84   _number_of_non_daemon_threads = 0;
 85 
 86   // Initialize global data structures and create system classes in heap
 87   vm_init_globals();
 88 
 89   // Attach the main thread to this os thread
 90   JavaThread* main_thread = new JavaThread();
 91   main_thread->set_thread_state(_thread_in_vm);
 92   // must do this before set_active_handles and initialize_thread_local_storage
 93   // Note: on solaris initialize_thread_local_storage() will (indirectly)
 94   // change the stack size recorded here to one based on the java thread
 95   // stacksize. This adjusted size is what is used to figure the placement
 96   // of the guard pages.
 97   main_thread->record_stack_base_and_size();
 98   main_thread->initialize_thread_local_storage();
 99 
100   main_thread->set_active_handles(JNIHandleBlock::allocate_block());
101 
102   if (!main_thread->set_as_starting_thread()) {
103     vm_shutdown_during_initialization(
104       "Failed necessary internal allocation. Out of swap space");
105     delete main_thread;
106     *canTryAgain = false; // don't let caller call JNI_CreateJavaVM again
107     return JNI_ENOMEM;
108   }
109 
110   // Enable guard page *after* os::create_main_thread(), otherwise it would
111   // crash Linux VM, see notes in os_linux.cpp.
112   main_thread->create_stack_guard_pages();
113 
114   // Initialize Java-Level synchronization subsystem
115   ObjectMonitor::Initialize() ;
116 
117   // Second phase of bootstrapping, VM is about entering multi-thread mode
118   MemTracker::bootstrap_multi_thread();
119 
120   // Initialize global modules
121   jint status = init_globals();
122   if (status != JNI_OK) {
123     delete main_thread;
124     *canTryAgain = false; // don't let caller call JNI_CreateJavaVM again
125     return status;
126   }
127 
128   // Should be done after the heap is fully created
129   main_thread->cache_global_variables();
130 
131   HandleMark hm;
132 
133   { MutexLocker mu(Threads_lock);
134     Threads::add(main_thread);
135   }
136 
137   // Any JVMTI raw monitors entered in onload will transition into
138   // real raw monitor. VM is setup enough here for raw monitor enter.
139   JvmtiExport::transition_pending_onload_raw_monitors();
140 
141   // Fully start NMT
142   MemTracker::start();
143 
144   // Create the VMThread
145   { TraceTime timer("Start VMThread", TraceStartupTime);
146     VMThread::create();
147     Thread* vmthread = VMThread::vm_thread();
148 
149     if (!os::create_thread(vmthread, os::vm_thread))
150       vm_exit_during_initialization("Cannot create VM thread. Out of system resources.");
151 
152     // Wait for the VM thread to become ready, and VMThread::run to initialize
153     // Monitors can have spurious returns, must always check another state flag
154     {
155       MutexLocker ml(Notify_lock);
156       os::start_thread(vmthread);
157       while (vmthread->active_handles() == NULL) {
158         Notify_lock->wait();
159       }
160     }
161   }
162 
163   assert (Universe::is_fully_initialized(), "not initialized");
164   if (VerifyDuringStartup) {
165     // Make sure we're starting with a clean slate.
166     VM_Verify verify_op;
167     VMThread::execute(&verify_op);
168   }
169 
170   EXCEPTION_MARK;
171 
172   // At this point, the Universe is initialized, but we have not executed
173   // any byte code.  Now is a good time (the only time) to dump out the
174   // internal state of the JVM for sharing.
175   if (DumpSharedSpaces) {
176     MetaspaceShared::preload_and_dump(CHECK_0);
177     ShouldNotReachHere();
178   }
179 
180   // Always call even when there are not JVMTI environments yet, since environments
181   // may be attached late and JVMTI must track phases of VM execution
182   JvmtiExport::enter_start_phase();
183 
184   // Notify JVMTI agents that VM has started (JNI is up) - nop if no agents.
185   JvmtiExport::post_vm_start();
186 
187   {
188     TraceTime timer("Initialize java.lang classes", TraceStartupTime);
189 
190     if (EagerXrunInit && Arguments::init_libraries_at_startup()) {
191       create_vm_init_libraries();
192     }
193 
194     initialize_class(vmSymbols::java_lang_String(), CHECK_0);
195 
196     // Initialize java_lang.System (needed before creating the thread)
197     initialize_class(vmSymbols::java_lang_System(), CHECK_0);
198     initialize_class(vmSymbols::java_lang_ThreadGroup(), CHECK_0);
199     Handle thread_group = create_initial_thread_group(CHECK_0);
200     Universe::set_main_thread_group(thread_group());
201     initialize_class(vmSymbols::java_lang_Thread(), CHECK_0);
202     oop thread_object = create_initial_thread(thread_group, main_thread, CHECK_0);
203     main_thread->set_threadObj(thread_object);
204     // Set thread status to running since main thread has
205     // been started and running.
206     java_lang_Thread::set_thread_status(thread_object,
207                                         java_lang_Thread::RUNNABLE);
208 
209     // The VM creates & returns objects of this class. Make sure it's initialized.
210     initialize_class(vmSymbols::java_lang_Class(), CHECK_0);
211 
212     // The VM preresolves methods to these classes. Make sure that they get initialized
213     initialize_class(vmSymbols::java_lang_reflect_Method(), CHECK_0);
214     initialize_class(vmSymbols::java_lang_ref_Finalizer(),  CHECK_0);
215     call_initializeSystemClass(CHECK_0);
216 
217     // get the Java runtime name after java.lang.System is initialized
218     JDK_Version::set_runtime_name(get_java_runtime_name(THREAD));
219     JDK_Version::set_runtime_version(get_java_runtime_version(THREAD));
220 
221     // an instance of OutOfMemory exception has been allocated earlier
222     initialize_class(vmSymbols::java_lang_OutOfMemoryError(), CHECK_0);
223     initialize_class(vmSymbols::java_lang_NullPointerException(), CHECK_0);
224     initialize_class(vmSymbols::java_lang_ClassCastException(), CHECK_0);
225     initialize_class(vmSymbols::java_lang_ArrayStoreException(), CHECK_0);
226     initialize_class(vmSymbols::java_lang_ArithmeticException(), CHECK_0);
227     initialize_class(vmSymbols::java_lang_StackOverflowError(), CHECK_0);
228     initialize_class(vmSymbols::java_lang_IllegalMonitorStateException(), CHECK_0);
229     initialize_class(vmSymbols::java_lang_IllegalArgumentException(), CHECK_0);
230   }
231 
232   // See        : bugid 4211085.
233   // Background : the static initializer of java.lang.Compiler tries to read
234   //              property"java.compiler" and read & write property "java.vm.info".
235   //              When a security manager is installed through the command line
236   //              option "-Djava.security.manager", the above properties are not
237   //              readable and the static initializer for java.lang.Compiler fails
238   //              resulting in a NoClassDefFoundError.  This can happen in any
239   //              user code which calls methods in java.lang.Compiler.
240   // Hack :       the hack is to pre-load and initialize this class, so that only
241   //              system domains are on the stack when the properties are read.
242   //              Currently even the AWT code has calls to methods in java.lang.Compiler.
243   //              On the classic VM, java.lang.Compiler is loaded very early to load the JIT.
244   // Future Fix : the best fix is to grant everyone permissions to read "java.compiler" and
245   //              read and write"java.vm.info" in the default policy file. See bugid 4211383
246   //              Once that is done, we should remove this hack.
247   initialize_class(vmSymbols::java_lang_Compiler(), CHECK_0);
248 
249   // More hackery - the static initializer of java.lang.Compiler adds the string "nojit" to
250   // the java.vm.info property if no jit gets loaded through java.lang.Compiler (the hotspot
251   // compiler does not get loaded through java.lang.Compiler).  "java -version" with the
252   // hotspot vm says "nojit" all the time which is confusing.  So, we reset it here.
253   // This should also be taken out as soon as 4211383 gets fixed.
254   reset_vm_info_property(CHECK_0);
255 
256   quicken_jni_functions();
257 
258   // Must be run after init_ft which initializes ft_enabled
259   if (TRACE_INITIALIZE() != JNI_OK) {
260     vm_exit_during_initialization("Failed to initialize tracing backend");
261   }
262 
263   // Set flag that basic initialization has completed. Used by exceptions and various
264   // debug stuff, that does not work until all basic classes have been initialized.
265   set_init_completed();
266 
267 #ifndef USDT2
268   HS_DTRACE_PROBE(hotspot, vm__init__end);
269 #else /* USDT2 */
270   HOTSPOT_VM_INIT_END();
271 #endif /* USDT2 */
272 
273   // record VM initialization completion time
274 #if INCLUDE_MANAGEMENT
275   Management::record_vm_init_completed();
276 #endif // INCLUDE_MANAGEMENT
277 
278   // Compute system loader. Note that this has to occur after set_init_completed, since
279   // valid exceptions may be thrown in the process.
280   // Note that we do not use CHECK_0 here since we are inside an EXCEPTION_MARK and
281   // set_init_completed has just been called, causing exceptions not to be shortcut
282   // anymore. We call vm_exit_during_initialization directly instead.
283   SystemDictionary::compute_java_system_loader(THREAD);
284   if (HAS_PENDING_EXCEPTION) {
285     vm_exit_during_initialization(Handle(THREAD, PENDING_EXCEPTION));
286   }
287 
288 #if INCLUDE_ALL_GCS
289   // Support for ConcurrentMarkSweep. This should be cleaned up
290   // and better encapsulated. The ugly nested if test would go away
291   // once things are properly refactored. XXX YSR
292   if (UseConcMarkSweepGC || UseG1GC) {
293     if (UseConcMarkSweepGC) {
294       ConcurrentMarkSweepThread::makeSurrogateLockerThread(THREAD);
295     } else {
296       ConcurrentMarkThread::makeSurrogateLockerThread(THREAD);
297     }
298     if (HAS_PENDING_EXCEPTION) {
299       vm_exit_during_initialization(Handle(THREAD, PENDING_EXCEPTION));
300     }
301   }
302 #endif // INCLUDE_ALL_GCS
303 
304   // Always call even when there are not JVMTI environments yet, since environments
305   // may be attached late and JVMTI must track phases of VM execution
306   JvmtiExport::enter_live_phase();
307 
308   // Signal Dispatcher needs to be started before VMInit event is posted
309   os::signal_init();
310 
311   // Start Attach Listener if +StartAttachListener or it can't be started lazily
312   if (!DisableAttachMechanism) {
313     AttachListener::vm_start();
314     if (StartAttachListener || AttachListener::init_at_startup()) {
315       AttachListener::init();
316     }
317   }
318 
319   // Launch -Xrun agents
320   // Must be done in the JVMTI live phase so that for backward compatibility the JDWP
321   // back-end can launch with -Xdebug -Xrunjdwp.
322   if (!EagerXrunInit && Arguments::init_libraries_at_startup()) {
323     create_vm_init_libraries();
324   }
325 
326   // Notify JVMTI agents that VM initialization is complete - nop if no agents.
327   JvmtiExport::post_vm_initialized();
328 
329   if (TRACE_START() != JNI_OK) {
330     vm_exit_during_initialization("Failed to start tracing backend.");
331   }
332 
333   if (CleanChunkPoolAsync) {
334     Chunk::start_chunk_pool_cleaner_task();
335   }
336 
337   // initialize compiler(s)
338 #if defined(COMPILER1) || defined(COMPILER2) || defined(SHARK)
339   CompileBroker::compilation_init();
340 #endif
341 
342   if (EnableInvokeDynamic) {
343     // Pre-initialize some JSR292 core classes to avoid deadlock during class loading.
344     // It is done after compilers are initialized, because otherwise compilations of
345     // signature polymorphic MH intrinsics can be missed
346     // (see SystemDictionary::find_method_handle_intrinsic).
347     initialize_class(vmSymbols::java_lang_invoke_MethodHandle(), CHECK_0);
348     initialize_class(vmSymbols::java_lang_invoke_MemberName(), CHECK_0);
349     initialize_class(vmSymbols::java_lang_invoke_MethodHandleNatives(), CHECK_0);
350   }
351 
352 #if INCLUDE_MANAGEMENT
353   Management::initialize(THREAD);
354 #endif // INCLUDE_MANAGEMENT
355 
356   if (HAS_PENDING_EXCEPTION) {
357     // management agent fails to start possibly due to
358     // configuration problem and is responsible for printing
359     // stack trace if appropriate. Simply exit VM.
360     vm_exit(1);
361   }
362 
363   if (Arguments::has_profile())       FlatProfiler::engage(main_thread, true);
364   if (MemProfiling)                   MemProfiler::engage();
365   StatSampler::engage();
366   if (CheckJNICalls)                  JniPeriodicChecker::engage();
367 
368   BiasedLocking::init();
369 
370   if (JDK_Version::current().post_vm_init_hook_enabled()) {
371     call_postVMInitHook(THREAD);
372     // The Java side of PostVMInitHook.run must deal with all
373     // exceptions and provide means of diagnosis.
374     if (HAS_PENDING_EXCEPTION) {
375       CLEAR_PENDING_EXCEPTION;
376     }
377   }
378 
379   {
380       MutexLockerEx ml(PeriodicTask_lock, Mutex::_no_safepoint_check_flag);
381       // Make sure the watcher thread can be started by WatcherThread::start()
382       // or by dynamic enrollment.
383       WatcherThread::make_startable();
384       // Start up the WatcherThread if there are any periodic tasks
385       // NOTE:  All PeriodicTasks should be registered by now. If they
386       //   aren't, late joiners might appear to start slowly (we might
387       //   take a while to process their first tick).
388       if (PeriodicTask::num_tasks() > 0) {
389           WatcherThread::start();
390       }
391   }
392 
393   // Give os specific code one last chance to start
394   os::init_3();
395 
396   create_vm_timer.end();
397 #ifdef ASSERT
398   _vm_complete = true;
399 #endif
400   return JNI_OK;
401 }
```

309 行处，看到了os::signal_init() 的调用（**hotspot/src/share/vm/runtime/os.cpp**），这就是我们要找的。接着，我们看下其具体实现

```
 1 void os::signal_init() {
 2   if (!ReduceSignalUsage) {
 3     // Setup JavaThread for processing signals
 4     EXCEPTION_MARK;
 5     Klass* k = SystemDictionary::resolve_or_fail(vmSymbols::java_lang_Thread(), true, CHECK);
 6     instanceKlassHandle klass (THREAD, k);
 7     instanceHandle thread_oop = klass->allocate_instance_handle(CHECK);
 8 
 9     const char thread_name[] = "Signal Dispatcher";
10     Handle string = java_lang_String::create_from_str(thread_name, CHECK);
11 
12     // Initialize thread_oop to put it into the system threadGroup
13     Handle thread_group (THREAD, Universe::system_thread_group());
14     JavaValue result(T_VOID);
15     JavaCalls::call_special(&result, thread_oop,
16                            klass,
17                            vmSymbols::object_initializer_name(),
18                            vmSymbols::threadgroup_string_void_signature(),
19                            thread_group,
20                            string,
21                            CHECK);
22 
23     KlassHandle group(THREAD, SystemDictionary::ThreadGroup_klass());
24     JavaCalls::call_special(&result,
25                             thread_group,
26                             group,
27                             vmSymbols::add_method_name(),
28                             vmSymbols::thread_void_signature(),
29                             thread_oop,         // ARG 1
30                             CHECK);
31 
32     os::signal_init_pd();
33 
34     { MutexLocker mu(Threads_lock);
35       JavaThread* signal_thread = new JavaThread(&signal_thread_entry);
36 
37       // At this point it may be possible that no osthread was created for the
38       // JavaThread due to lack of memory. We would have to throw an exception
39       // in that case. However, since this must work and we do not allow
40       // exceptions anyway, check and abort if this fails.
41       if (signal_thread == NULL || signal_thread->osthread() == NULL) {
42         vm_exit_during_initialization("java.lang.OutOfMemoryError",
43                                       "unable to create new native thread");
44       }
45 
46       java_lang_Thread::set_thread(thread_oop(), signal_thread);
47       java_lang_Thread::set_priority(thread_oop(), NearMaxPriority);
48       java_lang_Thread::set_daemon(thread_oop());
49 
50       signal_thread->set_threadObj(thread_oop());
51       Threads::add(signal_thread);
52       Thread::start(signal_thread);
53     }
54     // Handle ^BREAK
55     os::signal(SIGBREAK, os::user_handler());
56   }
57 }
```

这里的完全可以看出来，在此函数中**35行处**，创建了一个 java 线程，用于执行**signal_thread_entry** 函数，那我们来看看，这个 signal_thread_entry 函数到底做了什么？

```
 1 // sigexitnum_pd is a platform-specific special signal used for terminating the Signal thread.
 2 static void signal_thread_entry(JavaThread* thread, TRAPS) {
 3   os::set_priority(thread, NearMaxPriority);
 4   while (true) {
 5     int sig;
 6     {
 7       // FIXME : Currently we have not decieded what should be the status
 8       //         for this java thread blocked here. Once we decide about
 9       //         that we should fix this.
10       sig = os::signal_wait();
11     }
12     if (sig == os::sigexitnum_pd()) {
13        // Terminate the signal thread
14        return;
15     }
16 
17     switch (sig) {
18       case SIGBREAK: {
19         // Check if the signal is a trigger to start the Attach Listener - in that
20         // case don't print stack traces.
21         if (!DisableAttachMechanism && AttachListener::is_init_trigger()) {
22           continue;
23         }
24         // Print stack traces
25         // Any SIGBREAK operations added here should make sure to flush
26         // the output stream (e.g. tty->flush()) after output.  See 4803766.
27         // Each module also prints an extra carriage return after its output.
28         VM_PrintThreads op;
29         VMThread::execute(&op);
30         VM_PrintJNI jni_op;
31         VMThread::execute(&jni_op);
32         VM_FindDeadlocks op1(tty);
33         VMThread::execute(&op1);
34         Universe::print_heap_at_SIGBREAK();
35         if (PrintClassHistogram) {
36           VM_GC_HeapInspection op1(gclog_or_tty, true /* force full GC before heap inspection */);
37           VMThread::execute(&op1);
38         }
39         if (JvmtiExport::should_post_data_dump()) {
40           JvmtiExport::post_data_dump();
41         }
42         break;
43       }
44       default: {
45         // Dispatch the signal to java
46         HandleMark hm(THREAD);
47         Klass* k = SystemDictionary::resolve_or_null(vmSymbols::sun_misc_Signal(), THREAD);
48         KlassHandle klass (THREAD, k);
49         if (klass.not_null()) {
50           JavaValue result(T_VOID);
51           JavaCallArguments args;
52           args.push_int(sig);
53           JavaCalls::call_static(
54             &result,
55             klass,
56             vmSymbols::dispatch_name(),
57             vmSymbols::int_void_signature(),
58             &args,
59             THREAD
60           );
61         }
62         if (HAS_PENDING_EXCEPTION) {
63           // tty is initialized early so we don't expect it to be null, but
64           // if it is we can't risk doing an initialization that might
65           // trigger additional out-of-memory conditions
66           if (tty != NULL) {
67             char klass_name[256];
68             char tmp_sig_name[16];
69             const char* sig_name = "UNKNOWN";
70             InstanceKlass::cast(PENDING_EXCEPTION->klass())->
71               name()->as_klass_external_name(klass_name, 256);
72             if (os::exception_name(sig, tmp_sig_name, 16) != NULL)
73               sig_name = tmp_sig_name;
74             warning("Exception %s occurred dispatching signal %s to handler"
75                     "- the VM may need to be forcibly terminated",
76                     klass_name, sig_name );
77           }
78           CLEAR_PENDING_EXCEPTION;
79         }
80       }
81     }
82   }
83 }
```

函数里面意思已经很清晰明了了，首先在10行处，有一个os::signal_wait()的调用，该调用的主要是阻塞当前线程，并等待接收系统信号，然后再根据接收到的信号 sig 做 switch 逻辑，对于不同的信号做不同的处理。至此，关于“**目标 JVM 对OS信号监听的实现**”这一点，就已经分析结束了。简单的一句话总结就是，JVM 在启动的时候，会创建一个名为“Signal Dispatcher”的线程用于接收os 的信号，以便对不同信号分别做处理。

## 4.2. 文件 Socket 通信的通道的创建

经过3.1的分析，我们已经知道在 JVM 启动之后，内部会有线程监听并处理 os 的信号，那么，这个时候，如果我们想和已经启动的 JVM 建立通信，当然就可以毫不犹豫的使用信号来进行了。不过，基于信号的通信，也是存在限制的，一方面，os 支持的信号是有限的，二来信号的通信往往是单向的，不方便通信双方进行高效的通信。基于这些，笔者认为，为了使得 Client JVM 和 Target JVM 更好的通信，就采用了 Socket 通信来实现二者的通信。那接下来我们看看，这个通道究竟是如何创建的？

当我们需要 attach 到某个目标 JVM 进程上去的时候，我们通常会写如下代码

```
1 VirtualMachine vm = VirtualMachine.attach(pid);
```

这样我们就能得到目标 JVM 的相关信息了，是不是很简单？不过，今天我们要做的可不是这么简单的事情，我们需要深入其后，了解其根本。接下来我们就以**com.sun.tools.attach.VirtualMachine**的 **attach** 方法入手，逐层揭开其神秘面纱。

```
 1 public static VirtualMachine attach(String id)
 2         throws AttachNotSupportedException, IOException
 3     {
 4         if (id == null) {
 5             throw new NullPointerException("id cannot be null");
 6         }
 7         List<AttachProvider> providers = AttachProvider.providers();
 8         if (providers.size() == 0) {
 9             throw new AttachNotSupportedException("no providers installed");
10         }
11         AttachNotSupportedException lastExc = null;
12         for (AttachProvider provider: providers) {
13             try {
14                 return provider.attachVirtualMachine(id);
15             } catch (AttachNotSupportedException x) {
16                 lastExc = x;
17             }
18         }
19         throw lastExc;
20     }
```

这是attach的源码，入参为目标 JVM 的进程 ID，其实现委派给了 AttachProvider 了，通过provider.attachVirtualMachine(id);来实现真正的 attach 操作。由于 AttachProvider 是个抽象类，所以这个方法的真正实现在子类中，在 Linux 环境下，我们看 **sun.tools.attach.BsdAttachProvider.java** 的实现。

```
 1 public VirtualMachine attachVirtualMachine(String vmid)
 2         throws AttachNotSupportedException, IOException
 3     {
 4         checkAttachPermission();
 5 
 6         // AttachNotSupportedException will be thrown if the target VM can be determined
 7         // to be not attachable.
 8         testAttachable(vmid);
 9 
10         return new BsdVirtualMachine(this, vmid);
11     }
```

这个方法非常简单，就是 new 了一个 **BsdVirtualMachine** 对象，并且把目标进程 ID 带过去了。看**sun.tools.attach.BsdVirtualMachine.java** 的构造函数

```
 1 /**
 2      * Attaches to the target VM
 3      */
 4     BsdVirtualMachine(AttachProvider provider, String vmid)
 5         throws AttachNotSupportedException, IOException
 6     {
 7         super(provider, vmid);
 8 
 9         // This provider only understands pids
10         int pid;
11         try {
12             pid = Integer.parseInt(vmid);
13         } catch (NumberFormatException x) {
14             throw new AttachNotSupportedException("Invalid process identifier");
15         }
16 
17         // Find the socket file. If not found then we attempt to start the
18         // attach mechanism in the target VM by sending it a QUIT signal.
19         // Then we attempt to find the socket file again.
20         path = findSocketFile(pid);
21         if (path == null) {
22             File f = new File(tmpdir, ".attach_pid" + pid);
23             createAttachFile(f.getPath());
24             try {
25                 sendQuitTo(pid);
26 
27                 // give the target VM time to start the attach mechanism
28                 int i = 0;
29                 long delay = 200;
30                 int retries = (int)(attachTimeout() / delay);
31                 do {
32                     try {
33                         Thread.sleep(delay);
34                     } catch (InterruptedException x) { }
35                     path = findSocketFile(pid);
36                     i++;
37                 } while (i <= retries && path == null);
38                 if (path == null) {
39                     throw new AttachNotSupportedException(
40                         "Unable to open socket file: target process not responding " +
41                         "or HotSpot VM not loaded");
42                 }
43             } finally {
44                 f.delete();
45             }
46         }
47 
48         // Check that the file owner/permission to avoid attaching to
49         // bogus process
50         checkPermissions(path);
51 
52         // Check that we can connect to the process
53         // - this ensures we throw the permission denied error now rather than
54         // later when we attempt to enqueue a command.
55         int s = socket();
56         try {
57             connect(s, path);
58         } finally {
59             close(s);
60         }
61     }
```

首先看20行处的findSocketFile(pid);这里是找对应的 socket （/tmp/.java_pid${pid}）文件，这个文件就是我们在第二大点图中画出来的，用于进程间通信的 socket 文件，如果不存在，即第一次进入该方法的时候。这时会运行到74行的createAttachFile(f.getPath());来创建一个attach 文件，socket 文件的命名方式为：/tmp/../.attach_pid${pid}，关于这两个方法（findSocketFile和createAttachFile）的具体实现，这里就不展开了，感兴趣的可以直接去查看**jdk/src/solaris/native/sun/tools/attach/BsdVirtualMachine.c**的相关源码。然后就会运行到一个非常关键的方法25**行的sendQuitTo(pid);**这个方法的实现，我们等会进入BsdVirtualMachine.c看下源码，其主要目的就是给该进程发送一个信号。之后会进入到31行处的 do...while循环，自旋反复轮询指定的次数来获取该 socket 文件的路径，直到超时或者 path（即 socket 文件路径） 不为空，最后在**55行**处，建立一个 socket，并且在57行处通过 path 进行 socket 的连接，从而完成了客户端（Client JVM）到目标进程（Target JVM）的 socket 通道建立。不过，请打住，这里是不是少了点什么？我相信细心的你肯定发现了，至少还存2个问题，

**1. Target JVM 的 socket 服务端是何时创建的？**

**2. 用于通信的 socket 文件是在哪里创建的？**

带着这两个问题，我们进入25行关键方法**sendQuitTo(pid);**的源码解读，该方法是个本地方法，位于**jdk/src/solaris/native/sun/tools/attach/BsdVirtualMachine.c中**

```
 1 /*
 2  * Class:     sun_tools_attach_BsdVirtualMachine
 3  * Method:    sendQuitTo
 4  * Signature: (I)V
 5  */
 6 JNIEXPORT void JNICALL Java_sun_tools_attach_BsdVirtualMachine_sendQuitTo
 7   (JNIEnv *env, jclass cls, jint pid)
 8 {
 9     if (kill((pid_t)pid, SIGQUIT)) {
10         JNU_ThrowIOExceptionWithLastError(env, "kill");
11     }
12 }
```

看到第9行的时候，是不是觉得这里必然和前面3.1中大篇幅分析的信号处理线程“Signal Dispatcher”有种必然联系了？没错，这里就是通过 kill 这个系统调用像目标 JVM，发送了一个 SIGQUIT 的信号，该信号是个#define，即宏，表示的数字“3”，即类似在 linux 命令行执行了“kill -3 ${pid}”的操做（其实，这个命令正是获取目标 JVM 线程 dump 文件的一种方式，读者可以试试）。既然这里向目标 JVM 发送了这么个信号，那么我们现在就移步到3.1中讲到过的 signal_thread_entry 方法中去。

```
 1 static void signal_thread_entry(JavaThread* thread, TRAPS) {
 2   os::set_priority(thread, NearMaxPriority);
 3   while (true) {
 4     int sig;
 5     {
 6       // FIXME : Currently we have not decieded what should be the status
 7       //         for this java thread blocked here. Once we decide about
 8       //         that we should fix this.
 9       sig = os::signal_wait();
10     }
11     if (sig == os::sigexitnum_pd()) {
12        // Terminate the signal thread
13        return;
14     }
15 
16     switch (sig) {
17       case SIGBREAK: {
18         // Check if the signal is a trigger to start the Attach Listener - in that
19         // case don't print stack traces.
20         if (!DisableAttachMechanism && AttachListener::is_init_trigger()) {
21           continue;
22         }
23         // Print stack traces
24         // Any SIGBREAK operations added here should make sure to flush
25         // the output stream (e.g. tty->flush()) after output.  See 4803766.
26         // Each module also prints an extra carriage return after its output.
27         VM_PrintThreads op;
28         VMThread::execute(&op);
29         VM_PrintJNI jni_op;
30         VMThread::execute(&jni_op);
31         VM_FindDeadlocks op1(tty);
32         VMThread::execute(&op1);
33         Universe::print_heap_at_SIGBREAK();
34         if (PrintClassHistogram) {
35           VM_GC_HeapInspection op1(gclog_or_tty, true /* force full GC before heap inspection */);
36           VMThread::execute(&op1);
37         }
38         if (JvmtiExport::should_post_data_dump()) {
39           JvmtiExport::post_data_dump();
40         }
41         break;
42       }
43       default: {
44         // Dispatch the signal to java
45         HandleMark hm(THREAD);
46         Klass* k = SystemDictionary::resolve_or_null(vmSymbols::sun_misc_Signal(), THREAD);
47         KlassHandle klass (THREAD, k);
48         if (klass.not_null()) {
49           JavaValue result(T_VOID);
50           JavaCallArguments args;
51           args.push_int(sig);
52           JavaCalls::call_static(
53             &result,
54             klass,
55             vmSymbols::dispatch_name(),
56             vmSymbols::int_void_signature(),
57             &args,
58             THREAD
59           );
60         }
61         if (HAS_PENDING_EXCEPTION) {
62           // tty is initialized early so we don't expect it to be null, but
63           // if it is we can't risk doing an initialization that might
64           // trigger additional out-of-memory conditions
65           if (tty != NULL) {
66             char klass_name[256];
67             char tmp_sig_name[16];
68             const char* sig_name = "UNKNOWN";
69             InstanceKlass::cast(PENDING_EXCEPTION->klass())->
70               name()->as_klass_external_name(klass_name, 256);
71             if (os::exception_name(sig, tmp_sig_name, 16) != NULL)
72               sig_name = tmp_sig_name;
73             warning("Exception %s occurred dispatching signal %s to handler"
74                     "- the VM may need to be forcibly terminated",
75                     klass_name, sig_name );
76           }
77           CLEAR_PENDING_EXCEPTION;
78         }
79       }
80     }
81   }
82 }
```

这里的**17行**，我们看到了有个对 SIGBREAK（宏定义） 信号处理的 case，事实上，这个SIGBREAK和前面客户端发过来的SIGQUIT 的值是一样的，都是“3”，熟悉 C语言的读者应该不难理解。所以，当客户端发送这个信号给目标 JVM 时，就理所应当的进入了这个 case 的处理逻辑。**这里的27行到40行，事实上就是对“kill -3 ${pid}”执行时对应的处理逻辑“进行目标 JVM 进程的线程 dump 操作”**。现在我们重点关注一下20行的 if 语句，第一个 boolean 值，某认情况下是false（可通过/hotspot/src/share/vm/runtime/globals.c）查看，表示某认情况下是不禁止attach 机制的，于是就会进入第二个条件的判断AttachListener::is_init_trigger()，这里的判断还是比较有意思的（即判断当前杀是不是需要进行 attach 的初始化操作），我们进入源码，源码的文件为：**hotspot/src/os/bsd/vm/attachListener_bsd.cpp**

```
 1 // If the file .attach_pid<pid> exists in the working directory
 2 // or /tmp then this is the trigger to start the attach mechanism
 3 bool AttachListener::is_init_trigger() {
 4   if (init_at_startup() || is_initialized()) {
 5     return false;               // initialized at startup or already initialized
 6   }
 7   char path[PATH_MAX + 1];
 8   int ret;
 9   struct stat st;
10 
11   snprintf(path, PATH_MAX + 1, "%s/.attach_pid%d",
12            os::get_temp_directory(), os::current_process_id());
13   RESTARTABLE(::stat(path, &st), ret);
14   if (ret == 0) {
15     // simple check to avoid starting the attach mechanism when
16     // a bogus user creates the file
17     if (st.st_uid == geteuid()) {
18       init();
19       return true;
20     }
21   }
22   return false;
23 }
```

方法进入的第一行，即判断是不是在 JVM 启动时就初始化或者之前已经初始化过，如果是，则直接返回，否则继续当前方法。方法的第11行，是在处理/tmp/attach_pid${pid}路径（这个文件就是 Client JVM 在attach 时创建的），并把 path 传入13行定义的宏进行判断，如果这个文件存在，且刚好是当前用户的创建的 attach_pid 文件，则进入18行的 init() 方法，否则什么也不做，返回 false。接着我们进入 **init** 的源码(hotspot/src/share/vm/services/attachListener.cpp)

```
 1 // Starts the Attach Listener thread
 2 void AttachListener::init() {
 3   EXCEPTION_MARK;
 4   Klass* k = SystemDictionary::resolve_or_fail(vmSymbols::java_lang_Thread(), true, CHECK);
 5   instanceKlassHandle klass (THREAD, k);
 6   instanceHandle thread_oop = klass->allocate_instance_handle(CHECK);
 7 
 8   const char thread_name[] = "Attach Listener";
 9   Handle string = java_lang_String::create_from_str(thread_name, CHECK);
10 
11   // Initialize thread_oop to put it into the system threadGroup
12   Handle thread_group (THREAD, Universe::system_thread_group());
13   JavaValue result(T_VOID);
14   JavaCalls::call_special(&result, thread_oop,
15                        klass,
16                        vmSymbols::object_initializer_name(),
17                        vmSymbols::threadgroup_string_void_signature(),
18                        thread_group,
19                        string,
20                        THREAD);
21 
22   if (HAS_PENDING_EXCEPTION) {
23     tty->print_cr("Exception in VM (AttachListener::init) : ");
24     java_lang_Throwable::print(PENDING_EXCEPTION, tty);
25     tty->cr();
26 
27     CLEAR_PENDING_EXCEPTION;
28 
29     return;
30   }
31 
32   KlassHandle group(THREAD, SystemDictionary::ThreadGroup_klass());
33   JavaCalls::call_special(&result,
34                         thread_group,
35                         group,
36                         vmSymbols::add_method_name(),
37                         vmSymbols::thread_void_signature(),
38                         thread_oop,             // ARG 1
39                         THREAD);
40 
41   if (HAS_PENDING_EXCEPTION) {
42     tty->print_cr("Exception in VM (AttachListener::init) : ");
43     java_lang_Throwable::print(PENDING_EXCEPTION, tty);
44     tty->cr();
45 
46     CLEAR_PENDING_EXCEPTION;
47 
48     return;
49   }
50 
51   { MutexLocker mu(Threads_lock);
52     JavaThread* listener_thread = new JavaThread(&attach_listener_thread_entry);
53 
54     // Check that thread and osthread were created
55     if (listener_thread == NULL || listener_thread->osthread() == NULL) {
56       vm_exit_during_initialization("java.lang.OutOfMemoryError",
57                                     "unable to create new native thread");
58     }
59 
60     java_lang_Thread::set_thread(thread_oop(), listener_thread);
61     java_lang_Thread::set_daemon(thread_oop());
62 
63     listener_thread->set_threadObj(thread_oop());
64     Threads::add(listener_thread);
65     Thread::start(listener_thread);
66   }
67 }
```

从源码中，我们可以看出来，这里最主要的功能是，创建一个名为“Attach Listener”的 Java 线程，该线程启动后会调用**attach_listener_thread_entry**这个方法（52行），来完成有关的任务处理。进入**attach_listener_thread_entry**方法

```
 1 // The Attach Listener threads services a queue. It dequeues an operation
 2 // from the queue, examines the operation name (command), and dispatches
 3 // to the corresponding function to perform the operation.
 4 
 5 static void attach_listener_thread_entry(JavaThread* thread, TRAPS) {
 6   os::set_priority(thread, NearMaxPriority);
 7 
 8   thread->record_stack_base_and_size();
 9 
10   if (AttachListener::pd_init() != 0) {
11     return;
12   }
13   AttachListener::set_initialized();
14 
15   for (;;) {
16     AttachOperation* op = AttachListener::dequeue();
17     if (op == NULL) {
18       return;   // dequeue failed or shutdown
19     }
20 
21     ResourceMark rm;
22     bufferedStream st;
23     jint res = JNI_OK;
24 
25     // handle special detachall operation
26     if (strcmp(op->name(), AttachOperation::detachall_operation_name()) == 0) {
27       AttachListener::detachall();
28     } else {
29       // find the function to dispatch too
30       AttachOperationFunctionInfo* info = NULL;
31       for (int i=0; funcs[i].name != NULL; i++) {
32         const char* name = funcs[i].name;
33         assert(strlen(name) <= AttachOperation::name_length_max, "operation <= name_length_max");
34         if (strcmp(op->name(), name) == 0) {
35           info = &(funcs[i]);
36           break;
37         }
38       }
39 
40       // check for platform dependent attach operation
41       if (info == NULL) {
42         info = AttachListener::pd_find_operation(op->name());
43       }
44 
45       if (info != NULL) {
46         // dispatch to the function that implements this operation
47         res = (info->func)(op, &st);
48       } else {
49         st.print("Operation %s not recognized!", op->name());
50         res = JNI_ERR;
51       }
52     }
53 
54     // operation complete - send result and output to client
55     op->complete(res, &st);
56   }
57 }
```

这里需要关注两个方面的内容，

**第一、第10行的AttachListener::pd_init()；**

**第二、第15行开始的 for 循环里面的内容。**

**首先看AttachListener::pd_init()**

```
 1 int AttachListener::pd_init() {
 2   JavaThread* thread = JavaThread::current();
 3   ThreadBlockInVM tbivm(thread);
 4 
 5   thread->set_suspend_equivalent();
 6   // cleared by handle_special_suspend_equivalent_condition() or
 7   // java_suspend_self() via check_and_wait_while_suspended()
 8 
 9   int ret_code = BsdAttachListener::init();
10 
11   // were we externally suspended while we were waiting?
12   thread->check_and_wait_while_suspended();
13 
14   return ret_code;
15 }
```

以上的 pd_init() 方法是在hotspot/src/os/bsd/vm/attachListener_bsd.cpp中实现的，我们看第9行的代码，调用了BsdAttachListener::init()一个这样的方法，该方法的主要作用就是生产 socket 通信文件的。源码如下

```
 1 // Initialization - create a listener socket and bind it to a file
 2 
 3 int BsdAttachListener::init() {
 4   char path[UNIX_PATH_MAX];          // socket file
 5   char initial_path[UNIX_PATH_MAX];  // socket file during setup
 6   int listener;                      // listener socket (file descriptor)
 7 
 8   // register function to cleanup
 9   ::atexit(listener_cleanup);
10 
11   int n = snprintf(path, UNIX_PATH_MAX, "%s/.java_pid%d",
12                    os::get_temp_directory(), os::current_process_id());
13   if (n < (int)UNIX_PATH_MAX) {
14     n = snprintf(initial_path, UNIX_PATH_MAX, "%s.tmp", path);
15   }
16   if (n >= (int)UNIX_PATH_MAX) {
17     return -1;
18   }
19 
20   // create the listener socket
21   listener = ::socket(PF_UNIX, SOCK_STREAM, 0);
22   if (listener == -1) {
23     return -1;
24   }
25 
26   // bind socket
27   struct sockaddr_un addr;
28   addr.sun_family = AF_UNIX;
29   strcpy(addr.sun_path, initial_path);
30   ::unlink(initial_path);
31   int res = ::bind(listener, (struct sockaddr*)&addr, sizeof(addr));
32   if (res == -1) {
33     ::close(listener);
34     return -1;
35   }
36 
37   // put in listen mode, set permissions, and rename into place
38   res = ::listen(listener, 5);
39   if (res == 0) {
40     RESTARTABLE(::chmod(initial_path, S_IREAD|S_IWRITE), res);
41     if (res == 0) {
42       // make sure the file is owned by the effective user and effective group
43       // (this is the default on linux, but not on mac os)
44       RESTARTABLE(::chown(initial_path, geteuid(), getegid()), res);
45       if (res == 0) {
46         res = ::rename(initial_path, path);
47       }
48     }
49   }
50   if (res == -1) {
51     ::close(listener);
52     ::unlink(initial_path);
53     return -1;
54   }
55   set_path(path);
56   set_listener(listener);
57 
58   return 0;
59 }
```

从方法的注释，就能看出这个方法就是用来创建一个基于文件的 socket 的 listener 端，即服务端的。具体的创建过程，代码写的已经很清楚了，我做下简单描述，11行处，构建 socket 通信文件的路径（/tmp/.java_pid${pid}）,21 行，创建一个 socket，其中关注 socket 函数的第一个参数，当为 PF_UNIX 时，表示创建文件 socket，详情可以参考 linux 的 socket 函数说明，然后到29行，将 socket 文件 path 拷贝到 socket 通信地址中，即以此文件作为通信地址，然后在31行时，将 socket 和该 socket 文件地址做一个绑定，38行，表示对当前 socket 进行监听（数字5表示监听时可容纳客户端连接的队列的大小），如果有Client JVM 的客户端连接上来，并且发送了相关消息，该服务端就可以对其进行相应处理了。**至此，进程间 socket 的通信的通道就建立了。**

**其次看下 for 循环做了什么？**其实很简单，16行，**BsdAttachListener::dequeue()** 从监听器的队列中拿到一个 Client JVM 的AttachOperation（当客户端 attach 上 target JVM 之后，往目标 JVM 发送任意 socket 信息，都会被放置到这个队列中，等待被处理），此处会被阻塞，直到收到请求，如下源码的13行， socket 的 accept 函数处于等待状态，等待来之客户端 JVM 的相关请求，一旦获取到请求，则将请求组装好返回给调用者一个BadAttachOperation 对象。

```
 1 // Dequeue an operation
 2 //
 3 // In the Bsd implementation there is only a single operation and clients
 4 // cannot queue commands (except at the socket level).
 5 //
 6 BsdAttachOperation* BsdAttachListener::dequeue() {
 7   for (;;) {
 8     int s;
 9 
10     // wait for client to connect
11     struct sockaddr addr;
12     socklen_t len = sizeof(addr);
13     RESTARTABLE(::accept(listener(), &addr, &len), s);
14     if (s == -1) {
15       return NULL;      // log a warning?
16     }
17 
18     // get the credentials of the peer and check the effective uid/guid
19     // - check with jeff on this.
20     uid_t puid;
21     gid_t pgid;
22     if (::getpeereid(s, &puid, &pgid) != 0) {
23       ::close(s);
24       continue;
25     }
26     uid_t euid = geteuid();
27     gid_t egid = getegid();
28 
29     if (puid != euid || pgid != egid) {
30       ::close(s);
31       continue;
32     }
33 
34     // peer credential look okay so we read the request
35     BsdAttachOperation* op = read_request(s);
36     if (op == NULL) {
37       ::close(s);
38       continue;
39     } else {
40       return op;
41     }
42   }
43 }
```

所以，只要收到一个AttachOperation不为“detachall”的操纵请求就会进入到45行处进行处理，这里的目的就是为了拿到对应的操作AttachOperationFunctionInfo对象，如果不为空，则调用其func，来完成对客户端的响应，如47行所示。AttachOperationFunctionInfo（/hotspot/src/share/vm/services/attachListener.cpp）的定义如下

```
 1 // names must be of length <= AttachOperation::name_length_max
 2 static AttachOperationFunctionInfo funcs[] = {
 3   { "agentProperties",  get_agent_properties },
 4   { "datadump",         data_dump },
 5   { "dumpheap",         dump_heap },
 6   { "load",             JvmtiExport::load_agent_library },
 7   { "properties",       get_system_properties },
 8   { "threaddump",       thread_dump },
 9   { "inspectheap",      heap_inspection },
10   { "setflag",          set_flag },
11   { "printflag",        print_flag },
12   { "jcmd",             jcmd },
13   { NULL,               NULL }
14 };
```

从这里，我们可以看到，threaddump、dumpheap 等我们常用的操纵。到此为止，水落石出，涉及到 attach 操纵的服务端的原理基本已经理清楚了。接下来我们以 jstack 为例，来看下客户端 JVM 是不是确实是以我们上面分析出来的方式与服务端 JVM 进行通信，并获取到它想要的内容的。

## 4.3. JVM 对 Attach 上来的进程的命令的响应，以 jstack -l 为例

我们首先进入 jstack 的源码，源码目录为jdk/src/share/classes/sun/tools/jstack/JStack.java。进入 main 函数

```
 1 public static void main(String[] args) throws Exception {
 2         if (args.length == 0) {
 3             usage(1); // no arguments
 4         }
 5 
 6         boolean useSA = false;
 7         boolean mixed = false;
 8         boolean locks = false;
 9 
10         // Parse the options (arguments starting with "-" )
11         int optionCount = 0;
12         while (optionCount < args.length) {
13             String arg = args[optionCount];
14             if (!arg.startsWith("-")) {
15                 break;
16             }
17             if (arg.equals("-help") || arg.equals("-h")) {
18                 usage(0);
19             }
20             else if (arg.equals("-F")) {
21                 useSA = true;
22             }
23             else {
24                 if (arg.equals("-m")) {
25                     mixed = true;
26                 } else {
27                     if (arg.equals("-l")) {
28                        locks = true;
29                     } else {
30                         usage(1);
31                     }
32                 }
33             }
34             optionCount++;
35         }
36 
37         // mixed stack implies SA tool
38         if (mixed) {
39             useSA = true;
40         }
41 
42         // Next we check the parameter count. If there are two parameters
43         // we assume core file and executable so we use SA.
44         int paramCount = args.length - optionCount;
45         if (paramCount == 0 || paramCount > 2) {
46             usage(1);
47         }
48         if (paramCount == 2) {
49             useSA = true;
50         } else {
51             // If we can't parse it as a pid then it must be debug server
52             if (!args[optionCount].matches("[0-9]+")) {
53                 useSA = true;
54             }
55         }
56 
57         // now execute using the SA JStack tool or the built-in thread dumper
58         if (useSA) {
59             // parameters (<pid> or <exe> <core>
60             String params[] = new String[paramCount];
61             for (int i=optionCount; i<args.length; i++ ){
62                 params[i-optionCount] = args[i];
63             }
64             runJStackTool(mixed, locks, params);
65         } else {
66             // pass -l to thread dump operation to get extra lock info
67             String pid = args[optionCount];
68             String params[];
69             if (locks) {
70                 params = new String[] { "-l" };
71             } else {
72                 params = new String[0];
73             }
74             runThreadDump(pid, params);
75         }
76     }
```

当采用 jstack -l 时，会走65行的 else 分支，最终执行77行的runThreadDump方法

```
 1 // Attach to pid and perform a thread dump
 2     private static void runThreadDump(String pid, String args[]) throws Exception {
 3         VirtualMachine vm = null;
 4         try {
 5             vm = VirtualMachine.attach(pid);
 6         } catch (Exception x) {
 7             String msg = x.getMessage();
 8             if (msg != null) {
 9                 System.err.println(pid + ": " + msg);
10             } else {
11                 x.printStackTrace();
12             }
13             if ((x instanceof AttachNotSupportedException) &&
14                 (loadSAClass() != null)) {
15                 System.err.println("The -F option can be used when the target " +
16                     "process is not responding");
17             }
18             System.exit(1);
19         }
20 
21         // Cast to HotSpotVirtualMachine as this is implementation specific
22         // method.
23         InputStream in = ((HotSpotVirtualMachine)vm).remoteDataDump((Object[])args);
24 
25         // read to EOF and just print output
26         byte b[] = new byte[256];
27         int n;
28         do {
29             n = in.read(b);
30             if (n > 0) {
31                 String s = new String(b, 0, n, "UTF-8");
32                 System.out.print(s);
33             }
34         } while (n > 0);
35         in.close();
36         vm.detach();
37     }
```

5行：执行VirtualMachine.attach(pid);则会达到3.1、3.2的效果，即服务端已经做好了所有 attach 所需的准备，如 socket 服务端、socket 通信文件、socket 请求处理线程“Attach Listener”。

23行：通过调用 HotSpotVirtualMachine 对象的 remoteDataDump 函数进行远程 dump，获得输入流 InputStream in，最后通过读取输入流的内容，来通过标准输出流输出从服务端获取的数据。至此，jstack -l 命令完成所有操作。

接下来，我们重点分析**HotSpotVirtualMachine 对象的 remoteDataDump** 函数。首先上HotSpotVirtualMachine（/jdk/src/share/classes/sun/tools/attach/HotSpotVirtualMachine.java） 对象的 remoteDataDump的源码

```
1 // Remote ctrl-break. The output of the ctrl-break actions can
2     // be read from the input stream.
3     public InputStream remoteDataDump(Object ... args) throws IOException {
4         return executeCommand("threaddump", args);
5     }
```

请注意4行的 cmd 字符串为“threaddump”，这个和3.2中 AttachOperationFunctionInfo 的定义是吻合的，也就是说最终在服务端会调用 thread_dump 方法，来执行线程 dump，并将结果返回给客户端。接着我们看下下 executeCommand方法，该方法只是简单的调用 execute 方法，如下

```
 1 /*
 2      * Convenience method for simple commands
 3      */
 4     private InputStream executeCommand(String cmd, Object ... args) throws IOException {
 5         try {
 6             return execute(cmd, args);
 7         } catch (AgentLoadException x) {
 8             throw new InternalError("Should not get here", x);
 9         }
10     }
```

exectue 方法在该类中为抽象方法，其具体实现放在了sun.tools.attach.BsdVirtualMachine.java中，我们看下在其具体实现这里可是最最关键的地方了

```
 1 /**
 2      * Execute the given command in the target VM.
 3      */
 4     InputStream execute(String cmd, Object ... args) throws AgentLoadException, IOException {
 5         assert args.length <= 3;                // includes null
 6 
 7         // did we detach?
 8         String p;
 9         synchronized (this) {
10             if (this.path == null) {
11                 throw new IOException("Detached from target VM");
12             }
13             p = this.path;
14         }
15 
16         // create UNIX socket
17         int s = socket();
18 
19         // connect to target VM
20         try {
21             connect(s, p);
22         } catch (IOException x) {
23             close(s);
24             throw x;
25         }
26 
27         IOException ioe = null;
28 
29         // connected - write request
30         // <ver> <cmd> <args...>
31         try {
32             writeString(s, PROTOCOL_VERSION);
33             writeString(s, cmd);
34 
35             for (int i=0; i<3; i++) {
36                 if (i < args.length && args[i] != null) {
37                     writeString(s, (String)args[i]);
38                 } else {
39                     writeString(s, "");
40                 }
41             }
42         } catch (IOException x) {
43             ioe = x;
44         }
45 
46 
47         // Create an input stream to read reply
48         SocketInputStream sis = new SocketInputStream(s);
49 
50         // Read the command completion status
51         int completionStatus;
52         try {
53             completionStatus = readInt(sis);
54         } catch (IOException x) {
55             sis.close();
56             if (ioe != null) {
57                 throw ioe;
58             } else {
59                 throw x;
60             }
61         }
62 
63         if (completionStatus != 0) {
64             sis.close();
65 
66             // In the event of a protocol mismatch then the target VM
67             // returns a known error so that we can throw a reasonable
68             // error.
69             if (completionStatus == ATTACH_ERROR_BADVERSION) {
70                 throw new IOException("Protocol mismatch with target VM");
71             }
72 
73             // Special-case the "load" command so that the right exception is
74             // thrown.
75             if (cmd.equals("load")) {
76                 throw new AgentLoadException("Failed to load agent library");
77             } else {
78                 throw new IOException("Command failed in target VM");
79             }
80         }
81 
82         // Return the input stream so that the command output can be read
83         return sis;
84     }
```

17行：创建一个 socket，这个是 socket 是一个 jni 本地方法，有兴趣的可以去看对应的实现，源码在jdk/src/solaris/native/sun/tools/attach/BsdVirtualMachine.c中，其关键操作就一个return socket(PF_UNIX, SOCK_STREAM, 0) 客户端 socket 连接。

21行：这里也是一个本地方法，调用了 connect(s,p),这里的 p 就是 attach 时产生的/tmp/.java_pid${pid}的 socket 文件路径，这样，客户端就和目标 JVM 连接上了，该方法同样是一个 native 方法，可以通过查看BsdVirtualMachine.c的源码来进行查看，如下，重点在16行使用 socket 文件路径作为连接地址 和 18 行与目标 JVM 端启动的 socket server 建立连接；

```
 1 /*
 2  * Class:     sun_tools_attach_BsdVirtualMachine
 3  * Method:    connect
 4  * Signature: (ILjava/lang/String;)I
 5  */
 6 JNIEXPORT void JNICALL Java_sun_tools_attach_BsdVirtualMachine_connect
 7   (JNIEnv *env, jclass cls, jint fd, jstring path)
 8 {
 9     jboolean isCopy;
10     const char* p = GetStringPlatformChars(env, path, &isCopy);
11     if (p != NULL) {
12         struct sockaddr_un addr;
13         int err = 0;
14 
15         addr.sun_family = AF_UNIX;
16         strcpy(addr.sun_path, p);
17 
18         if (connect(fd, (struct sockaddr*)&addr, sizeof(addr)) == -1) {
19             err = errno;
20         }
21 
22         if (isCopy) {
23             JNU_ReleaseStringPlatformChars(env, path, p);
24         }
25 
26         /*
27          * If the connect failed then we throw the appropriate exception
28          * here (can't throw it before releasing the string as can't call
29          * JNI with pending exception)
30          */
31         if (err != 0) {
32             if (err == ENOENT) {
33                 JNU_ThrowByName(env, "java/io/FileNotFoundException", NULL);
34             } else {
35                 char* msg = strdup(strerror(err));
36                 JNU_ThrowIOException(env, msg);
37                 if (msg != NULL) {
38                     free(msg);
39                 }
40             }
41         }
42     }
```

31~44行：想 Target JVM 端发送命令 threaddump、以及可能存在的相关参数，如-l；这里的 writeString 同样是一个本地方法，涉及到的底层操作就是一个 C 语言库的 write 操作，感兴趣的可以自己看源码，不再赘述；

48~83行：这里就是对当前 socket 连接，构建一个 SocketInputStream 对象，并等待Target JVM 端数据完全返回，最后将这个 InputStream 对象作为方法返回参数返回。

## 总结

本文结合 Attach 的原理和使用案例（jstack -l），对 Attach 的各个方面都进行了深入的分析和总结，希望能对有需要的同学有所帮助。当然，以上均为本人个人所学，所以难免会有错误和疏忽的地方，如果您发现了，还麻烦指出。
