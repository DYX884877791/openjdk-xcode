---
created: 2021-12-07T02:17:03 (UTC +08:00)
tags: [HotSpot VM,Java 虚拟机（JVM）]
source: https://zhuanlan.zhihu.com/p/379257556
author: 
---

# openjdk源码学习--jvm的启动流程 - 知乎
---
## 前言

学习openjdk的源码时，第一步我们需要找到入口，也就是main函数。我们以openjdk11的源码进行解析，源码量过大，想快速找到jvm的启动入口，还是得借助gdb。

可参考

我们采用slowdebug版本。

```
root@ce8abeba371f:/path/to/dev/jdk11u/build/linux-riscv32-normal-core-slowdebug/jdk/bin# /path/to/riscv32/bin/riscv32-unknown-linux-gnu-gdb --args ./java -version
GNU gdb (GDB) 10.1
Copyright (C) 2020 Free Software Foundation, Inc.
License GPLv3+: GNU GPL version 3 or later <http://gnu.org/licenses/gpl.html>
This is free software: you are free to change and redistribute it.
There is NO WARRANTY, to the extent permitted by law.
Type "show copying" and "show warranty" for details.
This GDB was configured as "--host=x86_64-pc-linux-gnu --target=riscv32-unknown-linux-gnu".
Type "show configuration" for configuration details.
For bug reporting instructions, please see:
<https://www.gnu.org/software/gdb/bugs/>.
Find the GDB manual and other documentation resources online at:
    <http://www.gnu.org/software/gdb/documentation/>.

For help, type "help".
Type "apropos word" to search for commands related to "word"...
Reading symbols from ./java...
(gdb) target remote localhost:33334
Remote debugging using localhost:33334
Reading symbols from /path/to/riscv32/sysroot/lib/ld-linux-riscv32-ilp32d.so.1...
0x3ffe6a00 in _start () from /path/to/riscv32/sysroot/lib/ld-linux-riscv32-ilp32d.so.1
(gdb) set solib-search-path /path/to/dev/jdk11u/build/linux-riscv32-normal-core-slowdebug/jdk/lib:/path/to/dev/jdk11u/build/linux-riscv32-normal-core-slowdebug/jdk/lib/jli:/path/to/dev/jdk11u/build/linux-riscv32-normal-core-slowdebug/jdk/lib/server
Reading symbols from /path/to/riscv32/sysroot/lib/ld-linux-riscv32-ilp32d.so.1...
(gdb) b main
Breakpoint 1 at 0x10852: file /path/to/dev/jdk11u/src/java.base/share/native/launcher/main.c, line 98.
(gdb) 
```

可以清楚的看到main函数所在的目录以及行数。

main函数内容

```
#ifdef JAVAW

char **__initenv;

int WINAPI
WinMain(HINSTANCE inst, HINSTANCE previnst, LPSTR cmdline, int cmdshow)
{
    int margc;
    char** margv;
    int jargc;
    char** jargv;
    const jboolean const_javaw = JNI_TRUE;

    __initenv = _environ;

#else /* JAVAW */
JNIEXPORT int
main(int argc, char **argv)
{
    int margc;
    char** margv;
    int jargc;
    char** jargv;
    const jboolean const_javaw = JNI_FALSE;
#endif /* JAVAW */
    {
        int i, main_jargc, extra_jargc;
        JLI_List list;

        main_jargc = (sizeof(const_jargs) / sizeof(char *)) > 1
            ? sizeof(const_jargs) / sizeof(char *)
            : 0; // ignore the null terminator index

        extra_jargc = (sizeof(const_extra_jargs) / sizeof(char *)) > 1
            ? sizeof(const_extra_jargs) / sizeof(char *)
            : 0; // ignore the null terminator index

        if (main_jargc > 0 && extra_jargc > 0) { // combine extra java args
            jargc = main_jargc + extra_jargc;
            list = JLI_List_new(jargc + 1);

            for (i = 0 ; i < extra_jargc; i++) {
                JLI_List_add(list, JLI_StringDup(const_extra_jargs[i]));
            }

            for (i = 0 ; i < main_jargc ; i++) {
                JLI_List_add(list, JLI_StringDup(const_jargs[i]));
            }

            // terminate the list
            JLI_List_add(list, NULL);
            jargv = list->elements;
         } else if (extra_jargc > 0) { // should never happen
            fprintf(stderr, "EXTRA_JAVA_ARGS defined without JAVA_ARGS");
            abort();
         } else { // no extra args, business as usual
            jargc = main_jargc;
            jargv = (char **) const_jargs;
         }
    }

    JLI_InitArgProcessing(jargc > 0, const_disable_argfile);

#ifdef _WIN32
    {
        int i = 0;
        if (getenv(JLDEBUG_ENV_ENTRY) != NULL) {
            printf("Windows original main args:\n");
            for (i = 0 ; i < __argc ; i++) {
                printf("wwwd_args[%d] = %s\n", i, __argv[i]);
            }
        }
    }
    JLI_CmdToArgs(GetCommandLine());
    margc = JLI_GetStdArgc();
    // add one more to mark the end
    margv = (char **)JLI_MemAlloc((margc + 1) * (sizeof(char *)));
    {
        int i = 0;
        StdArg *stdargs = JLI_GetStdArgs();
        for (i = 0 ; i < margc ; i++) {
            margv[i] = stdargs[i].arg;
        }
        margv[i] = NULL;
    }
#else /* *NIXES */
    {
        // accommodate the NULL at the end
        JLI_List args = JLI_List_new(argc + 1);
        int i = 0;

        // Add first arg, which is the app name
        JLI_List_add(args, JLI_StringDup(argv[0]));
        // Append JDK_JAVA_OPTIONS
        if (JLI_AddArgsFromEnvVar(args, JDK_JAVA_OPTIONS)) {
            // JLI_SetTraceLauncher is not called yet
            // Show _JAVA_OPTIONS content along with JDK_JAVA_OPTIONS to aid diagnosis
            if (getenv(JLDEBUG_ENV_ENTRY)) {
                char *tmp = getenv("_JAVA_OPTIONS");
                if (NULL != tmp) {
                    JLI_ReportMessage(ARG_INFO_ENVVAR, "_JAVA_OPTIONS", tmp);
                }
            }
        }
        // Iterate the rest of command line
        for (i = 1; i < argc; i++) {
            JLI_List argsInFile = JLI_PreprocessArg(argv[i], JNI_TRUE);
            if (NULL == argsInFile) {
                JLI_List_add(args, JLI_StringDup(argv[i]));
            } else {
                int cnt, idx;
                cnt = argsInFile->size;
                for (idx = 0; idx < cnt; idx++) {
                    JLI_List_add(args, argsInFile->elements[idx]);
                }
                // Shallow free, we reuse the string to avoid copy
                JLI_MemFree(argsInFile->elements);
                JLI_MemFree(argsInFile);
            }
        }
        margc = args->size;
        // add the NULL pointer at argv[argc]
        JLI_List_add(args, NULL);
        margv = args->elements;
    }
#endif /* WIN32 */
    return JLI_Launch(margc, margv,
                   jargc, (const char**) jargv,
                   0, NULL,
                   VERSION_STRING,
                   DOT_VERSION,
                   (const_progname != NULL) ? const_progname : *margv,
                   (const_launcher != NULL) ? const_launcher : *margv,
                   jargc > 0,
                   const_cpwildcard, const_javaw, 0);
}
```

接下来一个个的进行分析

```
#ifdef JAVAW

省略的windows平台相关的代码

#else /* JAVAW */

int main(int argc, char **argv)

{
    int margc;

    char** margv;

    const jboolean const_javaw = JNI_FALSE;

#endif /* JAVAW */

#ifdef _WIN32

    省略的windows平台相关的代码

#else /* *NIXES */

    margc = argc;

    margv = argv;

#endif /* WIN32 */
这部分内容大多都是涉及windows平台内容，都被省略了
```

我们主要关注

```
    return JLI_Launch(margc, margv,
                   jargc, (const char**) jargv,
                   0, NULL,
                   VERSION_STRING,
                   DOT_VERSION,
                   (const_progname != NULL) ? const_progname : *margv,
                   (const_launcher != NULL) ? const_launcher : *margv,
                   jargc > 0,
                   const_cpwildcard, const_javaw, 0);
}
由launch可知，这部分涉及到真正的启动
```

我们进入内部看看

![](https://pic3.zhimg.com/v2-a4211a03afe86db7bf990d8399ec0b7a_b.png)

这些相关参数也可以与下面函数内容进行比对

```
JLI_Launch (argc=2, argv=0x131b0, jargc=0, jargv=0x12020 <const_jargs>, appclassc=0, appclassv=0x0, fullversion=0x10c70 "11.0.9-internal+0-adhoc..jdk11u", dotversion=0x10c6c "0.0",
    pname=0x10bd0 "java", lname=0x10be0 "openjdk", javaargs=0 '\000', cpwildcard=1 '\001', javaw=0 '\000', ergo=0)
```

这里你可以看到对应的版本信息

```
JNIEXPORT int JNICALL
JLI_Launch(int argc, char ** argv,              /* main argc, argv */
        int jargc, const char** jargv,          /* java args */
        int appclassc, const char** appclassv,  /* app classpath */
        const char* fullversion,                /* full version defined */
        const char* dotversion,                 /* UNUSED dot version defined */
        const char* pname,                      /* program name */
        const char* lname,                      /* launcher name */
        jboolean javaargs,                      /* JAVA_ARGS */
        jboolean cpwildcard,                    /* classpath wildcard*/
        jboolean javaw,                         /* windows-only javaw */
        jint ergo                               /* unused */
)
{
    int mode = LM_UNKNOWN;
    char *what = NULL;
    char *main_class = NULL;
    int ret;
    InvocationFunctions ifn;
    jlong start = 0, end = 0;
    char jvmpath[MAXPATHLEN];//jvm的路径
    char jrepath[MAXPATHLEN];//jre的路径
    char jvmcfg[MAXPATHLEN];//jvm配置路径

    _fVersion = fullversion;
    _launcher_name = lname;
    _program_name = pname;
    _is_java_args = javaargs;
    _wc_enabled = cpwildcard;


    //会根据_JAVA_LAUNCHER_DEBUG环境变量是否设置来设置是否打印debug信息
    InitLauncher(javaw);
    DumpState();//根据是否设置debug来选择输出一些配置信息
    if (JLI_IsTraceLauncher()) {//同样如果设置了debug信息就输出命令行参数的输出
        int i;
        printf("Java args:\n");
        for (i = 0; i < jargc ; i++) {
            printf("jargv[%d] = %s\n", i, jargv[i]);
        }
        printf("Command line args:\n");
        for (i = 0; i < argc ; i++) {
            printf("argv[%d] = %s\n", i, argv[i]);
        }
        AddOption("-Dsun.java.launcher.diag=true", NULL);
    }

    /*
     * SelectVersion() has several responsibilities:
     *
     *  1) Disallow specification of another JRE.  With 1.9, another
     *     version of the JRE cannot be invoked.
     *  2) Allow for a JRE version to invoke JDK 1.9 or later.  Since
     *     all mJRE directives have been stripped from the request but
     *     the pre 1.9 JRE [ 1.6 thru 1.8 ], it is as if 1.9+ has been
     *     invoked from the command line.
     */
    //选择jre的版本,这个函数实现的功能比较简单，就是选择正确的jre版本来作为即将运行java程序的版本。选择的方式，如果环境变量设置了_JAVA_VERSION_SET，那么代表已经选择了jre的版本，不再进行选择；否则，根据运行时给定的参数来搜索不同的目录选择，例如指定版本和限制了搜索目录等，也可能执行的是一个jar文件，所以需要解析manifest文件来获取相关信息，对应Manifest文件的数据结构，通过函数ParseManifest解析

    SelectVersion(argc, argv, &main_class);
     //创建环境的执行变量
     //这个函数主要创建执行的一些环境，这个环境主要是指jvm的环境，例如需要确定数据模型，是32位还是64位以及jvm本身的一些配置在jvm.cfg文件中读取和解析。里面有一个重要的函数就是专门解析jvm.cfg的
    CreateExecutionEnvironment(&argc, &argv,
                               jrepath, sizeof(jrepath),
                               jvmpath, sizeof(jvmpath),
                               jvmcfg,  sizeof(jvmcfg));

    if (!IsJavaArgs()) {
        SetJvmEnvironment(argc,argv);
    }

    ifn.CreateJavaVM = 0;
    ifn.GetDefaultJavaVMInitArgs = 0;

    if (JLI_IsTraceLauncher()) {
        start = CounterGet();
    }

    if (!LoadJavaVM(jvmpath, &ifn)) {
        return(6);
    }

    if (JLI_IsTraceLauncher()) {
        end   = CounterGet();
    }

    JLI_TraceLauncher("%ld micro seconds to LoadJavaVM\n",
             (long)(jint)Counter2Micros(end-start));

    ++argv;
    --argc;

    if (IsJavaArgs()) {
        /* Preprocess wrapper arguments */
        TranslateApplicationArgs(jargc, jargv, &argc, &argv);
        if (!AddApplicationOptions(appclassc, appclassv)) {
            return(1);
        }
    } else {
        /* Set default CLASSPATH */
        char* cpath = getenv("CLASSPATH");
        if (cpath != NULL) {
            SetClassPath(cpath);
        }
    }

    /* Parse command line options; if the return value of
     * ParseArguments is false, the program should exit.
     */
    if (!ParseArguments(&argc, &argv, &mode, &what, &ret, jrepath)) {
        return(ret);
    }

    /* Override class path if -jar flag was specified */
    if (mode == LM_JAR) {
        SetClassPath(what);     /* Override class path */
    }

    /* set the -Dsun.java.command pseudo property */
    SetJavaCommandLineProp(what, argc, argv);

    /* Set the -Dsun.java.launcher pseudo property */
    SetJavaLauncherProp();

    /* set the -Dsun.java.launcher.* platform properties */
    SetJavaLauncherPlatformProps();

    return JVMInit(&ifn, threadStackSize, argc, argv, mode, what, ret);
}
```

先从总体看一下它的主要流程

**其主要流程如下：**

1、SelectVersion，从jar包中manifest文件或者命令行读取用户使用的JDK版本，判断当前版本是否合适。

2、CreateExecutionEnvironment，设置执行环境参数

3、LoadJavaVM，加载libjvm动态链接库，从中获取JNI\_CreateJavaVM，JNI\_GetDefaultJavaVMInitArgs和JNI\_GetCreatedJavaVMs三个函数的实现，其中JNI\_CreateJavaVM是JVM初始化的核心入口，具体实现在hotspot目录中

4、ParseArguments 解析命令行参数，如-version，-help等参数在该方法中解析的

5、SetJavaCommandLineProp 解析形如-Dsun.java.command=的命令行参数

6、SetJavaLauncherPlatformProps 解析形如-Dsun.java.launcher.\*的命令行参数

其实（4，5，6）都是解析命令行参数，下面只介绍了4

7、JVMInit，通过JVMInit->ContinueInNewThread->ContinueInNewThread0->pthread\_create创建了一个新的线程，执行JavaMain函数，主线程pthread\_join该线程，在JavaMain函数中完成虚拟机的初始化和启动。

再一个个流程进行仔细分析

**1)SelectVersion**：选择jre的版本,这个函数实现的功能比较简单，就是选择正确的jre版本来作为即将运行java程序的版本。选择的方式，如果环境变量设置了\_JAVA\_VERSION\_SET，那么代表已经选择了jre的版本，不再进行选择；否则，根据运行时给定的参数来搜索不同的目录选择，例如指定版本和限制了搜索目录等，也可能执行的是一个jar文件，所以需要解析manifest文件来获取相关信息，对应Manifest文件的数据结构如下，可以看到对应注释都是涉及到jre版本的相关信息。

```
typedef struct manifest_info {  /* Interesting fields from the Manifest */
    char        *manifest_version;      /* Manifest-Version string */
    char        *main_class;            /* Main-Class entry */
    char        *jre_version;           /* Appropriate J2SE release spec */
    char        jre_restrict_search;    /* Restricted JRE search */
    char        *splashscreen_image_file_name; /* splashscreen image file */
} manifest_info;
```

通过函数ParseManifest解析，最终会解析出一个真正需要的jre版本并且判断当前执行本java程序的jre版本是不是和这个版本一样，如果不一样调用linux的execv函数终止当前进出并且使用新的jre版本重新运行这个java程序，但是进程ID不会改变。

**2)CreateExecutionEnvironment**，这个函数主要创建执行的一些环境，这个环境主要是指jvm的环境，例如需要确定数据模型，是32位还是64位以及jvm本身的一些配置在jvm.cfg文件中读取和解析。

```
CreateExecutionEnvironment(int *pargc, char ***pargv,
                           char *jrepath, jint so_jrepath,
                           char *jvmpath, jint so_jvmpath,
                           char *jvmcfg,  jint so_jvmcfg) {
//入参都是对应的路径以及配置
    char *jvmtype;
    int i = 0;
    char** argv = *pargv;
    /* Find out where the JRE is that we will be using. */
    if (!GetJREPath(jrepath, so_jrepath)) {
        JLI_ReportErrorMessage(JRE_ERROR1);
        exit(2);
    }
    JLI_Snprintf(jvmcfg, so_jvmcfg, "%s%slib%sjvm.cfg",
        jrepath, FILESEP, FILESEP)
    /* Find the specified JVM type ，可以进去找到对应的类型的结构体*/
    if (ReadKnownVMs(jvmcfg, JNI_FALSE) < 1) {
        JLI_ReportErrorMessage(CFG_ERROR7);
        exit(1);
    }

    jvmtype = CheckJvmType(pargc, pargv, JNI_FALSE);
    if (JLI_StrCmp(jvmtype, "ERROR") == 0) {
        JLI_ReportErrorMessage(CFG_ERROR9);
        exit(4);
    }
    jvmpath[0] = '\0';
    if (!GetJVMPath(jrepath, jvmtype, jvmpath, so_jvmpath)) {
        JLI_ReportErrorMessage(CFG_ERROR8, jvmtype, jvmpath);
        exit(4);
    }
    /* If we got here, jvmpath has been correctly initialized. */
    /* Check if we need preload AWT */
#ifdef ENABLE_AWT_PRELOAD
    argv = *pargv;
    for (i = 0; i < *pargc ; i++) {
        /* Tests the "turn on" parameter only if not set yet. */
        if (awtPreloadD3D < 0) {
            if (GetBoolParamValue(PARAM_PRELOAD_D3D, argv[i]) == 1) {
                awtPreloadD3D = 1;
            }
        }
        /* Test parameters which can disable preloading if not already disabled. */
        if (awtPreloadD3D != 0) {
            if (GetBoolParamValue(PARAM_NODDRAW, argv[i]) == 1
                || GetBoolParamValue(PARAM_D3D, argv[i]) == 0
                || GetBoolParamValue(PARAM_OPENGL, argv[i]) == 1)
            {
                awtPreloadD3D = 0;
                /* no need to test the rest of the parameters */
                break;
            }
        }
    }
#endif /* ENABLE_AWT_PRELOAD */
}
```

从函数主体内容看，主要涉及到找的jre是否是需要的jre（通过jrepath），找到对应的jvm的类型，里面有一个重要的函数就是专门解析[jvm.cfg](https://www.zhihu.com/search?q=jvm.cfg&search_source=Entity&hybrid_search_source=Entity&hybrid_search_extra=%7B%22sourceType%22%3A%22article%22%2C%22sourceId%22%3A379257556%7D)的，如下：jint ReadKnownVMs(const char \*jvmCfgName, jboolean speculative)。这个函数解析jvm.cfg文件来确定jvm的类型，jvm的类型有如下几种（是一个枚举定义）：

```
/* Values for vmdesc.flag */
enum vmdesc_flag {
    VM_UNKNOWN = -1,
    VM_KNOWN,
    VM_ALIASED_TO,
    VM_WARN,
    VM_ERROR,
    VM_IF_SERVER_CLASS,
    VM_IGNORE
};
```

然后还有一个结构体专门描述jvm的信息，如下：

```
struct vmdesc {
    char *name;//名字
    int flag;//上面说的枚举定义类型
    char *alias;//别名
    char *server_class;//服务器类
};
```

总结一下：这个函数主要是确定jvm的信息并且做一个初始化相关信息，为后面的jvm执行做准备。

**3）LoadJavaVM**：动态加载jvm.so这个共享库，并把jvm.so中的相关函数导出并且初始化，例如JNI\_CreateJavaVM函数。后期启动真正的java虚拟就是通过这里面加载的函数,**这里要注意的是，这个函数在unix以及windows平台下都有，使用ide进行跳转时请注意选择。当然使用gdb是最好的选择。可以看到jvmpath**

![](https://pic3.zhimg.com/v2-e7cbf5af12fa0ac0ef6d4d1e76c6c692_b.png)

```
LoadJavaVM (jvmpath=0x407fd51c "/path/to/dev/jdk11u/build/linux-riscv32-normal-core-slowdebug/jdk/lib/server/libjvm.so", ifn=0x407fd510) at /path/to/dev/jdk11u/src/java.base/unix/native/libjli/java_md_solinux.c:546
546     {
(gdb) n
```

其中部分主要代码：

```
    ifn->CreateJavaVM = (CreateJavaVM_t)
        dlsym(libjvm, "JNI_CreateJavaVM");
    if (ifn->CreateJavaVM == NULL) {
        JLI_ReportErrorMessage(DLL_ERROR2, jvmpath, dlerror());
        return JNI_FALSE;
    }
    ifn->GetDefaultJavaVMInitArgs = (GetDefaultJavaVMInitArgs_t)
        dlsym(libjvm, "JNI_GetDefaultJavaVMInitArgs");
    if (ifn->GetDefaultJavaVMInitArgs == NULL) {
        JLI_ReportErrorMessage(DLL_ERROR2, jvmpath, dlerror());
        return JNI_FALSE;
    }
    ifn->GetCreatedJavaVMs = (GetCreatedJavaVMs_t)
        dlsym(libjvm, "JNI_GetCreatedJavaVMs");
    if (ifn->GetCreatedJavaVMs == NULL) {
        JLI_ReportErrorMessage(DLL_ERROR2, jvmpath, dlerror());
        return JNI_FALSE;
```

这个ifn的具体信息如下，主要就是初始化jvm的信息：

```
typedef struct {
    CreateJavaVM_t CreateJavaVM;
    GetDefaultJavaVMInitArgs_t GetDefaultJavaVMInitArgs;
    GetCreatedJavaVMs_t GetCreatedJavaVMs;
} InvocationFunctions;
```

总结一下：总结：这个函数就是初始化jvm相关的初始化函数和入后函数，后面就是调用这里的JNI\_CreateJavaVM函数真正的开始启动一个jvm的，这个函数会做很多的初始化工作，基本上一个完整的jvm信息在这个函数里面都能够看到。

**4）ParseArguments：**解析命令行参数，就不多解析了，不同的命令行参数具体使用到来详细介绍其作用。

可以看到如-version，-help等参数在该方法中解析的。

```
 } else if (!has_arg && (JLI_StrCmp(arg, "--module-path") == 0 ||
                                JLI_StrCmp(arg, "-p") == 0 ||
                                JLI_StrCmp(arg, "--upgrade-module-path") == 0)) {
            REPORT_ERROR (has_arg, ARG_ERROR4, arg);

        } else if (!has_arg && (IsModuleOption(arg) || IsLongFormModuleOption(arg))) {
            REPORT_ERROR (has_arg, ARG_ERROR6, arg);
/*
 * The following cases will cause the argument parsing to stop
 */
        } else if (JLI_StrCmp(arg, "-help") == 0 ||
                   JLI_StrCmp(arg, "-h") == 0 ||
                   JLI_StrCmp(arg, "-?") == 0) {
            printUsage = JNI_TRUE;
            return JNI_TRUE;
        } else if (JLI_StrCmp(arg, "--help") == 0) {
            printUsage = JNI_TRUE;
            printTo = USE_STDOUT;
            return JNI_TRUE;
        } else if (JLI_StrCmp(arg, "-version") == 0) {
            printVersion = JNI_TRUE;
            return JNI_TRUE;
        } else if (JLI_StrCmp(arg, "--version") == 0) {
            printVersion = JNI_TRUE;
            printTo = USE_STDOUT;
            return JNI_TRUE;
        } else if (JLI_StrCmp(arg, "-showversion") == 0) {
            showVersion = JNI_TRUE;
        } else if (JLI_StrCmp(arg, "--show-version") == 0) {
            showVersion = JNI_TRUE;
            printTo = USE_STDOUT;
        } else if (JLI_StrCmp(arg, "--dry-run") == 0) {
            dryRun = JNI_TRUE;
        } else if (JLI_StrCmp(arg, "-X") == 0) {
            printXUsage = JNI_TRUE;
            return JNI_TRUE;
        } else if (JLI_StrCmp(arg, "--help-extra") == 0) {
            printXUsage = JNI_TRUE;
            printTo = USE_STDOUT;
```

**5）JVMInit：**这是启动流程最后执行的一个函数，如果这个函数返回了那么这个java启动就结束了，所有这个函数最终会以某种形式进行执行下去。

```
int
JVMInit(InvocationFunctions* ifn, jlong threadStackSize,
        int argc, char **argv,
        int mode, char *what, int ret)
{
    ShowSplashScreen();
    return ContinueInNewThread(ifn, threadStackSize, argc, argv, mode, what, ret);
}
```

具体先看看这个函数的主要流程，如下：

**JVMInit**\->**ContinueInNewThread**\->**ContinueInNewThread0**\->(可能是新线程的入口函数进行执行，新线程创建失败就在原来的线程继续支持这个函数)**JavaMain**\->**InitializeJVM**(初始化jvm，这个函数调用jvm.so里面导出的CreateJavaVM函数创建jvm了，JNI\_CreateJavaVM这个函数很复杂)->**LoadMainClass**（这个函数就是找到我们真正java程序的入口类，就是我们开发应用程序带有main函数的类）->**GetApplicationClass**\->后面就是调用环境类的工具获得main函数并且传递参数调用main函数，查找main和调用main函数都是使用类似java里面支持的反射实现的。

至此，java这个启动命令全部流程解析完毕

后续将进一步细致的介绍启动过程，包括最后查找入口类以及查找main入口函数的具体实现以及5）中涉及到的具体初始化过程。

本文的源码是来自于

其中jdk的编译指导来自于
