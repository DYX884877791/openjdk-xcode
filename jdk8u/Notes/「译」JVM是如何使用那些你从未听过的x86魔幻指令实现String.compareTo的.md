---
source: https://zhuanlan.zhihu.com/p/139412068
---
### **魔幻的String.compareTo**

我们之前可能已经见过Java的String的比较方法，它会找出第一个不同的字符之间的距离，没找到不同，就返回较两个字符串长度之差

```
public int compareTo(String anotherString) {
    int len1 = value.length;
    int len2 = anotherString.value.length;
    int lim = Math.min(len1, len2);
    char v1[] = value;
    char v2[] = anotherString.value;

    int k = 0;
    while (k < lim) {
        char c1 = v1[k];
        char c2 = v2[k];
        if (c1 != c2) {
            return c1 - c2;
        }
        k++;
    }
    return len1 - len2;
}
```

但是你知道除了上面的实现外还有第二种秘密实现吗？String.compareTo是少数非常重要的方法之一，为此虚拟机工程师专门为它手写了汇编风格的代码（译注：这些代码会被汇编器转换为机器代码，所以实际上是指用**汇编风格写机器代码**）

```
# {method} 'compare' '(Ljava/lang/String;Ljava/lang/String;)I' in 'Test'
# parm0:    rsi:rsi   = 'java/lang/String'
# parm1:    rdx:rdx   = 'java/lang/String'
#           [sp+0x20]  (sp of caller)
7fe3ed1159a0: mov    %eax,-0x14000(%rsp)
7fe3ed1159a7: push   %rbp
7fe3ed1159a8: sub    $0x10,%rsp        
7fe3ed1159ac: mov    0x10(%rsi),%rdi  
7fe3ed1159b0: mov    0x10(%rdx),%r10
7fe3ed1159b4: mov    %r10,%rsi
7fe3ed1159b7: add    $0x18,%rsi
7fe3ed1159bb: mov    0x10(%r10),%edx
7fe3ed1159bf: mov    0x10(%rdi),%ecx
7fe3ed1159c2: add    $0x18,%rdi
7fe3ed1159c6: mov    %ecx,%eax
7fe3ed1159c8: sub    %edx,%ecx
7fe3ed1159ca: push   %rcx
7fe3ed1159cb: cmovle %eax,%edx
7fe3ed1159ce: test   %edx,%edx
7fe3ed1159d0: je     0x00007fe3ed115a6f
7fe3ed1159d6: movzwl (%rdi),%eax
7fe3ed1159d9: movzwl (%rsi),%ecx
7fe3ed1159dc: sub    %ecx,%eax
7fe3ed1159de: jne    0x00007fe3ed115a72
7fe3ed1159e4: cmp    $0x1,%edx
7fe3ed1159e7: je     0x00007fe3ed115a6f
7fe3ed1159ed: cmp    %rsi,%rdi
7fe3ed1159f0: je     0x00007fe3ed115a6f
7fe3ed1159f6: mov    %edx,%eax
7fe3ed1159f8: and    $0xfffffff8,%edx
7fe3ed1159fb: je     0x00007fe3ed115a4f
7fe3ed1159fd: lea    (%rdi,%rax,2),%rdi
7fe3ed115a01: lea    (%rsi,%rax,2),%rsi
7fe3ed115a05: neg    %rax
7fe3ed115a08: vmovdqu (%rdi,%rax,2),%xmm0
7fe3ed115a0d: vpcmpestri $0x19,(%rsi,%rax,2),%xmm0
7fe3ed115a14: jb     0x00007fe3ed115a40
7fe3ed115a16: add    $0x8,%rax
7fe3ed115a1a: sub    $0x8,%rdx
7fe3ed115a1e: jne    0x00007fe3ed115a08
7fe3ed115a20: test   %rax,%rax
7fe3ed115a23: je     0x00007fe3ed115a6f
7fe3ed115a25: mov    $0x8,%edx
7fe3ed115a2a: mov    $0x8,%eax
7fe3ed115a2f: neg    %rax
7fe3ed115a32: vmovdqu (%rdi,%rax,2),%xmm0
7fe3ed115a37: vpcmpestri $0x19,(%rsi,%rax,2),%xmm0
7fe3ed115a3e: jae    0x00007fe3ed115a6f
7fe3ed115a40: add    %rax,%rcx
7fe3ed115a43: movzwl (%rdi,%rcx,2),%eax
7fe3ed115a47: movzwl (%rsi,%rcx,2),%edx
7fe3ed115a4b: sub    %edx,%eax
7fe3ed115a4d: jmp    0x00007fe3ed115a72
7fe3ed115a4f: mov    %eax,%edx
7fe3ed115a51: lea    (%rdi,%rdx,2),%rdi
7fe3ed115a55: lea    (%rsi,%rdx,2),%rsi
7fe3ed115a59: dec    %edx
7fe3ed115a5b: neg    %rdx
7fe3ed115a5e: movzwl (%rdi,%rdx,2),%eax
7fe3ed115a62: movzwl (%rsi,%rdx,2),%ecx
7fe3ed115a66: sub    %ecx,%eax
7fe3ed115a68: jne    0x00007fe3ed115a72
7fe3ed115a6a: inc    %rdx
7fe3ed115a6d: jne    0x00007fe3ed115a5e
7fe3ed115a6f: pop    %rax
7fe3ed115a70: jmp    0x00007fe3ed115a73
7fe3ed115a72: pop    %rcx
7fe3ed115a73: add    $0x10,%rsp
7fe3ed115a77: pop    %rbp
7fe3ed115a78: test   %eax,0x17ed6582(%rip)
7fe3ed115a7e: retq
```

面的代码由[macroAssembler_x86.cpp](https://link.zhihu.com/?target=http%3A//hg.openjdk.java.net/jdk8/jdk8/hotspot/file/87ee5ee27509/src/cpu/x86/vm/macroAssembler_x86.cpp%23l5719)的`MacroAssembler::string_compare`生成，里面有详细的注释。值得注意的是其实如果CPU支持AVX256指令集，它还有一个更魔幻的版本，不过这里不会介绍，只关注上面的实现。

### **PCMPESTRI是什么**

`pcmpestri`是SSE4.2中引入的指令，属于`pcmpxstrx`向量化字符串比较指令家族。它通过一个控制字节（Control byte）复杂的功能，由于它们很复杂，x86指令集手册专门用一个小节来描述它，为了易于理解甚至还提供了一个flow图

![](https://pic3.zhimg.com/v2-1edba57c7db4e89a9649eac3dd525e6e_b.jpg)

看起来就像是把C语言代码放到CISC指令集里面一样！

控制字节的每个bit的功能如下：

```
-------0b 128-bit sources treated as 16 packed bytes.
-------1b 128-bit sources treated as 8 packed words.
------0-b Packed bytes/words are unsigned.
------1-b Packed bytes/words are signed.
----00--b Mode is equal any.
----01--b Mode is ranges.
----10--b Mode is equal each.
----11--b Mode is equal ordered.
---0----b IntRes1 is unmodified.
---1----b IntRes1 is negated (1’s complement).
--0-----b Negation of IntRes1 is for all 16 (8) bits.
--1-----b Negation of IntRes1 is masked by reg/mem validity.
-0------b Index of the least significant, set, bit is used
          (regardless of corresponding input element validity).
          IntRes2 is returned in least significant bits of XMM0.
-1------b Index of the most significant, set, bit is used
          (regardless of corresponding input element validity).
          Each bit of IntRes2 is expanded to byte/word.
0-------b This bit currently has no defined effect, should be 0.
1-------b This bit currently has no defined effect, should be 0.
```

(如果想要深入了解，可以参见Intel Instruction Set Reference Section 4.1)

`compareTo`使用`0x19`(译注：`'0b11001'`)，即对每8个packed words使用`equal each`模式(逐个相等比较)比较，结果取反。这个怪物指令使用4个寄存器作为输入：两个字符串作为参数，加上`%rax`和`%rdx`指定它们的长度（ PCMPESTRI中的E表示显示指定长度——与之相对的pcmpistri和pcmpistrm表示用null作为结尾符，即不显示指定长度）。结果（IntRes2）会放到`%ecx`。有时候这些不够的情况下`pcmpxstrx`家族的指令还会设置一些flag：

```
CFlag – Reset if IntRes2 is equal to zero, set otherwise
ZFlag – Set if absolute-value of EDX is < 16 (8), reset otherwise
SFlag – Set if absolute-value of EAX is < 16 (8), reset otherwise
OFlag – IntRes2[0]
AFlag – Reset
PFlag – Reset
```

不过这些都不在我们的讨论范围内，让我们仔细看看循环里面的代码，以及一些初始化动作

```
7fe3ed1159f6: mov    %edx,%eax
7fe3ed1159f8: and    $0xfffffff8,%edx
7fe3ed1159fd: lea    (%rdi,%rax,2),%rdi
7fe3ed115a01: lea    (%rsi,%rax,2),%rsi
7fe3ed115a05: neg    %rax
7fe3ed115a08: vmovdqu (%rdi,%rax,2),%xmm0
7fe3ed115a0d: vpcmpestri $0x19,(%rsi,%rax,2),%xmm0
7fe3ed115a14: jb     0x00007fe3ed115a40
7fe3ed115a16: add    $0x8,%rax
7fe3ed115a1a: sub    $0x8,%rdx
7fe3ed115a1e: jne    0x00007fe3ed115a08
```

`%rax`是较短字符串长度，`%rdx`与`~0x7`求与 （即最大循环次数的8倍）。然后它会比较指向两个字符串数组（`%rsi`和`%rdi`）的指针，由于循环前对`%rax`取反，所以循环实际上是反向进行的。  
它加载第一个字符串的8个字符到`%xmm0`寄存器，然后与第二个字符串的8个字符比较，如果CFlag设置了就跳出（即不同的字符已经找到，下标在`%ecx`中设置了），然后比较两个字符串的长度寄存器，并检测是否是最后一次迭代（即`%rdx`为0了）。但是一个负数怎么可能是正确的长度？额，忘记说了，`pcmpestri`使用长度的绝对值。

在循环之后，还有一个fallthrough分支，如果最短字符串剩下的字符不能被8整除了，那就使用这个分支处理剩下的字符，还有一个final分支，用来处理一个字符串是另一个的子字符串或者完全相同字符串的情况。

### **更合适的乐趣**

如果上面对你来说不是很复杂，那么可以看看更魔幻的[indexOf](https://link.zhihu.com/?target=http%3A//hg.openjdk.java.net/jdk8/jdk8/hotspot/file/87ee5ee27509/src/cpu/x86/vm/macroAssembler_x86.cpp%23l5456)[实现](https://link.zhihu.com/?target=http%3A//hg.openjdk.java.net/jdk8/jdk8/hotspot/file/87ee5ee27509/src/cpu/x86/vm/macroAssembler_x86.cpp%23l5305)（有两个版本，取决于待匹配字符串的长度），它使用控制字节`0x0d`，即`equal ordered`模式进行匹配
