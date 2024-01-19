---
source: https://zhuanlan.zhihu.com/p/563479524
---
> 简单分析一下java命令的动态依赖库. 这样可以对其构建过程做一个反向推导. 这篇文章内容干货较少, 如果您已经了解动态链或者HotSpot相关的内容. 可以直接略过, 如果想大概了解下结论. 直接到文章结尾的小结.

我们可以使用`ldd`命令查看一下当前的java命令的动态链接库有哪些. java命令的目录为:

```
~/sourcecode/jdk8u/build/linux-x86_64-normal-server-slowdebug/jdk/bin
```

> 使用命令: `ldd java`

![](https://pic3.zhimg.com/v2-ddf8ed14b9357bf1dd2f6c2ce5094dd6_b.png)

依赖的动态库列表

```
linux-vdso.so.1 =>  (0x00007ffcc6c94000)
libjli.so => /home/lijianhong/sourcecode/jdk8u/build/linux-x86_64-normal-server-slowdebug/jdk/bin/./../lib/amd64/jli/libjli.so (0x00007fc504dec000)
libc.so.6 => /lib/x86_64-linux-gnu/libc.so.6 (0x00007fc504a22000)
libdl.so.2 => /lib/x86_64-linux-gnu/libdl.so.2 (0x00007fc50481e000)
libpthread.so.0 => /lib/x86_64-linux-gnu/libpthread.so.0 (0x00007fc504601000)
/lib64/ld-linux-x86-64.so.2 (0x00007fc505209000)
```

### linux-vdso.so.1

这个是linux的一个虚拟的.<sup data-text="linux-vdso.so.1介绍" data-url="https://blog.csdn.net/wang_xya/article/details/43985241" data-numero="1" data-draft-node="inline" data-draft-type="reference" data-tooltip="linux-vdso.so.1介绍 https://blog.csdn.net/wang_xya/article/details/43985241" data-tooltip-preset="white" data-tooltip-classname="ztext-referene-tooltip"><a id="ref_1_0" href="https://zhuanlan.zhihu.com/p/563479524#ref_1" data-reference-link="true" aria-labelledby="ref_1">[1]</a></sup>这里不展开说, 具体可以查看参考资料.

### ld-linux-x86-64.so.2

这个是64位系统下的动态链接加载器,略过不表.

### libdl.so.2

这个是为了手工加载动态库所依赖的系统组件. 同略.

### libc.so.6

glibc的动态版本,路径为: `/lib/x86_64-linux-gnu/libc.so.6` , 常规依赖. 也略过不表. 主要包含C语言的库函数，如memcpy memset 等常用函数的动态库。每个不同glibc的版本不同。

### libpthread.so.0

libpthread库是glibc的多线程库，主要包含多线程变成时 pthread_xxx 开头的函数<sup data-text="libpthread.so.0有什么作用" data-url="https://bbs.pediy.com/thread-264491.htm" data-numero="2" data-draft-node="inline" data-draft-type="reference" data-tooltip="libpthread.so.0有什么作用 https://bbs.pediy.com/thread-264491.htm" data-tooltip-preset="white" data-tooltip-classname="ztext-referene-tooltip"><a id="ref_2_0" href="https://zhuanlan.zhihu.com/p/563479524#ref_2" data-reference-link="true" aria-labelledby="ref_2">[2]</a></sup>。

我们可以使用`nm`命令可以查看导出函数列表:

![](https://pic4.zhimg.com/v2-61d38b7a97b2d2903bd7dca7b55acf83_b.jpg)

libpthread 导出函数列表示意

### libjli.so

最后的依赖是: libjli.so , 这个是JDK的核心lib库.其位置位于: `lib/amd64/jli/libjli.so`

```
/build/linux-x86_64-normal-server-slowdebug/jdk/bin/./../lib/amd64/jli/libjli.so (0x00007ffa3eeae000)
```

同样的我们也可以看一下这个库里面导出了什么符号 (函数)

```
nm /home/lijianhong/sourcecode/jdk8u/build/linux-x86_64-normal-server-slowdebug/jdk/bin/./../lib/amd64/jli/libjli.so
0000000000010997 t acceptable_element
0000000000010888 t acceptable_simple_element
                 U access@@GLIBC_2.2.5
000000000000a557 t AddApplicationOptions
000000000000891c t AddOption
0000000000011c93 t adler32
0000000000011e20 t adler32_combine
0000000000011cbb t adler32_combine_
0000000000011e47 t adler32_combine64
0000000000011716 t adler32_z
000000000000cf44 t borrowed_unsetenv
000000000021a3f8 b __bss_start
0000000000007fc1 t CheckJvmType
000000000000c685 t CheckSanity
                 U closedir@@GLIBC_2.2.5
                 U close@@GLIBC_2.2.5
000000000021a3f8 b completed.7594
0000000000010565 t comp_string
000000000000f08e t compute_cen
000000000000d210 t ContainsLibJVM
000000000000bbbd t ContinueInNewThread
000000000000e540 t ContinueInNewThread0
000000000000e6ec t CounterGet
0000000000011ede t crc32
00000000000124f8 t crc32_big
0000000000012da9 t crc32_combine
0000000000012c2a t crc32_combine_
0000000000012dd0 t crc32_combine64
0000000000011f06 t crc32_little
0000000000011e7b t crc32_z
00000000000154e0 r crc_table
000000000000d137 t CreateApplicationArgs
000000000000d5ea t CreateExecutionEnvironment
                 U __ctype_b_loc@@GLIBC_2.3
                 w __cxa_finalize@@GLIBC_2.2.5
0000000000013d40 r dbase.3800
00000000000025c0 t deregister_tm_clones
0000000000013d80 r dext.3801
0000000000013bc0 r distfix.3894
                 U dlclose@@GLIBC_2.2.5
                 U dlerror@@GLIBC_2.2.5
                 U dlopen@@GLIBC_2.2.5
                 U dlsym@@GLIBC_2.2.5
0000000000002650 t __do_global_dtors_aux
0000000000219d10 t __do_global_dtors_aux_fini_array_entry
000000000001035f t DoSplashClose
000000000001044a t DoSplashGetScaledImageName
0000000000010322 t DoSplashInit
00000000000102cf t DoSplashLoadFile
0000000000010274 t DoSplashLoadMemory
000000000001039c t DoSplashSetFileJarName
00000000000103f6 t DoSplashSetScaleFactor
000000000021a2a0 d __dso_handle
000000000000bca0 t DumpState
000000000021a430 b _dVersion
0000000000219d70 d _DYNAMIC
000000000021a3f8 d _edata
000000000021a500 b _end
                 U __environ@@GLIBC_2.2.5
                 U environ@@GLIBC_2.2.5
0000000000010e4c t equal
000000000021a43c b _ergo_policy
                 U __errno_location@@GLIBC_2.2.5
000000000000cbd5 t ExecJRE
000000000021a4a0 b execname
                 U execve@@GLIBC_2.2.5
                 U execv@@GLIBC_2.2.5
0000000000010d1e t exists
                 U exit@@GLIBC_2.2.5
                 U fclose@@GLIBC_2.2.5
                 U fflush@@GLIBC_2.2.5
                 U fgets@@GLIBC_2.2.5
0000000000010fa7 t FileList_add
0000000000010ffb t FileList_addSubstring
0000000000010f3d t FileList_ensureCapacity
00000000000114c7 t FileList_expandWildcards
0000000000010ec8 t FileList_free
000000000001108f t FileList_join
0000000000010e79 t FileList_new
0000000000011193 t FileList_split
000000000021a498 b findBootClass
000000000000d0a9 t FindBootStrapClass
000000000000ed8d t find_end
000000000000ecbf t find_end64
000000000000c22b t FindExecName
000000000000f6cb t find_file
0000000000012ea0 t _fini
00000000000039bf t fixedtables
                 U fopen@@GLIBC_2.2.5
0000000000014c50 r format.8213
                 U fprintf@@GLIBC_2.2.5
                 U fputc@@GLIBC_2.2.5
0000000000002690 t frame_dummy
0000000000219d08 t __frame_dummy_init_array_entry
0000000000019300 r __FRAME_END__
                 U free@@GLIBC_2.2.5
000000000000b952 t FreeKnownVMs
000000000021a428 b _fVersion
000000000000a1a0 t GetApplicationClass
000000000000bf2c t GetApplicationHome
000000000000d16f t GetArchPath
00000000000027b4 t get_cpuid
0000000000011e6e t get_crc_table
                 U getcwd@@GLIBC_2.2.5
000000000000bb63 t GetDotVersion
                 U getegid@@GLIBC_2.2.5
                 U getenv@@GLIBC_2.2.5
000000000000bb97 t GetErgoPolicy
                 U geteuid@@GLIBC_2.2.5
000000000000d162 t GetExecName
000000000000bb70 t GetFullVersion
                 U getgid@@GLIBC_2.2.5
000000000000df42 t GetJREPath
000000000000de1b t GetJVMPath
0000000000009c67 t GetLauncherHelperClass
000000000000bb8a t GetLauncherName
                 U getpid@@GLIBC_2.2.5
000000000000bb7d t GetProgramName
                 U gettimeofday@@GLIBC_2.2.5
                 U getuid@@GLIBC_2.2.5
0000000000012bc0 t gf2_matrix_square
0000000000012b7d t gf2_matrix_times
000000000021a000 d _GLOBAL_OFFSET_TABLE_
                 w __gmon_start__
000000000001755c r __GNU_EH_FRAME_HDR
000000000000b83b t GrowKnownVMs
000000000000ebae t haveZIP64
000000000021a488 b helperClass
000000000021a4a8 b hSplashLib
0000000000002876 t hyperthreading_support
0000000000003c0c t inflate
0000000000006a2c t inflateCodesUsed
00000000000066ab t inflateCopy
0000000000013c80 r inflate_copyright
00000000000060e3 t inflateEnd
0000000000002be6 t inflate_fast
000000000000e939 t inflate_file
0000000000006172 t inflateGetDictionary
000000000000634f t inflateGetHeader
00000000000038c3 t inflateInit_
000000000000377f t inflateInit2_
000000000000699e t inflateMark
00000000000038f0 t inflatePrime
0000000000003642 t inflateReset
00000000000036a0 t inflateReset2
0000000000003516 t inflateResetKeep
000000000000624f t inflateSetDictionary
000000000000349c t inflateStateCheck
0000000000006447 t inflateSync
0000000000006654 t inflateSyncPoint
0000000000006a81 t inflate_table
00000000000068f0 t inflateUndermine
0000000000006937 t inflateValidate
0000000000002080 t _init
000000000021a480 b initialHeapSize
0000000000009b21 t InitializeJVM
000000000000d08f t InitLauncher
0000000000011282 t isJarFileName
000000000021a420 b _is_java_args
000000000000bba3 t IsJavaArgs
00000000000104b1 t isjavaint
000000000000d084 t IsJavaw
0000000000011454 t isWildcard
000000000000bbb0 t IsWildCardEnabled
                 w _ITM_deregisterTMCloneTable
                 w _ITM_registerTMCloneTable
00000000000076b4 t JavaMain
0000000000219d18 d __JCR_END__
0000000000219d18 d __JCR_LIST__
0000000000010a09 t JLI_AcceptableRelease
00000000000106f8 t JLI_ExactVersionId
000000000001013c t JLI_FreeManifest
000000000000d12c T JLI_GetStdArgc
000000000000d121 T JLI_GetStdArgs
000000000000e8f8 t JLI_IsTraceLauncher
00000000000100b8 t JLI_JarUnpackFile
0000000000007248 T JLI_Launch
000000000001015e T JLI_ManifestIterate
000000000000e72f t JLI_MemAlloc
000000000000e7f7 t JLI_MemFree
000000000000e76e t JLI_MemRealloc
000000000000fea5 t JLI_ParseManifest
00000000000105c4 t JLI_PrefixVersionId
000000000000c495 T JLI_ReportErrorMessage
000000000000c558 T JLI_ReportErrorMessageSys
000000000000c65f T JLI_ReportExceptionDescription
000000000000be69 T JLI_ReportMessage
000000000000e8c1 T JLI_SetTraceLauncher
000000000000e905 t JLI_StrCCmp
000000000000e7b8 t JLI_StringDup
000000000000e812 t JLI_TraceLauncher
0000000000010c6b t JLI_ValidVersionString
0000000000011679 t JLI_WildcardExpandClasspath
000000000000d077 t jlong_format_specifier
000000000000d17f t JvmExists
000000000000e66d t JVMInit
                 w _Jv_RegisterClasses
000000000000b8dc t KnownVMIndex
000000000021a460 b knownVMs
000000000021a468 b knownVMsCount
000000000021a46c b knownVMsLimit
000000000021a4b0 b _launcher_debug
000000000021a418 b _launcher_name
000000000021a2c0 d launchModeNames
000000000021a2f0 d launchModeNames
000000000021a320 d launchModeNames
000000000021a360 d launchModeNames
000000000021a390 d launchModeNames
000000000021a3e0 d launchModeNames
0000000000013cc0 r lbase.3798
00000000000133c0 r lenfix.3893
0000000000013d00 r lext.3799
000000000000e11f t LoadJavaVM
0000000000009ffb t LoadMainClass
000000000000ca4c t LocateJRE
0000000000002af6 t logical_processors_per_package
                 U lseek64@@GLIBC_2.2.5
000000000021a490 b makePlatformStringMID
                 U malloc@@GLIBC_2.2.5
000000000021a4b8 b manifest
000000000021a3b0 d manifest_name
000000000000ceda t match_noeq
000000000021a478 b maxHeapSize
000000000021a45c b maxOptions
                 U memcmp@@GLIBC_2.2.5
                 U memcpy@@GLIBC_2.14
                 U memmove@@GLIBC_2.2.5
                 U memset@@GLIBC_2.2.5
0000000000009cc6 t NewPlatformString
0000000000009ec9 t NewPlatformStringArray
000000000021a338 d NMT_Env_Name.8166
000000000021a458 b numOptions
                 U opendir@@GLIBC_2.2.5
                 U open@@GLIBC_2.2.5
000000000021a450 b options
0000000000013c40 r order.3922
00000000000093e1 t ParseArguments
000000000000fcb1 t parse_nv_pair
00000000000087b5 t parse_size
                 U perror@@GLIBC_2.2.5
0000000000002724 t physical_memory
0000000000002b6a t physical_processors
000000000000e6c3 t PostJVMInit
                 U printf@@GLIBC_2.2.5
000000000000a9d9 t PrintJavaVersion
000000000021a402 b printUsage
000000000000aba4 t PrintUsage
000000000021a400 b printVersion
000000000021a403 b printXUsage
000000000021a4c8 b proc.2381
000000000021a4d0 b proc.2385
000000000021a4d8 b proc.2389
000000000021a4e0 b proc.2393
000000000021a4e8 b proc.2398
000000000021a4f0 b proc.2402
000000000021a4f8 b proc.2408
000000000000c753 t ProcessDir
000000000000e6dd t ProcessPlatformOption
000000000000c0ab t ProgramExists
000000000021a410 b _program_name
                 U pthread_attr_destroy@@GLIBC_2.2.5
                 U pthread_attr_init@@GLIBC_2.2.5
                 U pthread_attr_setdetachstate@@GLIBC_2.2.5
                 U pthread_attr_setstacksize@@GLIBC_2.2.5
                 U pthread_create@@GLIBC_2.2.5
                 U pthread_join@@GLIBC_2.2.5
                 U putchar@@GLIBC_2.2.5
                 U putenv@@GLIBC_2.2.5
                 U puts@@GLIBC_2.2.5
                 U readdir@@GLIBC_2.2.5
                 U read@@GLIBC_2.2.5
000000000000b134 t ReadKnownVMs
                 U readlink@@GLIBC_2.2.5
                 U realloc@@GLIBC_2.2.5
                 U realpath@@GLIBC_2.3
000000000000e6d6 t RegisterThread
0000000000002600 t register_tm_clones
000000000000be21 t RemovableOption
000000000000d45e t RequiresSetenv
000000000000c107 t Resolve
0000000000008b95 t SelectVersion
000000000021a3b8 d separators
00000000000026c0 t ServerClassMachine
00000000000027f3 t ServerClassMachineImpl
0000000000008ad8 t SetClassPath
000000000000e2a1 t SetExecname
000000000000a859 t SetJavaCommandLineProp
000000000000e607 t SetJavaLauncherPlatformProps
000000000000a9c1 t SetJavaLauncherProp
00000000000084de t SetJvmEnvironment
000000000021a408 b showSettings
000000000000aa98 t ShowSettings
000000000000b9be t ShowSplashScreen
000000000021a401 b showVersion
                 U snprintf@@GLIBC_2.2.5
000000000021a440 b splash_file_entry
000000000000e513 t SplashFreeLibrary
000000000021a448 b splash_jar_entry
000000000000e37c t SplashProcAddress
000000000021a3a8 d SPLASHSCREEN_SO
                 U sprintf@@GLIBC_2.2.5
                 U sscanf@@GLIBC_2.2.5
                 U __stack_chk_fail@@GLIBC_2.4
0000000000012e90 t stat
0000000000012e90 t __stat
                 U stderr@@GLIBC_2.2.5
                 U stdout@@GLIBC_2.2.5
                 U strcasecmp@@GLIBC_2.2.5
                 U strcat@@GLIBC_2.2.5
                 U strchr@@GLIBC_2.2.5
                 U strcmp@@GLIBC_2.2.5
                 U strcpy@@GLIBC_2.2.5
                 U strcspn@@GLIBC_2.2.5
                 U strdup@@GLIBC_2.2.5
                 U strerror@@GLIBC_2.2.5
                 U strlen@@GLIBC_2.2.5
                 U strncmp@@GLIBC_2.2.5
                 U strncpy@@GLIBC_2.2.5
                 U strpbrk@@GLIBC_2.2.5
                 U strrchr@@GLIBC_2.2.5
                 U strspn@@GLIBC_2.2.5
                 U strstr@@GLIBC_2.2.5
                 U strtok_r@@GLIBC_2.2.5
0000000000000000 A SUNWprivate_1.1
00000000000063b5 t syncsearch
                 U sysconf@@GLIBC_2.2.5
000000000021a2b0 d system_dir
000000000021a2e0 d system_dir
000000000021a310 d system_dir
000000000021a350 d system_dir
000000000021a380 d system_dir
000000000021a3d0 d system_dir
000000000021a470 b threadStackSize
000000000021a3f8 d __TMC_END__
000000000000a24a t TranslateApplicationArgs
000000000000d05d t UnsetEnv
00000000000039fe t updatewindow
000000000021a2b8 d user_dir
000000000021a2e8 d user_dir
000000000021a318 d user_dir
000000000021a358 d user_dir
000000000021a388 d user_dir
000000000021a3d8 d user_dir
0000000000010beb t valid_element
0000000000010aab t valid_simple_element
                 U vfprintf@@GLIBC_2.2.5
                 U vprintf@@GLIBC_2.2.5
000000000021a438 b _wc_enabled
000000000021a340 d whiteSpace.8426
0000000000011324 t wildcardConcat
00000000000113b2 t wildcardFileList
0000000000010e1b t WildcardIterator_close
0000000000010d45 t WildcardIterator_for
0000000000010de4 t WildcardIterator_next
                 U __xstat@@GLIBC_2.2.5
0000000000012e49 t zcalloc
0000000000012e6e t zcfree
000000000021a3c0 d zero_string
0000000000219d20 d z_errmsg
0000000000012e28 t zError
000000000021a4c0 b zip64_present
0000000000012e04 t zlibCompileFlags
0000000000012df7 t zlibVersion
```

其中的主要导出函数是`JLI`打头的函数:

![](https://pic4.zhimg.com/v2-1e6a01ab901558d3cdfecef7f038f8ab_b.jpg)

JAVA Launcher的相关函数.

从上面的函数列表可以看出, 我JLI相关的函数在这个动态库中导出. 其中的一个方法: `JLI_Launch` 也就是我们最熟悉的函数, 如下图所示.

![](https://pic2.zhimg.com/v2-720a998f3d356c07458a64366939d87d_b.jpg)

Java Launcher的入口函数.

通过这里我们可以知道. 即使是Launcher的代码. 也是通过一个动态链接库进行加载的. 也就是说main函数里面真的是什么都没有. 只有一个main函数入口.

### libjvm.so

> 这是一个并没有在依赖列表里面的动态链接库.因为我们知道用到了dl动态库. 同时我们之前的 [红色的红：OpenJDK8 Java程序启动解析(上) - Java程序是怎样执行到main方法的](https://zhuanlan.zhihu.com/p/527855131) 里已经介绍过main方法会加载jvm的核心动态库. libjvm.so

这个动态库是手工加载进来的. 主要是为了调用其`JNI_CreateJavaVM` 用于创建`Java`虚拟机. 这个就是`hotSpot`虚拟机的虚拟动态库了.我们也看一下核心库会提供什么方法. 不出意外会有特别多的方法:

> nm /home/lijianhong/sourcecode/jdk8u/build/linux-x86_64-normal-server-slowdebug/jdk/lib/amd64/server/libjvm.so

最终实际导出符号有20W+

![](https://pic2.zhimg.com/v2-6349adf912aa99e73940002b38356ea1_b.png)

导出符号数量.

最后我们后一下JNI相关的符号有哪些:

![](https://pic2.zhimg.com/v2-498cf2775449e4ccf88cfbdcd0144f45_b.jpg)

JVM虚拟机相关JNI方法

### 小结:

与之前分析Launcher的结论比较一致. Launcher是一个壳. 所有的函数都是由其它的模块提供.

-   libjli.so:

-   一个是JLI模块.这个提供Launcher相关的一个包装. 以动态库的形式提供. 名称:

-   **libjvm.so:**

-   这个是另外一个核心动态库. 这个是HostSpot的VM的核心库实现. Launcher在使用的时候通过手工加载的方式加载到内存使用. 核心是用于创建并初始化虚拟机.

-   [libc.so](https://link.zhihu.com/?target=http%3A//libc.so/), pthread 库.

-   这里由glibc提供的标准库.

从这里可以看出, Java对系统的相关的依赖特别的少. 所有的东西基本都是自己实现的. 比如GC ,执行引擎等.

## 参考

1.  [^](https://zhuanlan.zhihu.com/p/563479524#ref_1_0)linux-vdso.so.1介绍 [https://blog.csdn.net/wang_xya/article/details/43985241](https://blog.csdn.net/wang_xya/article/details/43985241)
2.  [^](https://zhuanlan.zhihu.com/p/563479524#ref_2_0)libpthread.so.0有什么作用 [https://bbs.pediy.com/thread-264491.htm](https://bbs.pediy.com/thread-264491.htm)
