---
created: 2021-12-07T01:48:19 (UTC +08:00)
tags: []
source: https://www.jianshu.com/p/feaed5853a02
author: 
---

# OpenJDK 编译调试指南(Ubuntu 16.04 + MacOS 10.15) - 简书
---
本篇文章主要介绍在`MacOS`系统和`Ubuntu`系统上如何编译`OpenJDK`项目代码，并使用`IDE`工具`JetBrains CLion`（下文简称`CLion`）来运行/调试`OpenJDK`。  

文中仅包含两种操作系统的特定版本（`MacOS 10.15`和`Ubuntu 16.04`）下的方法，不同版本下可能会略有差异。希望对读者有一定的参考价值。

总体来说，编译`OpenJDK11`在两种系统上都没有太大的阻碍，难度低于`OpenJDK8`。编译`OpenJDK8`在`Ubuntu`上比较简单，在`MacOS`上比较繁琐复杂。

完成编译并实现调试`OpenJDK`流程可以分为以下几个步骤：

1.  获取`OpenJDK`项目源代码
2.  下载一个合适版本的`JDK`作为`BootJDK`
3.  下载所需工具链，包括编译器、调试器、构建工具等
4.  下载所需依赖库
5.  `Running Configure`（配置）
6.  `Running Make`（构建）
7.  导入`CLion`并进行`Run/Debug`配置

其中3-6步对于不同操作系统和不同`OpenJDK`版本差异较大，其余步骤差异较小，所以本文的行文顺序是先将1-2、7三部分以及一些前置知识做详细介绍。在之后的具体场景（不同操作系统 + 不同`OpenJDK`版本）中针对对这些部分只做差异点的特殊说明。

## 如何获取 OpenJDK 源代码？

编译调试的第一步当然是获取到`OpenJDK`的源代码，获取方式主要有以下三种：

### 1\. 通过`OpenJDK`官方的`Mercurial`仓库下载

`OpenJDK`官方使用`Mercurial`来进行版本控制。`Mercurial`仓库地址：[http://hg.openjdk.java.net/](https://links.jianshu.com/go?to=http%3A%2F%2Fhg.openjdk.java.net%2F)，主要项目地址：

项目

地址

jdk

[http://hg.openjdk.java.net/jdk/jdk](https://links.jianshu.com/go?to=http%3A%2F%2Fhg.openjdk.java.net%2Fjdk%2Fjdk)

jdk8u

[http://hg.openjdk.java.net/jdk8u/jdk8u/](https://links.jianshu.com/go?to=http%3A%2F%2Fhg.openjdk.java.net%2Fjdk8u%2Fjdk8u%2F)

jdk11u

[https://hg.openjdk.java.net/jdk-updates/jdk11u/](https://links.jianshu.com/go?to=https%3A%2F%2Fhg.openjdk.java.net%2Fjdk-updates%2Fjdk11u%2F)

通过这种方式下载源代码，需要先安装`Mercurial`工具，并使用`hg clone <url>`下载源代码。比如下载 `jdk8u`使用如下命令：

```
hg clone http://hg.openjdk.java.net/jdk8u/jdk8u/
```

这种方式下载比较慢，而且会有中断的情况，**不推荐使用**。

### 2\. 通过镜像`Git`仓库下载

OpenJDK官方在`GitHub`上有JDK项目的仓库镜像，主页地址：[https://github.com/openjdk](https://links.jianshu.com/go?to=https%3A%2F%2Fgithub.com%2Fopenjdk)，主要项目地址：

项目

地址

jdk

[https://github.com/openjdk/jdk](https://links.jianshu.com/go?to=https%3A%2F%2Fgithub.com%2Fopenjdk%2Fjdk)

jdk11u

[https://github.com/openjdk/jdk11u](https://links.jianshu.com/go?to=https%3A%2F%2Fgithub.com%2Fopenjdk%2Fjdk11u)

jdk12u

[https://github.com/openjdk/jdk12u](https://links.jianshu.com/go?to=https%3A%2F%2Fgithub.com%2Fopenjdk%2Fjdk12u)

但在官方`git`仓库中没有看到 `jdk11` 以下的版本。

除此之外还有一些非官方的镜像`Git`仓库。比如`AdoptOpenJDK`项目，主页地址：[https://github.com/AdoptOpenJDK/](https://links.jianshu.com/go?to=https%3A%2F%2Fgithub.com%2FAdoptOpenJDK%2F)，主要项目地址：

项目

地址

jdk

[https://github.com/AdoptOpenJDK/openjdk-jdk](https://links.jianshu.com/go?to=https%3A%2F%2Fgithub.com%2FAdoptOpenJDK%2Fopenjdk-jdk)

jdk8u

[https://github.com/AdoptOpenJDK/openjdk-jdk8u](https://links.jianshu.com/go?to=https%3A%2F%2Fgithub.com%2FAdoptOpenJDK%2Fopenjdk-jdk8u)

jdk11u

[https://github.com/AdoptOpenJDK/openjdk-jdk11u](https://links.jianshu.com/go?to=https%3A%2F%2Fgithub.com%2FAdoptOpenJDK%2Fopenjdk-jdk11u)

使用这种方式需要安装`git`工具，并使用`git clone <url>`下载源代码。比如下载`jdk8u`使用如下命令：

```
git clone https://github.com/AdoptOpenJDK/openjdk-jdk8u
```

`git`工具使用方便，并且比从官方`Mercurial`仓库下载要快很多，**推荐使用**。

### 3\. 下载`Mercurial`仓库或者`Git`仓库 打包文件

除此之外，`OpenJDK`官方的`Mercurial`仓库还提供了直接下载压缩包的入口，入口在各个项目页面左侧`bz2`,`zip`,`gz`三个链接，点击即可下载。如下图所示：  

![](https://upload-images.jianshu.io/upload_images/23289123-e65fdb5e54e9a00a.png?imageMogr2/auto-orient/strip|imageView2/2/w/1200/format/webp)

image

同样在`GitHub`上的每个项目都可以下载`ZIP`格式的打包文件。如下图所示：  

![](https://upload-images.jianshu.io/upload_images/23289123-0b172181531292a3.png?imageMogr2/auto-orient/strip|imageView2/2/w/1200/format/webp)

image

显然这种方式的缺点是缺少版本控制的能力，如果对项目代码进行了改动想要恢复是做不到的，也不能够方便地获取最新代码改动，但是相比前两种方式更下载更快，也不要任何额外工具。

## 如何下载JDK？

让人觉得矛盾的是，编译JDK之前我们需要一个已经编译好的`JDK`作为`Boot JDK`。一般需要的`JDK`版本是编译版本前一版本的`JDK`，比如编译`JDK8`需要`JDK7`,编译`JDK11`需要`JDK10`。  
有几种途径可以下载`JDK`：

1.  在[Oracle官网](https://links.jianshu.com/go?to=https%3A%2F%2Fwww.oracle.com%2Fjava%2Ftechnologies%2Foracle-java-archive-downloads.html)下载 `Oracle JDK`。下载需要登录账号
2.  下载`OpenJDK`
    -   在[https://jdk.java.net/](https://links.jianshu.com/go?to=https%3A%2F%2Fjdk.java.net%2F)可以找到`Oracle`提供的基于`OpenJDK`的参考实现。但大部分只有`Linux`版本  
        
        ![](https://upload-images.jianshu.io/upload_images/23289123-d0777217d0d8dd82.png?imageMogr2/auto-orient/strip|imageView2/2/w/1134/format/webp)
        
        image
        
          
        
        ![](https://upload-images.jianshu.io/upload_images/23289123-2194410119c53f2b.png?imageMogr2/auto-orient/strip|imageView2/2/w/1200/format/webp)
        
        image
        
    -   也可以使用其他组织预编译好的`OpenJDK`。比如[AdoptOpenJDK](https://links.jianshu.com/go?to=https%3A%2F%2Fadoptopenjdk.net%2F)，但是没有`jdk8`以下版本
3.  在`Linux`系统下，还可以通过软件包管理器(比如`Ubuntu`下的`apt`，`Centos`下的`yum`)下载安装`JDK`

## JSON Compilation Database

`CLion`对`CMake`构建的项目支持很友好，但`OpenJDK`项目是基于`Make`构建的，对于使用`Make`构建的项目，`CLion`仍然可以通过`Compilation Database`来导入项目，而不用将其修改为`CMake`项目。同时也能通过`Compilation Database`实现代码的分析、跳转等功能，这对于我们进行代码调试很有帮助。

有关于`Compilation Database`的介绍可以参考这几篇文章：

-   `Clang`官方的`Compilation Database`介绍页面：[JSON Compilation Database Format Specification — Clang 11 documentation](https://links.jianshu.com/go?to=https%3A%2F%2Fclang.llvm.org%2Fdocs%2FJSONCompilationDatabase.html)
-   这篇文章介绍了在不同场景下生成`Compilation database`的各种工具：[Compilation database — Sarcasm notebook](https://links.jianshu.com/go?to=https%3A%2F%2Fsarcasm.github.io%2Fnotes%2Fdev%2Fcompilation-database.html)
-   `CLion`的帮助页面简要介绍了`Compilation Database`，以及在`CLion`的使用：[Compilation Database - Help | CLion](https://links.jianshu.com/go?to=https%3A%2F%2Fwww.jetbrains.com%2Fhelp%2Fclion%2Fcompilation-database.html)

对于基于`Make`构建的`OpenJDK`项目来说，有一些工具可以生成`Compilation Database`，比如下面几个：

-   [https://github.com/rizsotto/Bear](https://links.jianshu.com/go?to=https%3A%2F%2Fgithub.com%2Frizsotto%2FBear)
-   [https://github.com/rizsotto/scan-build](https://links.jianshu.com/go?to=https%3A%2F%2Fgithub.com%2Frizsotto%2Fscan-build)
-   [https://github.com/nickdiego/compiledb](https://links.jianshu.com/go?to=https%3A%2F%2Fgithub.com%2Fnickdiego%2Fcompiledb)

通常默认生成的`Compilation Database`是一个`compile_commands.json`文件。

除此之外，在`OpenJDK 11u`及之后版本中，`OpenJDK`官方提供了对于`IDE`的支持，可以使用`make compile-commands`命令生成`Compilation Database`，不需要使用额外的工具，具体命令可以查看看源代码目录下的[`\doc\ide.md`](https://links.jianshu.com/go?to=https%3A%2F%2Fgithub.com%2Fopenjdk%2Fjdk11u%2Fblob%2Fmaster%2Fdoc%2Fide.md)

## 如何将OpenJDK项目导入CLion中并运行/调试

> 读者可以选择先跳过本小节，然后在完成前面几个步骤之后再来根据本小节进行导入调试。

对于如何在`CLion`中管理基于`Make`构建的项目，`CLion`的官方帮助文档中有详细的介绍：  
[Managing Makefile Projects](https://links.jianshu.com/go?to=https%3A%2F%2Fwww.jetbrains.com%2Fhelp%2Fclion%2Fmanaging-makefile-projects.html)。

参考上述文档，本小节简要介绍了如何在`CLion`中导入`OpenJDK`项目并运行/调试的方法。请注意小节中所展示的图示是基于`Ubuntu 16.04`系统 + `CLion 2020.1.1` 环境下的，操作步骤基本也适用于`MacOS 系统`下同版本的`CLion`。

1.  下载安装`CLion`，并安装`Makefile Support`插件。建议使用最新版本，比较老的版本是不支持`Make`构建的项目的。
    
2.  导入项目。打开`CLion`，选择`Open Or Import`，选择项目目录中的`compile_commands.json`文件，弹出框选择`Open as Project`，等待文件索引完成。
    
    > `compile_commands.json`的生成方法及生成位置根据不同的`OpenJDK`版本略有不同，具体位置请看下文具体场景的介绍。
    
    ![](https://upload-images.jianshu.io/upload_images/23289123-be557bf720f4cc0b.gif?imageMogr2/auto-orient/strip|imageView2/2/w/674/format/webp)
    
    image
    
3.  创建自定义`Build Target`。点击`File`菜单栏，`Settings | Build, Execution, Deployment | Custom Build Targets`，点击`+`新建一个 `Target`。
    
    -   `Name`：`Target`的名字，之后在创建`Run/Debug`配置的时候会看到这个名字
    -   点击`Build`或者`Clean`右边的三点，弹出框中点击`+`新建两个`External Tool`配置如下：
        
        ```
        # 第一个配置如下，用来指定构建指令
        # Program 和 Arguments 共同构成了所要执行的命令 "make all"
        Name: make
        Program: make
        Arguments: all
        Working directory: {项目的根目录}
        
        # 第二个配置如下，用来清理构建输出
        # Program 和 Arguments 共同构成了所要执行的命令 "make clean"
        Name: make clean
        Program: make
        Arguments: clean
        Working directory: {项目的根目录}
        ```
        
        ![](https://upload-images.jianshu.io/upload_images/23289123-a264e54ce10e2d8c.gif?imageMogr2/auto-orient/strip|imageView2/2/w/1180/format/webp)
        
        image
        
    -   `ToolChain`选择`Default`；`Build`选择`make`（上面创建的第一个`External Tool`）；`Clean`选择`make clean`（上面创建的第二个`External Tool`）
    
    > 其中两个`External Tool`配置中`make`的参数可以根据需要改变。一般情况用于`Build`的配置与执行构建时`make`的`target`保持一致即可。
    
4.  创建自定义的`Run/Debug configuration`。点击`Run`菜单栏,`Edit Configurations`， 点击`+`，选择`Custom Build Application`，配置如下：
    
    ```
    # Executable 和 Program arguments 可以根据需要调试的信息自行选择
    # NameL：Configure 的名称
    Name: linux-x86_64-normal-server-slowdebug
    # Target：选择上一步创建的 “Custom Build Target”
    Target: linux-x86_64-normal-server-slowdebug
    # Executable：程序执行入口，也就是需要调试的程序
    Executable: 这里我们调试`java`，选择`{source_root}/build/{build_name}/jdk/bin/java`。
    # Program arguments: 与 “Executable” 配合使用，指定其参数
    Program arguments: 这里我们选择`-version`，简单打印一下`java`版本。
    ```
    
    ![](https://upload-images.jianshu.io/upload_images/23289123-59f1190b4ca17a2b.gif?imageMogr2/auto-orient/strip|imageView2/2/w/1200/format/webp)
    
    image
    
    如果不想每次运行/调试前都执行`Build`操作（在这里就是`Make`构建过程，比较耗时），可以在编辑页下方`Before launch`框中删除`Build`条目。
    
5.  点击`Run`/`Debug`开始运行/调试。
    
    -   如果使用的调试器是`gdb`（`Ubuntu`下默认），调试的时候可能会发现`gdb`报错：`Signal: SIGSEGV (Segmentation fault)`。解决办法是，创建用户家目录创建`.gdbinit`，内容如下：
        
        ```
        handle SIGSEGV pass noprint nostop
        handle SIGBUS pass noprint nostop
        ```
        
    -   如果使用的调试器是`lldb`（`MacOS`下默认），调试的时候可能会发现`lldb`报错：`SIGSEGV (signal SIGSEGV)`。解决办法是，在用户家目录创建`.lldbinit`，内容如下：
        
        ```
        break set -n main -C "process handle --pass true --stop false SIGSEGV" -C "process handle --pass true --stop false SIGBUS"
        ```
        
6.  配合`File Watchers`插件自动更新`Compilation Database`（可选）
    
    如果修改了项目代码，需要重新生成`Compilation Database`，一般情况需要重新构建才可以。
    
    如果不想每次都重新手动构建，可以使用`Files Watcher`插件来实现监听变更自动重新生成。详见：[https://www.jetbrains.com/help/clion/managing-makefile-projects.html#filewatcher-plugin](https://links.jianshu.com/go?to=https%3A%2F%2Fwww.jetbrains.com%2Fhelp%2Fclion%2Fmanaging-makefile-projects.html%23filewatcher-plugin)
    

## OpenJDK 8

### 1\. 下载OpenJDK8源代码

我们这里选择从`AdoptOpenJDK`的`GitHub`仓库下载源代码。

```
# 首先需要安装git工具（如果没有的话）
sudo apt-get install git
# 克隆项目代码，可能耗时较长
git clone git@github.com:AdoptOpenJDK/openjdk-jdk8u.git
```

### 2\. 安装工具链及依赖

```
# 首先需要下载安装 JDK7 作为 BootJDK
方法见上文
# 安装编译器及构建工具
sudo apt-get install gcc g++ gdb make
# 下载安装依赖包
sudo apt-get install libasound2-dev libfreetype6-dev libcups2-dev libfontconfig1-dev libxext-dev libxrender-dev libxtst-dev libxt-dev
```

### 3\. 配置及构建

```
# 首先进入 OpenJDK8 源码目录
# 配置
sh ./configure --with-debug-level=slowdebug --disable-zip-debug-info --with-target-bits=64 --with-boot-jdk=/home/jiajiawang/software/jdk/jdk1.7.0_80 --with-freetype-include=/usr/include/freetype2 --with-freetype-lib=/usr/lib/x86_64-linux-gnu/
```

`configure`参数说明：

参数

含义

\--with-debug-level

调试信息的级别，可选值有`release`,`fastdebug`,`slowdebug`

\--disable-zip-debug-info

禁止压缩调试信息，设置为true有助于调试

\--with-target-bits

选择32位或者64位，根据操作系统选择

\--with-boot-jdk

`BootJDK`的位置

\--with-freetype-include  
\--with-freetype-lib

指定`freetype`依赖位置，如果提示找不到`freetype`，需要配置这两个参数

参数的更多说明及更多参数请参考：[OpenJDK 8 Build README - Configure](https://links.jianshu.com/go?to=http%3A%2F%2Fhg.openjdk.java.net%2Fjdk8u%2Fjdk8u%2Fraw-file%2Ftip%2FREADME-builds.html%23configure)

我们可以使用两种工具来生成`Compilation Database`

#### 使用Bear工具

```
# 下载Bear工具
sudo apt-get install bear
# 构建，并使用bear工具生成Compilation Database
bear make all
```

#### 使用compiledb工具

```
# 需要保证有python环境
# 安装pip
sudo apt-get install python-pip
# pip安装 compiledb
pip install compiledb
# 构建，并使用compiledb工具生成Compilation Database
compiledb make all
```

更多`make`的`target`请参考[OpenJDK 8 Build README - Make](https://links.jianshu.com/go?to=http%3A%2F%2Fhg.openjdk.java.net%2Fjdk8u%2Fjdk8u%2Fraw-file%2Ftip%2FREADME-builds.html%23make)

如果没有报错，完成之后应该可以在`./build/linux-x86_64-normal-server-slowdebug/jdk`目录下找到编译之后的`JDK`，验证一下是否成功

```
~: cd build/linux-x86_64-normal-server-slowdebug/jdk/bin/
~: ./java -version
openjdk version "1.8.0-internal-debug"
......
```

同时项目根目录下应该同时有一个`compile_commands.json`文件，并且不为空。

### 4\. 导入CLion并调试

步骤参见上文

## OpenJDK 11

### 1\. 下载OpenJDK11源代码

```
# 首先需要安装git工具（如果没有的话）
sudo apt-get install git
# 克隆项目代码，可能耗时较长
git clone git@github.com:AdoptOpenJDK/openjdk-jdk11u.git
```

### 2\. 安装工具链及依赖

```
# 需要下载安装 JDK10 作为 BootJDK
方法见上文
# 安装编译器及构建工具
sudo apt-get install gcc g++ gdb make autoconf
# 下载安装依赖包
sudo apt-get install libfreetype6-dev libcups2-dev
sudo apt-get install libx11-dev libxext-dev libxrender-dev libxrandr-dev libxtst-dev libxt-dev
sudo apt-get install libasound2-dev libffi-dev
sudo apt-get install libfontconfig1-dev
```

### 3\. 配置及构建

```
# 配置
sh ./configure --with-debug-level=slowdebug --with-native-debug-symbols=internal --with-target-bits=64 --with-boot-jdk=/home/jiajiawang/software/jdk/jdk-10.0.2
```

> 如果不成功，一般可能是缺少部分依赖，根据脚本给出的提示安装相应依赖即可。

`configure`参数说明：

参数

含义

\--with-debug-level

调试信息的级别，可选值有`release`,`fastdebug`,`slowdebug`

\--with-native-debug-symbols

指定如何构建`debug symbol`，可选值有`none`,`internal`, `external`, `zipped`，设置为`internal`可以更好地调试

\--with-target-bits

选择32位或者64位，根据操作系统选择

\--with-boot-jdk

`bootjdk`位置

\--with-freetype-include  
\--with-freetype-lib

指定`freetype`依赖位置。如果提示找不到`freetype`，需要配置这两个参数

参数的更多说明及更多参数请参考：[OpenJDK 11 Build README - Configure](https://links.jianshu.com/go?to=http%3A%2F%2Fhg.openjdk.java.net%2Fjdk%2Fjdk11%2Fraw-file%2Ftip%2Fdoc%2Fbuilding.html%23running-configure)

```
# 生成Compilation Database
make compile-commands
# 构建
make all
```

更多`make`的`target`请参考[OpenJDK 11 Build README - Make](https://links.jianshu.com/go?to=http%3A%2F%2Fhg.openjdk.java.net%2Fjdk%2Fjdk11%2Fraw-file%2Ftip%2Fdoc%2Fbuilding.html%23running-make)

完成之后应该可以在`./build/linux-x86_64-normal-server-slowdebug/jdk`目录下找到编译之后的jdk，验证一下是否成功

```
~: cd build/linux-x86_64-normal-server-slowdebug/jdk/bin/
~: ./java -version
openjdk version "11.0.8-internal" 2020-07-14
......
```

同时在`/build/linux-x86_64-normal-server-slowdebug/`目录下会有`compile_commands.json`文件，并且不为空。

### 4\. 导入CLion并调试

大致步骤与前文介绍相同。  
需要注意的是在选择`/build/linux-x86_64-normal-server-slowdebug/`目录下的`compile_commands.json`文件导入项目之后需要更改项目的根目录为`源码的根目录`（这里也就是`openjdk-11u`目录）。点击菜单`Tools | Compilation Database | Change Project Root`，选择`源码根目录`（`openjdk-11u`），等待重新索引完成。  

![](https://upload-images.jianshu.io/upload_images/23289123-e69c5d02737941f8.gif?imageMogr2/auto-orient/strip|imageView2/2/w/1200/format/webp)

image

## OpenJDK 8

> `MacOS 10.15.5`环境下构建`OpenJDK`与`Linux`下步骤相似，但是由于`OpenJDK 8`版本较老，其中一些依赖在`MacOS 10.15.5`版本中已经无法找到，导致在配置构建过程中会出现各种问题，本文给出了一种比较简单方便的办法。

### 1\. 下载OpenJDK8源代码

我们选择从`AdoptOpenJDK`的`github`仓库下载源代码

```
git clone git@github.com:AdoptOpenJDK/openjdk-jdk8u.git
```

### 2\. 安装工具链及依赖

1.  首先需要下载安装`jdk7`作为`bootjdk`，方法见文章开头。
2.  安装`HomeBrew`。`Homebrew`是一款`MacOS`系统上的软件包管理系统。
    
    ```
    /bin/bash -c "$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/master/install.sh)"
    ```
    
3.  安装`Xcode`。可以直接在`App Store`搜索安装，也可以在[https://developer.apple.com/download/more/](https://links.jianshu.com/go?to=https%3A%2F%2Fdeveloper.apple.com%2Fdownload%2Fmore%2F)下载离线安装包安装。可以使用`xcodebuild`命令验证是否安装成功，正确输出版本号则表明正确安装。 如果是离线安装的话，可能会报`xcode-select: error: tool 'xcodebuild' requires Xcode, but active developer directory '/Library/Developer/CommandLineTools' is a command line tools instance`错误，这是因为`xcodebuild`找不到新安装的`Xcode`，只需要执行下面这个命令即可。
    
    ```
    sudo xcode-select --switch /Applications/Xcode.app/Contents/Developer/
    ```
    
4.  安装编译工具
    
    ```
    # 安装构建工具
    brew install make
    ```
    
5.  安装所需依赖
    
    ```
    # 安装freetype
    brew install freetype
    ```
    

### 3\. 配置及构建

#### 3.1 修改代码

如果不对代码进行修改，在执行`configure`的时候，会报各种错误。网上也有很多针对每种错误来修改源文件来解决报错的方法， 这里给出一种更为方便的方法。  
`GitHub`上的[stooke/jdk8u-xcode10](https://links.jianshu.com/go?to=https%3A%2F%2Fgithub.com%2Fstooke%2Fjdk8u-xcode10)这个项目，提供了对`openjdk8`的代码进行修改的`patch`文件，对代码修改之后，就可以正常地`configure`。

```
git clone git@github.com:stooke/jdk8u-xcode10.git
```

这个项目本身提供了脚本来完成下载源码、下载依赖、代码修改、配置、编译、测试等工作，但我们这里只使用其中的修改代码这部分。原项目中的`README`可能已经过时了，通过查看其源代码中的`build8.sh`文件，能够大概了解其中的执行逻辑，据此我将其中的修改`OpenJDK8`代码的部分抽取出来，简化如下：

新建一个`shell`脚本`patch.sh`，内容如下：

```
#!/bin/bash
# JDK8 源码所在目录
JDK_DIR=`pwd`
# 下载的jdk8u-xcode10项目所在目录
PATCH_DIR="$(dirname $JDK_DIR)/jdk8u-xcode10"
PATCH_DIR="$PATCH_DIR/jdk8u-patch"

applypatch() {
    cd "$JDK_DIR/$1"
    echo "applying $1 $2"
    patch -p1 <$2
}

patchjdkbuild() {
    echo "patch jdk"
    # JDK-8019470: Changes needed to compile JDK 8 on MacOS with clang compiler
    applypatch . "$PATCH_DIR/jdk8u-8019470.patch"

    # JDK-8152545: Use preprocessor instead of compiling a program to generate native nio constants
    # (fixes genSocketOptionRegistry build error on 10.8)
    applypatch jdk "$PATCH_DIR/jdk8u-jdk-8152545.patch"

    # fix WARNINGS_ARE_ERRORS handling
    applypatch hotspot "$PATCH_DIR/jdk8u-hotspot-8241285.patch"

    # fix some help messages and Xcode version checks
    applypatch . "$PATCH_DIR/jdk8u-buildfix1.patch"
    # use correct C++ standard library
    #applypatch . "$PATCH_DIR/jdk8u-libcxxfix.patch"
    # misc clang-specific cleanup
    applypatch . "$PATCH_DIR/jdk8u-buildfix2.patch"

    # misc clang-specific cleanup; doesn't apply cleanly on top of 8019470 
    # (use -g1 for fastdebug builds)
    #applypatch . "$PATCH_DIR/jdk8u-buildfix2a.patch"

    # fix for clang crash if base has non-virtual destructor
    applypatch hotspot "$PATCH_DIR/jdk8u-hotspot-8244878.patch"
    
    applypatch hotspot "$PATCH_DIR/jdk8u-hotspot-mac.patch"

    # libosxapp.dylib fails to build on Mac OS 10.9 with clang
    applypatch jdk     "$PATCH_DIR/jdk8u-jdk-8043646.patch"

    applypatch jdk     "$PATCH_DIR/jdk8u-jdk-minversion.patch"
}
patchjdkbuild
```

然后执行这个脚本：

#### 3.2 配置

```
sh ./configure MAKE=/usr/bin/make --with-toolchain-type=clang --with-debug-level=slowdebug --disable-zip-debug-info --with-target-bits=64 --with-boot-jdk=/Users/jiajiawang/Software/jdk/jdk1.7.0_80.jdk/Contents/Home/ --with-freetype-include=/usr/local/Cellar/freetype/2.10.2/include/freetype2 --with-freetype-lib=/usr/local/Cellar/freetype/2.10.2/lib/
```

`MAKE=/usr/bin/make`是可选的，如果下文使用`Bear`来生成`Compilation Database`，可以不加这个参数。

`configure`部分参数说明如下：

参数

含义

\--with-toolchain-type

使用的工具链类型，取值有`gcc`，`clang`等，可以使用`--help`来查看所有可选值

\--with-debug-level

调试信息的级别，可选值有`release`,`fastdebug`,`slowdebug`

\--disable-zip-debug-info

禁止压缩调试信息，设置为true可以更好地调试

\--with-target-bits

选择32位或者64位，根据操作系统选择

\--with-boot-jdk

`bootjdk`位置

\--with-freetype-include  
\--with-freetype-lib

指定`freetype`依赖位置，在执行`configure`时如果找不到`freetype`，需要指定这两个参数

参数的更多说明及更多参数请参考：[OpenJDK 8 Build README - Configure](https://links.jianshu.com/go?to=http%3A%2F%2Fhg.openjdk.java.net%2Fjdk8u%2Fjdk8u%2Fraw-file%2Ftip%2FREADME-builds.html%23configure)

#### 3.3 构建

同样我们仍然可以使用`bear`或者`compiledb`来生成`Compilation Database`，但是使用`bear`过程中遇到一些问题比较繁琐，推荐使用`compiledb`。

##### 使用compiledb

```
# 1. 请先确保安装了Python及pip
方法请自行搜索
# 2. 安装compiledb工具
pip install compiledb
# 3. 构建，并使用compiledb工具生成Compilation Database
compiledb make all COMPILER_WARNINGS_FATAL=false
# 可以使用 LOG=debug 选项来输出更为详细的信息
# compiledb make all LOG=debug COMPILER_WARNINGS_FATAL=false
```

使用`compiledb`可能会存在的问题是生成的`compile_commands.json`文件中所有项的`directory`项都是项目的根目录，导致一个头文件无法找到，导入`CLion`时报错，代码定义及跳转无效。原因是在`MacOS`平台，编译构建使用的可能是`gmake`，而`compiledb`对`gmake`切换路径的操作无法捕捉到，导致路径全部都是根目录。解决办法就是显示指定使用`make`而不是`gmake`，指定方法就是上一小节中在`Configure`时指定的`MAKE=/usr/bin/make`参数。

##### 使用bear

上面我们使用`compiledb`工具来生成`Compilation Database`，同样也可以使用`bear`工具来生成：

```
# 1. 首先需要关闭SIP，否则生成的 Compilation Database 可能会是空的
关闭SIP的方法请自行搜索
# 2. 安装bear
brew install bear
# 3. 构建，并使用bear工具生成Compilation Database
bear make all COMPILER_WARNINGS_FATAL=false
# 可以使用 LOG=debug 选项来输出更为详细的信息
# compiledb make all LOG=debug COMPILER_WARNINGS_FATAL=false
```

如果遇到执行命令遇到报如下错误：

```
/Applications/Xcode.app/Contents/Developer/Toolchains/XcodeDefault.xctoolchain/usr/bin/../include/c++/v1/stdio.h:107:15: fatal error: 'stdio.h' file not found"
```

可以尝试执行如下命令：

```
sudo mount -uw /
sudo cp -R /Library/Developer/CommandLineTools/SDKs/MacOSX.sdk/usr/include /usr
```

更多`make`的`target`请参考[OpenJDK 8 Build README - Make](https://links.jianshu.com/go?to=http%3A%2F%2Fhg.openjdk.java.net%2Fjdk8u%2Fjdk8u%2Fraw-file%2Ftip%2FREADME-builds.html%23make)

如果没有报错，完成之后应该可以在`./build/macosx-x86_64-normal-server-slowdebug/jdk`目录下找到编译之后的`JDK`，验证一下是否成功

```
~: cd build/macosx-x86_64-normal-server-slowdebug/jdk/bin/
~: ./java -version
openjdk version "1.8.0-internal-debug"
......
```

同时项目根目录下应该同时有一个`compile_commands.json`文件，并且不为空。

### 4\. 导入CLion并调试

需要注意在创建`Custom Build Targets`时，新建的用于`Build`的`External Tool`中`make`的参数中也要加上`COMPILER_WARNINGS_FATAL=false`

## OpenJDK 11

### 1\. 下载OpenJDK11源代码

```
git clone git@github.com:AdoptOpenJDK/openjdk-jdk11u.git
```

### 2\. 安装工具链及依赖

1.  首先需要下载安装`jdk10`作为`bootjdk`，方法见文章开头。
    
2.  安装`HomeBrew`。`Homebrew`是一款`MacOS`系统上的软件包管理系统。
    
    ```
    /bin/bash -c "$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/master/install.sh)"
    ```
    
3.  安装`Xcode`。可以直接在`App Store`搜索`Xcode`安装，也可以在[https://developer.apple.com/download/more/](https://links.jianshu.com/go?to=https%3A%2F%2Fdeveloper.apple.com%2Fdownload%2Fmore%2F)下载离线安装包安装。可以使用`xcodebuild`命令验证是否安装成功，正确输出版本号则表明正确安装。
    
    如果是离线安装的话，可能会报`xcode-select: error: tool 'xcodebuild' requires Xcode, but active developer directory '/Library/Developer/CommandLineTools' is a command line tools instance`错误，这是因为`xcodebuild`找不到新安装的`Xcode`，只需要执行下面这个命令即可。
    
    ```
    sudo xcode-select --switch /Applications/Xcode.app/Contents/Developer/
    ```
    
4.  安装编译工具
    
    ```
    brew install autoconf
    brew install make
    ```
    
5.  安装所需依赖
    
    ```
    # 安装freetype
    brew install freetype
    ```
    

### 3\. 配置及编译

```
# 配置
sh ./configure --with-debug-level=slowdebug --with-native-debug-symbols=internal --with-target-bits=64 --with-boot-jdk=/Users/jiajiawang/Software/jdk/jdk-10.0.2+13/Contents/Home
```

`configure`参数说明：

参数

含义

\--with-debug-level

调试信息的级别，可选值有`release`,`fastdebug`,`slowdebug`

\--with-native-debug-symbols

指定如何构建`debug symbol`，可选值有`none`,`internal`, `external`, `zipped`，设置为`internal`可以更好地调试

\--with-target-bits

选择32位或者64位，根据操作系统选择

\--with-boot-jdk

`bootjdk`位置

\--with-freetype-include  
\--with-freetype-lib

指定`freetype`依赖位置，在执行`configure`时如果找不到`freetype`，需要指定这两个参数

参数的更多说明及更多参数请参考：[OpenJDK 11 Build README - Configure](https://links.jianshu.com/go?to=http%3A%2F%2Fhg.openjdk.java.net%2Fjdk%2Fjdk11%2Fraw-file%2Ftip%2Fdoc%2Fbuilding.html%23running-configure)

```
# 生成Compilation Database
make compile-commands
# 构建
make all
```

完成之后，应该在`/build/linux-x86_64-normal-server-slowdebug/`目录下会有`compile_commands.json`文件。

更多`make`的`target`请参考[OpenJDK 11 Build README - Make](https://links.jianshu.com/go?to=http%3A%2F%2Fhg.openjdk.java.net%2Fjdk%2Fjdk11%2Fraw-file%2Ftip%2Fdoc%2Fbuilding.html%23running-make)

完成之后应该可以在`./build/macosx-x86_64-normal-server-slowdebug/jdk`目录下找到编译之后的jdk，验证一下是否成功

```
~: cd build/macosx-x86_64-normal-server-slowdebug/jdk/bin/
~: ./java -version
openjdk version "11.0.8-internal" 2020-07-14
......
```

同时在`./build/macosx-x86_64-normal-server-slowdebug/`目录下会有`compile_commands.json`文件，并且不为空。

### 4\. 导入CLion并调试

大致步骤与前文介绍相同。  
需要注意的是在选择`/build/macosx-x86_64-normal-server-slowdebug/`目录下的`compile_commands.json`文件导入项目之后需要更改项目的根目录为`源码的根目录`(这里也就是`openjdk-11u`目录)。点击菜单`Tools | Compilation Database | Change Project Root`，将项目的根目录设置为`源码根目录`(`openjdk-11u`)，等待重新索引完成。  

![](https://upload-images.jianshu.io/upload_images/23289123-9da201e0c8c93d31.gif?imageMogr2/auto-orient/strip|imageView2/2/w/1200/format/webp)

image

1.  [OpenJDK](https://links.jianshu.com/go?to=https%3A%2F%2Fopenjdk.java.net%2F)
2.  [OpenJDK 8 Build README](https://links.jianshu.com/go?to=https%3A%2F%2Fhg.openjdk.java.net%2Fjdk8u%2Fjdk8u%2Fraw-file%2Ftip%2FREADME-builds.html)
3.  [OpenJDK 11 Build README](https://links.jianshu.com/go?to=https%3A%2F%2Fhg.openjdk.java.net%2Fjdk%2Fjdk11%2Fraw-file%2Ftip%2Fdoc%2Fbuilding.html)
4.  [rizsotto/Bear: Bear is a tool that generates a compilation database for clang tooling](https://links.jianshu.com/go?to=https%3A%2F%2Fgithub.com%2Frizsotto%2FBear)
5.  [nickdiego/compiledb: Tool for generating Clang's JSON Compilation Database files for make-based build systems.](https://links.jianshu.com/go?to=https%3A%2F%2Fgithub.com%2Fnickdiego%2Fcompiledb)
6.  [Managing Makefile Projects](https://links.jianshu.com/go?to=https%3A%2F%2Fwww.jetbrains.com%2Fhelp%2Fclion%2Fmanaging-makefile-projects.html)
7.  [Dealing with Makefile Projects in CLion: Status Update](https://links.jianshu.com/go?to=https%3A%2F%2Fblog.jetbrains.com%2Fclion%2F2020%2F02%2Fdealing-with-makefiles%2F)
8.  [Tips & Tricks: Develop OpenJDK in CLion with Pleasure](https://links.jianshu.com/go?to=https%3A%2F%2Fblog.jetbrains.com%2Fclion%2F2020%2F03%2Fopenjdk-with-clion%2F)
9.  [Debugging OpenJDK - DZone DevOps](https://links.jianshu.com/go?to=https%3A%2F%2Fdzone.com%2Farticles%2Fdebugging-openjdk)
10.  [Compilation database — Sarcasm notebook](https://links.jianshu.com/go?to=https%3A%2F%2Fsarcasm.github.io%2Fnotes%2Fdev%2Fcompilation-database.html)
11.  [More Software Downloads - Apple Developer](https://links.jianshu.com/go?to=https%3A%2F%2Fdeveloper.apple.com%2Fdownload%2Fmore%2F)
12.  [stooke/jdk8u-xcode10: How to compile JDK 8u with Xcode 9, 10 or 11 on macOS.](https://links.jianshu.com/go?to=https%3A%2F%2Fgithub.com%2Fstooke%2Fjdk8u-xcode10)
