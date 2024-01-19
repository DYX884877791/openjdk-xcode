---
source: https://zhuanlan.zhihu.com/p/441705104
---
在模板解释器下，JVM 的字节码实现会切换为汇编实现。若是切换为汇编实现后，我们就需要对汇编代码所在的内存位置进行断点。从而达到对 OpenJDK 的汇编字节码的调试。以下为了方便，主要通过调试 JVM `iload_0`的汇编实现来演示调试方法。

### 1.添加 -XX:+PrintInterpreter 可以用来查看VM启动时在内存中生成的模板

例如：`/home/zifeihan/qemu/bin/qemu-riscv32 -L /opt/riscv32/sysroot -g 28080 ./java -XX:+PrintInterpreter -version > log.txt` 注意我们在启动JVM时添加了`-XX:+PrintInterpreter` 参数，用于将字节码的汇编实现打印出来，最后我们将打印出来的日志内容输出到文件当中。

### 2.使用 VScode 进行 debug

将断点打在 init.cpp:120 ，使其运行 init.cpp:120 位置停下。到之所以打在这个位置是为了使 TemplateInterpreter 初始化完成，初始化完成后，`iload_0`的汇编实现，也能在 log 日志中查看。

### 3.在log日志中查看`iload_0`的汇编实现

我们打开在第一步输出的日志文件，查找 `iload_0`的汇编实现。我这里的内容如下：

```
----------------------------------------------------------------------
iload_0  26 iload_0  [0x3cccc140, 0x3cccc1c0]  128 bytes

  // 栈顶缓存
  0x3cccc140: addi  s4,s4,-4
  0x3cccc144: sw    a0,0(s4)
  0x3cccc148: j 0x3cccc180
  0x3cccc14c: addi  s4,s4,-4
  0x3cccc150: fsw   fa0,0(s4)
  0x3cccc154: j 0x3cccc180
  0x3cccc158: addi  s4,s4,-8
  0x3cccc15c: fsd   fa0,0(s4)
  0x3cccc160: j 0x3cccc180
  0x3cccc164: addi  s4,s4,-8
  0x3cccc168: sw    zero,4(s4)
  0x3cccc16c: sw    a0,0(s4)
  0x3cccc170: j 0x3cccc180
  0x3cccc174: addi  s4,s4,-4
  0x3cccc178: add   a0,a0,zero
  0x3cccc17c: sw    a0,0(s4)

  // iload_0，Load int from local variable
  0x3cccc180: lw    a0,0(s8)

  // 指令跳转
  0x3cccc184: lbu   t0,1(s6)
  0x3cccc188: addi  s6,s6,1
  0x3cccc18c: lui   t1,0x0
  0x3cccc190: addi  t1,t1,1024 # 0x00000400
  0x3cccc194: add   t1,t0,t1
  0x3cccc198: slli  t1,t1,0x2
  0x3cccc19c: add   t1,s5,t1
  0x3cccc1a0: lw    t1,0(t1)
  0x3cccc1a4: jr    t1

  // 省略内存补齐内容
  xxx
```

### 4.使用日志表中的汇编地址进行断点调试

此时我们可以重复前面1-2步骤，在前面第二步骤完成后，拿着这里的汇编指令的内存地址，在 GDB 窗口中进行断点调试。例如对`0x3cccc180: lw a0,0(s8)` 指令进行断点，我们可以在前面第二步骤完成后使用 `-exec b *0x3cccc180`，断点后使 VScode 继续运行，直到在 0x3cccc180 内存位置停止：

```
=thread-group-added,id="i1"
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
Type "apropos word" to search for commands related to "word".
Warning: Debuggee TargetArchitecture not detected, assuming x86_64.
=cmd-param-changed,param="pagination",value="off"
_start () at rtld.c:11
Loaded '/opt/riscv32/sysroot/lib/ld-linux-riscv32-ilp32d.so.1'. Symbols loaded.
[New Thread 1.3328211]

Thread 1 received signal SIGTRAP, Trace/breakpoint trap.
0x3ffb5260 in __futex_abstimed_wait_common64 (futex_word=<error reading variable: dwarf2_find_location_expression: Corrupted DWARF expression.>, expected=<error reading variable: dwarf2_find_location_expression: Corrupted DWARF expression.>, clockid=<error reading variable: dwarf2_find_location_expression: Corrupted DWARF expression.>, abstime=<error reading variable: dwarf2_find_location_expression: Corrupted DWARF expression.>, private=<error reading variable: dwarf2_find_location_expression: Corrupted DWARF expression.>, cancel=<error reading variable: dwarf2_find_location_expression: Corrupted DWARF expression.>) at ../sysdeps/nptl/futex-internal.c:74
Loaded '/opt/riscv32/sysroot/usr/lib/libz.so'. Symbols loaded.
Loaded '/opt/riscv32/sysroot/lib/libpthread.so.0'. Symbols loaded.
Loaded '/home/zifeihan/jdk11u/build/linux-riscv32-normal-core-slowdebug/jdk/lib/jli/libjli.so'. Symbols loaded.
Loaded '/opt/riscv32/sysroot/lib/libdl.so.2'. Symbols loaded.
Loaded '/opt/riscv32/sysroot/lib/libc.so.6'. Symbols loaded.
Loaded '/home/zifeihan/jdk11u/build/linux-riscv32-normal-core-slowdebug/jdk/lib/server/libjvm.so'. Symbols loaded.
Loaded '/opt/riscv32/sysroot/lib/libatomic.so.1'. Symbols loaded.
Loaded '/opt/riscv32/sysroot/lib/libm.so.6'. Symbols loaded.
Execute debugger commands using "-exec <command>", for example "-exec info registers" will list registers in use (when GDB is the debugger)
-exec t 2
[Switching to thread 2 (Thread 1.3328211)]
#0  __GI__dl_debug_state () at dl-debug.c:74
74  dl-debug.c: No such file or directory.
=thread-selected,id="2",frame={level="0",addr="0x3ffee7c2",func="__GI__dl_debug_state",args=[],file="dl-debug.c",fullname="/root/riscv-gnu-toolchain/riscv-glibc/elf/dl-debug.c",line="74",arch="riscv:rv32"}
[Switching to thread 2 (Thread 1.3328211)]
#0  __GI__dl_debug_state () at dl-debug.c:74
74  in dl-debug.c
=thread-selected,id="2",frame={level="0",addr="0x3ffee7c2",func="__GI__dl_debug_state",args=[],file="dl-debug.c",fullname="/root/riscv-gnu-toolchain/riscv-glibc/elf/dl-debug.c",line="74",arch="riscv:rv32"}


Thread 1 received signal SIGSEGV, Segmentation fault.
[Switching to Thread 1.3327968]
0x3cd6641c in ?? ()
Loaded '/opt/riscv32/sysroot/lib/librt.so.1'. Symbols loaded.
Loaded '/home/zifeihan/jdk11u/build/linux-riscv32-normal-core-slowdebug/jdk/lib/libverify.so'. Symbols loaded.
Loaded '/home/zifeihan/jdk11u/build/linux-riscv32-normal-core-slowdebug/jdk/lib/libjava.so'. Symbols loaded.
Loaded '/home/zifeihan/jdk11u/build/linux-riscv32-normal-core-slowdebug/jdk/lib/libjimage.so'. Symbols loaded.
Loaded '/opt/riscv32/sysroot/lib/libnss_files.so.2'. Symbols loaded.
Loaded '/home/zifeihan/jdk11u/build/linux-riscv32-normal-core-slowdebug/jdk/lib/libzip.so'. Symbols loaded.
Loaded '/home/zifeihan/jdk11u/build/linux-riscv32-normal-core-slowdebug/jdk/lib/server/hsdis-riscv32.so'. Symbols loaded.
-exec b *0x3cccc180
Breakpoint 3 at 0x3cccc180


Thread 1 received signal SIGSEGV, Segmentation fault.
0x3cd66428 in ?? ()
[New Thread 1.3328526]

Thread 1 hit Breakpoint 3, 0x3cccc180 in ?? ()
-exec i r
ra             0x3ccbfeb0   0x3ccbfeb0
sp             0x3f0d0bc8   0x3f0d0bc8
gp             0x12800  0x12800
tp             0x3f0d2930   0x3f0d2930
t0             0x1a 26
t1             0x3cccc180   1020051840
t2             0x2a 42
fp             0x3f0d0c48   0x3f0d0c48
s1             0x3f0d0be8   1057819624
a0             0x4  4
a1             0x2  2
a2             0xc4 196
a3             0x0  0
a4             0x4  4
a5             0x4  4
a6             0x0  0
a7             0x3ed19000   1053921280
s2             0x102    258
s3             0x3ccc41ec   1020019180
s4             0x3f0d0c20   1057819680
s5             0x3fe7fc2c   1072167980
s6             0x2f1f4e77   790580855
s7             0x3ed19000   1053921280
s8             0x3f0d0c5c   1057819740
s9             0x3ffbb1c8   1073459656
s10            0x2f1f55e0   790582752
s11            0x3f0d2420   1057825824
t3             0x3ffb1946   1073420614
t4             0x73614874   1935755380
t5             0x3ccc8544   1020036420
t6             0x2f1f4ed8   790580952
pc             0x3cccc180   0x3cccc180
dscratch       Could not fetch register "dscratch"; remote failure reply 'E14'
mucounteren    Could not fetch register "mucounteren"; remote failure reply 'E14'
```
