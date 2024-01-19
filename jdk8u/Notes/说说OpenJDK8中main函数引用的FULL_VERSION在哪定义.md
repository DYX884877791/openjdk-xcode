---
source: https://zhuanlan.zhihu.com/p/522261419
---
## 引子

> 刚开始看`openJDK`的源码的时候,准备调试第一个HelloWorld就被难在这里了.  
> 如截图中的这个`FULL_VERSION`是在哪定义的,找了半天也没有找到. 但是调试的时候它就是有值的. 今天就来看看这个值到底是怎么声明的.

![](https://pic2.zhimg.com/v2-8e9ac6d2404a0bc1e63eb5c4cba4fdfd_b.jpg)

大家熟悉的c语言main函数入口

## 函数定义

先看一下完整的函数定义.

```
int
main(int argc, char **argv)
{
    int margc;
    char** margv;
    const jboolean const_javaw = JNI_FALSE;
#endif /* JAVAW */
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
    margc = argc;
    margv = argv;
#endif /* WIN32 */
    return JLI_Launch(margc, margv,
                   sizeof(const_jargs) / sizeof(char *), const_jargs,
                   sizeof(const_appclasspath) / sizeof(char *), const_appclasspath,
                   FULL_VERSION,
                   DOT_VERSION,
                   (const_progname != NULL) ? const_progname : *margv,
                   (const_launcher != NULL) ? const_launcher : *margv,
                   (const_jargs != NULL) ? JNI_TRUE : JNI_FALSE,
                   const_cpwildcard, const_javaw, const_ergo_class);
}
```

里面其实有部分是WIN32的相关的代码. 我们把代码精简一下,根据宏定义处理,把不要的内容删除掉:

```
int
main(int argc, char **argv)
{
    int margc;
    char** margv;
    const jboolean const_javaw = JNI_FALSE;
    margc = argc;
    margv = argv;

    return JLI_Launch(margc, margv,
                   sizeof(const_jargs) / sizeof(char *), const_jargs,
                   sizeof(const_appclasspath) / sizeof(char *), const_appclasspath,
                   FULL_VERSION,
                   DOT_VERSION,
                   (const_progname != NULL) ? const_progname : *margv,
                   (const_launcher != NULL) ? const_launcher : *margv,
                   (const_jargs != NULL) ? JNI_TRUE : JNI_FALSE,
                   const_cpwildcard, const_javaw, const_ergo_class);
}
```

这就是一个标准的c语言的main函数了. 参数argc,argv接入进来. 然后调用方法: `JLI_Launch`;

  
上面的变量: `FULL_VERSION`,实际不是一个变量. 实际是一个宏定义的常量. 只是在全部源代码中没有找到.倒是`DOT_VERSION` 有一个宏定义不在上面一点点.代码贴出来如下:

```
#if defined(JDK_MAJOR_VERSION) && defined(JDK_MINOR_VERSION)
#define DOT_VERSION JDK_MAJOR_VERSION "." JDK_MINOR_VERSION
#else
```

这个其实就要说到GCC 在编译的时候可以通过命令行传入变量.或者是通过环境变量传入一个预定义的值.比如我写这样的代码:

```
#include<stdio.h>

int main(int argc ,char** argv)
{
    printf("%s says: hello world\n",PEOPLE)
}
```

然后我编译代码,会报一个错. `PEOPLE` 没有声明 , 当然还有一个报错是没有分号. (日常犯错)

```
gcc -g  main.c
main.c: In function ‘main’:
main.c:5: error: ‘PEOPLE’ undeclared (first use in this function)
main.c:5: error: (Each undeclared identifier is reported only once
main.c:5: error: for each function it appears in.)
main.c:6: error: expected ‘;’ before ‘}’ token
```

加上分号:

```
#include<stdio.h>

int main(int argc ,char** argv)
{
    printf("%s says: hello world\n",PEOPLE);
}
```

分号好了,仍然有找不到符号的报错:

```
main.c: In function ‘main’:
main.c:5: error: ‘PEOPLE’ undeclared (first use in this function)
main.c:5: error: (Each undeclared identifier is reported only once
main.c:5: error: for each function it appears in.)
```

现在有两种处理办法:

1.  我声明一个宏定义.比如:`#define "PEOPLE"`

```
#include<stdio.h>

#define PEOPLE "Jack"

int main(int argc ,char** argv)
{
    printf("%s says: hello world\n",PEOPLE);
}
```

编译运行结果如下:

```
[root@iZ25a8x4jw7Z ~/ccode/define]#gcc -g  main.c
[root@iZ25a8x4jw7Z ~/ccode/define]#./a.out
Jack says: hello world
```

但是,如果程序里面真的没有声明,或者我想在编译的时候再动态给怎么办呢?

1.  在编译参数中动态声明:

```
[root@iZ25a8x4jw7Z ~/ccode/define]#gcc -g  -DPEOPLE=\"JackMa\"  main.c
[root@iZ25a8x4jw7Z ~/ccode/define]#./a.out
JackMa says: hello world
```

因此,在此OpenJDK的代码中,应该也是这样定义的. 但是事情并没有我们想象的那么简单.  
如果真的这么简单这就是不JDK源代码了. 搜索源代码全文路径里面. 有如下一段代码:

![](https://pic1.zhimg.com/v2-eb657bcbb0ca6026eec04449e3f8e05c_b.jpg)

> 这里其实省略了很多中间过程, 如果你很了解GNUMAKE的话. 那应该能像我现在一样能够直接得到这个结果.而把其它干扰的结果全部给排除掉.

上图中的两个框分别有两个`FULL_VERSION=`这样的赋值操作.仔细分别看两段代码:

> 在`speck.gmk`中的定义如下:

```
ifneq ($(USER_RELEASE_SUFFIX), )
  FULL_VERSION=$(RELEASE)-$(USER_RELEASE_SUFFIX)-$(JDK_BUILD_NUMBER)
else
  FULL_VERSION=$(RELEASE)-$(JDK_BUILD_NUMBER)
endif
JRE_RELEASE_VERSION:=$(FULL_VERSION)
```

> 在`speck.gmk.in`中的定义如下:

```
ifneq ($(USER_RELEASE_SUFFIX), )
  FULL_VERSION=$(RELEASE)-$(USER_RELEASE_SUFFIX)-$(JDK_BUILD_NUMBER)
else
  FULL_VERSION=$(RELEASE)-$(JDK_BUILD_NUMBER)
endif
JRE_RELEASE_VERSION:=$(FULL_VERSION)
```

## gmk文件是什么

发现这两段脚本内容是一样的. 这是什么脚本呢? 不卖关子,直接说答案:

> 这些脚本是`GNU MAKE` 的语法形式的脚本.这里的`ifeq`是一个条件语句.  
> 里面的 `FULL_VERSION = XXX` 是递归扩展(或者说是延时扩展变量的定义)  
> 然后再下面的: `JRE_RELEASE_VERSION := $(FULL_VERSION)` 是一个立即扩展变量的定义.  
> 曾经我也疑惑过这个文件的后缀为什么是`.gmk`,是不是有会玄机. 下面是我曾经的搜索结果:  
> 曾经一度以为这是不是使用了哪种小众的脚本工具.或者是一人黑魔法的东西.

![](https://pic1.zhimg.com/v2-54ea83c5a50c55436117d3e66aab74a8_b.jpg)

## 答案:

> 其实这里的`.gmk`的含义,大概是 _GNU MAKE FILE_ 的意思. 简称为`gmk` , 实际上官方的makefile的名称在如下三个

-   GnuMakefile
-   makefile
-   Makefile

> 注: Clion里面内置的对于makefile的支持还有一种后缀: `.mk`

具体可以参考: [3.2 What Name to Give Your Makefile - Gnu Manual](https://link.zhihu.com/?target=https%3A//www.gnu.org/software/make/manual/make.html%23Makefile-Names)  
为什么这里没有使用这个名字呢? 原因是`OpenJDK`的项目非常的复杂.一个简单的makefile已经无法描述项目的结构了. 因此会有`子Makefile`的引入.  
`OpenJDK` 一般就用`*.gmk`来代表内部的子makefile文件.这种文件一般是用来被主Makefile引用的. 就像`C++`里面的`#include`指令一样.

说完为什么是`*.gmk`后再回过来看我们刚刚的问题. 这里有两个文件一个是:`spec.gmk.in` ,另外是`spec.gmk`; 这个又是为什么呢? 这就要说到另外一个工具:`autoconf`;  
如果看过我的其它几个编译`OpenJDK`的文摘. 就会知道,我们的`OpenJDK` 在JDK8开始, 使用了标准的`GNU MAKE`来管理和编译项目.  
同时项目的配置,使用了自动配置工具,也就是刚刚提到的`autoconf`.

## AutoConf & AutoTools

> autoconf的作用是,可以通过一堆的描述(当然是描述性的脚本语言了), 由于其中的复杂性和时间的关系. 这里给出一张图大概描述下autoconf的工作原理与流程

![](https://pic2.zhimg.com/v2-ec22e0ad981b7a0ee1f151fad9b87aa5_b.jpg)

AutoTools 工具集自动处理编译依赖关系的流程

这里只提到了configure文件,以及makefile文件的生成过程. 当然spec.gmk也是makefile.只是文件的名称和后缀不一样而已. 因此就不展开详说了.  
简单说就是我定义了一个*.in的文件.然后在在里面定义一些宏和文本.这样就可以生成我们的目标配置文件.  
这里的宏是`M4`脚本.具体的参考可以参考官网:[https://www.gnu.org/software/m4/m4.html](https://link.zhihu.com/?target=https%3A//www.gnu.org/software/m4/m4.html) , 当然autoconf也是有相应的官网的:[https://www.gnu.org/software/autoconf/](https://link.zhihu.com/?target=https%3A//www.gnu.org/software/autoconf/)

这样变量的定义找到后, 最后再说一说这个变量怎么生效的. makefile的变量和环境变量,都可以在主makefile和子makefile间进行传递.这样就可以把最上层的变量定义.  
传递到最底层的makefile的编译参数中.最终使我们这里的`main.c`中的版本号的值得到了定义.

> 注意: 这个值在makefile里面叫变量, 在编译到main.c的目标中其实是一个字面常量. 也就是一个字面的string的定义. 在我的这里,其值是:  
> `1.8.0-internal-yourName-b00`

## `后`记

至此,这个变量的来源与定义就基本搞清楚了. 前前后后大概花了半个月的时间来了解这个. 主要是要学习`GNU MAKE` 的基本语法和基本使用方法. 可以参考如下的资料进行自学,如果你也感兴趣的话:

## 引用

1.  [https://www.gnu.org/software/make/manual/make.html#SEC_Contents](https://link.zhihu.com/?target=https%3A//www.gnu.org/software/make/manual/make.html%23SEC_Contents)
2.  [https://www.gnu.org/software/autoconf/](https://link.zhihu.com/?target=https%3A//www.gnu.org/software/autoconf/)
3.  [https://www.gnu.org/software/m4/m4.html](https://link.zhihu.com/?target=https%3A//www.gnu.org/software/m4/m4.html)
4.  [linux下使用automake、autoconf生成configure文件](https://link.zhihu.com/?target=https%3A//blog.csdn.net/whatday/article/details/86915452)
5.  [configure、 make、 make install 背后的原理(翻译) - 知乎](https://zhuanlan.zhihu.com/p/77813702)
6.  [configure、make、makeinstall背後的原理(翻譯)](https://link.zhihu.com/?target=https%3A//codertw.com/%25E7%25A8%258B%25E5%25BC%258F%25E8%25AA%259E%25E8%25A8%2580/643856/)
7.  [[Linux]./configure | make | make install的工作过程与原理](https://link.zhihu.com/?target=https%3A//www.cnblogs.com/johnnyzen/p/13789760.html)
8.  [openjdk1.8中的Makefile](https://link.zhihu.com/?target=https%3A//www.lehoon.cn/backend/2018/12/06/openjdk-makefile-note.html%23Jdk%25E7%259B%25AE%25E5%25BD%2595)
9.  [makefile简明教程 - 不全,基本的语法可以参考](https://link.zhihu.com/?target=https%3A//www.zhaixue.cc/makefile/makefile-intro.html)
10.  [Openjdk中的build-infra变量,引入GNU MAKE , autoconf - 邮件列表](https://link.zhihu.com/?target=http%3A//mail.openjdk.java.net/pipermail/build-infra-dev/2011-August/000034.html)
11.  [hg: build-infra/jdk7/corba: First example of configure script.](https://link.zhihu.com/?target=http%3A//mail.openjdk.java.net/pipermail/build-infra-dev/2011-August/000037.html)
