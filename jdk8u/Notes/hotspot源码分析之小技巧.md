---
source: https://www.dazhuanlan.com/shiningx11/topics/1359183
---
## [](https://www.dazhuanlan.com/shiningx11/topics/1359183#%E5%BC%95%E8%A8%80 "引言")引言

本文所指 “小技巧” 并非分析 hotspot 源码的小技巧，而是 hotspot 源码中使用到的开发技巧。

## [](https://www.dazhuanlan.com/shiningx11/topics/1359183#%E4%B8%80%E4%B8%AA%E8%A1%A8%E7%A4%BA%E5%AF%84%E5%AD%98%E5%99%A8%E7%9A%84c-%E7%B1%BB "一个表示寄存器的c++类")一个表示寄存器的 c++ 类

如何用一个 c++ 类来表示处理器中的寄存器。寄存器通常有一个编码（例如 x64 上 rcx 寄存器的编码就是 001），还有获取其名字的方法`name`(例如 rcx 寄存器的名字就是`rcx`）。

通常情况下，根据如上需求，我们很可能会写如下的代码：

<table><tbody><tr><td><pre><span>1</span><br><span>2</span><br><span>3</span><br><span>4</span><br><span>5</span><br><span>6</span><br><span>7</span><br><span>8</span><br></pre></td><td><pre><span><span>class</span> Register {</span><br><span><span>public</span>:</span><br><span>  Register(<span>int</span> encoding): _encoding(encoding) {}</span><br><span>  <span><span>int</span> <span>()</span> </span>{<span>return</span> _encoding;}</span><br><span>  <span><span>const</span> <span>char</span>* <span>name</span><span>()</span></span>;</span><br><span><span>private</span>:</span><br><span>  <span>int</span> _encoding;</span><br><span>};</span><br></pre></td></tr></tbody></table>

然后写如下的代码创建及使用寄存器  

<table><tbody><tr><td><pre><span>1</span><br><span>2</span><br></pre></td><td><pre><span>Register* rcx = <span>new</span> Register(<span>1</span>);</span><br><span><span>printf</span>(<span>"%s, %d"</span>, rcx-&gt;name(), rcx-&gt;encoding());</span><br></pre></td></tr></tbody></table>

但 hotspot 却使用了一种让人很是费解的方法来定义和使用 Register。

## [](https://www.dazhuanlan.com/shiningx11/topics/1359183#hotspot%E7%9A%84Register%E5%AE%9E%E7%8E%B0 "hotspot的Register实现")hotspot 的[Register 实现](http://github.com/leafinwind/hotspot-study/blob/master/main.cpp)

hotspot 的 Register 定义大致如下：  

<table><tbody><tr><td><pre><span>1</span><br><span>2</span><br><span>3</span><br><span>4</span><br><span>5</span><br><span>6</span><br><span>7</span><br><span>8</span><br><span>9</span><br><span>10</span><br><span>11</span><br><span>12</span><br><span>13</span><br><span>14</span><br><span>15</span><br></pre></td><td><pre><span><span>typedef</span> RegisterImpl* Register;</span><br><span></span><br><span><span>class</span> RegisterImpl {</span><br><span><span>public</span>:</span><br><span>  <span><span>int</span> <span>()</span> <span>const</span> </span>{ <span>return</span> (<span>int</span>)<span>this</span>; }</span><br><span>  <span><span>const</span> <span>char</span>* <span>name</span><span>()</span> <span>const</span></span>;</span><br><span>};</span><br><span></span><br><span><span>const</span> <span>char</span>* RegisterImpl::name() <span>const</span> {</span><br><span>  <span>const</span> <span>char</span>* names[<span>16</span>] = {</span><br><span>    <span>"rax"</span>, <span>"rcx"</span>, <span>"rdx"</span>, <span>"rbx"</span>, <span>"rsp"</span>, <span>"rbp"</span>, <span>"rsi"</span>, <span>"rdi"</span>,</span><br><span>    <span>"r8"</span>,  <span>"r9"</span>,  <span>"r10"</span>, <span>"r11"</span>, <span>"r12"</span>, <span>"r13"</span>, <span>"r14"</span>, <span>"r15"</span></span><br><span>  };</span><br><span>  <span>return</span> names[encoding()];</span><br><span>}</span><br></pre></td></tr></tbody></table>

创建和使用寄存器的代码如下：  

<table><tbody><tr><td><pre><span>1</span><br><span>2</span><br></pre></td><td><pre><span>Register rcx = <span>1</span>;</span><br><span><span>printf</span>(<span>"%s, %d"</span>, rcx-&gt;name(), rcx-&gt;encoding());</span><br></pre></td></tr></tbody></table>

## [](https://www.dazhuanlan.com/shiningx11/topics/1359183#hotspot%E6%BA%90%E7%A0%81%E5%88%86%E6%9E%90 "hotspot源码分析")hotspot 源码分析

hotspot 源码让人费解的地方主要有三处：

-   首先是其`RegisterImpl`定义中竟然没有任何字段，其`encoding()`方法只是简单地返回了`this`指针。
-   其次是使用 Register 时竟然出现了`Register rcx = 1；`这样的代码，既然 Register 是一个 RegisterImpl 指针，怎么能把一个 int 值直接赋给一个指针呢？
-   最后就是既然`rcx`就是`1`，为什么还能有`rcx->name()`这样的调用呢？

其实，问题的关键就是`rcx`作为一个值为`1`的指针怎么能对其调用`name`方法，按照《深入探索 c++ 对象模型》一书的介绍，`rcx->name()`这样的调用要先通过`rcx`指针找到`RegisterImpl`类的虚表，然后在虚表中查找`name`方法，最后再以`rcx`为参数调用该方法。现在既然`rcx`为`1`，那么查找虚表的过程肯定会引发段为例错误啊！

通过查看调用过程的汇编代码，发现并没有基于`rcx`指针的虚表查找过程。原来，上述查虚表的过程针对的仅仅是`c++`的虚方法，而`name`并不是虚方法，因此不需要这个过程，针对`name`的调用是编译时静态绑定的，`rcx`作为一个指针，它在`name`调用中的意义仅仅是作为该调用的第一个参数（这是`c++`的规则），因此，调用`rcx->name()`其实就是调用`RegisterImpl::name(1)`，该调用可以正确的返回`“rcx”`。

同样的道理，也就不难理解`encoding`方法的实现了。

## [](https://www.dazhuanlan.com/shiningx11/topics/1359183#%E5%B0%8F%E7%BB%93 "小结")小结

hotspot 的 Register 实现方法其实给那些**只需要一个字段的且不需要虚方法**的类提供了一种新的实现方法，即直接使用隐含的`this`指针作为数据字段，而不再显示地去定义一个数据字段。使用这种类的方法是直接对类指针赋值，然后调用其方法，唯一限制是不能调用虚方法。此外，直接给指针赋值避免了一次构造函数的调用开销，因此代码虽然让人费解，但更加高效。

hotspot 源码中大量使用了上述技巧，例如`VMReg`、`NativeCall`、….。

相信我们自己在今后的大型工程开发过程中也可以有意识地使用上述技巧。
