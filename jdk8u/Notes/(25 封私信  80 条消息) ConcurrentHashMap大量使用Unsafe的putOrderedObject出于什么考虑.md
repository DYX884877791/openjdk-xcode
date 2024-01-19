---
source: https://www.zhihu.com/question/60888757
---
以下源码摘自JDK 12 [open jdk](https://www.zhihu.com/search?q=open%20jdk&search_source=Entity&hybrid_search_source=Entity&hybrid_search_extra=%7B%22sourceType%22%3A%22answer%22%2C%22sourceId%22%3A2125171030%7D) ，从JDK 1.8 的源码到12 可以看到 实现居然是一模一样的？您给出的这个结论是否有源码作为参考？

```
public void putOrderedObject(Object o, long offset, Object x) { theInternalUnsafe.putReferenceRelease(o, offset, x); }
 public void putObjectVolatile(Object o, long offset, Object x) { theInternalUnsafe.putReferenceVolatile(o, offset, x); }
 public final void putReferenceRelease(Object o, long offset, Object x) { putReferenceVolatile(o, offset, x); }
```

// 两者均调用了putReferenceVolatile

```
 public native void putReferenceVolatile(Object o, long offset, Object x);  UNSAFE_ENTRY(void, Unsafe_PutReferenceVolatile(JNIEnv *env, jobject unsafe, jobject obj, jlong offset, jobject x_h)) {   oop x = JNIHandles::resolve(x_h);   oop p = JNIHandles::resolve(obj);   assert_field_offset_sane(p, offset);   HeapAccess<MO_SEQ_CST | ON_UNKNOWN_OOP_REF>::oop_store_at(p, offset, x); } UNSAFE_END
```

// 满足 MO_SEQ_CST 特性，看这里的特性介绍：MO_SEQ_CST is equivalent to JMM volatile. 对于store 存储操作如下：

MO_SEQ_CST: Sequentially consistent stores. - The stores are observed in the same order by MO_SEQ_CST loads on other processors - Preceding loads and stores in program order are not reordered with subsequent loads and stores in program order. - Guarantees from releasing stores hold.

我们从上面看到，最终不管是putOrderedObject putObjectVolatile方法最终都是调用putReferenceVolatile方法（不过前面的兄台，居然面对源码说：这只是源码，而不是最终执行代码，我笑了，面对这种精怪，一般我都是：您说得对~），该源码出处：JDK 12 ， JDK 15的[Unsafe.java](https://link.zhihu.com/?target=http%3A//unsafe.java/) src\java.base\share\classes\jdk\internal\misc 和 [Unsafe.java](https://link.zhihu.com/?target=http%3A//unsafe.java/) src\jdk.unsupported\share\classes\sun\misc 由于原来我们用的Unsafe已经不支持，所以在jdk.unsupported目录下，该unsafe将会间接 调用 java.base的unsafe来实现。那么我们现在直接来看unsafe.cpp 该类是对于Unsafe.java文件的JNI实现。源码如下：我直接给出注释和调用链：

```
UNSAFE_ENTRY(void, Unsafe_PutReferenceVolatile(JNIEnv *env, jobject unsafe, jobject obj, jlong offset, jobject x_h)) {
  oop x = JNIHandles::resolve(x_h);
  oop p = JNIHandles::resolve(obj);
  assert_field_offset_sane(p, offset);
  // 最终调用oop_store_at方法完成store过程，读者这里注意关注 MO_SEQ_CST 这个修饰，前面已经给出修饰的含义，后面我们会详细使用
  HeapAccess<MO_SEQ_CST | ON_UNKNOWN_OOP_REF>::oop_store_at(p, offset, x);
} UNSAFE_END

template <DecoratorSet decorators, typename T>
  inline void store_at(oop base, ptrdiff_t offset, T value) {
    verify_types<decorators, T>();
    typedef typename Decay<T>::type DecayedT;
    DecayedT decayed_value = value;
    const DecoratorSet expanded_decorators = DecoratorFixup<decorators |
                                             (HasDecorator<decorators, INTERNAL_VALUE_IS_OOP>::value ?
                                              INTERNAL_CONVERT_COMPRESSED_OOP : DECORATORS_NONE)>::value;
    // 最终调用store_at完成存储
    PreRuntimeDispatch::store_at<expanded_decorators>(base, offset, decayed_value);
  }

 template <DecoratorSet decorators, typename T>
    inline static typename EnableIf<
      HasDecorator<decorators, AS_RAW>::value>::type
    store_at(oop base, ptrdiff_t offset, T value) {
      // 最后调用 store方法完成store过程，注意：我们这里始终带着decorators修饰变量走
      store<decorators>(field_addr(base, offset), value);
 }

template <DecoratorSet decorators, typename T>
    inline static typename EnableIf<
      HasDecorator<decorators, AS_RAW>::value && CanHardwireRaw<decorators>::value>::type
    store(void* addr, T value) {
      typedef RawAccessBarrier<decorators & RAW_DECORATOR_MASK> Raw;
      if (HasDecorator<decorators, INTERNAL_VALUE_IS_OOP>::value) {
        // 我们看这个方法即可，存储oop对象，因为我们使用的是put Ordered/Volatile Object
        Raw::oop_store(addr, value);
      } else {
        Raw::store(addr, value);
      }
}

template <DecoratorSet decorators>
template <DecoratorSet ds, typename T>
inline typename EnableIf<
  HasDecorator<ds, MO_SEQ_CST>::value>::type
RawAccessBarrier<decorators>::store_internal(void* addr, T value) {
  // 最终调用该方法完成，注意上面的 MO_SEQ_CST，和前面保持一致
  Atomic::release_store_fence(reinterpret_cast<volatile T*>(addr), value);
}

template <typename D, typename T>
inline void Atomic::release_store_fence(volatile D* p, T v) {
  StoreImpl<D, T, PlatformOrderedStore<sizeof(D), RELEASE_X_FENCE> >()(p, v);
}

// 注意我们这里选取linux  X86 下的源码来分析，读者可以查看源码看sparc或者其他平台的
template<>
struct Atomic::PlatformOrderedStore<8, RELEASE_X_FENCE>
{
  template <typename T>
  void operator()(volatile T* p, T v) const {
    __asm__ volatile (  "xchgq (%2), %0" // 注意这个指令 xchgq
                      : "=r" (v)
                      : "0" (v), "r" (p)
                      : "memory");
  }
};
指令含义如下（摘自intel 开发手册）：
The XCHG (exchange) instruction swaps the contents of two operands. This instruction takes the place of three 
MOV instructions and does not require a temporary location to save the contents of one operand location while the 
other is being loaded. When a memory operand is used with the XCHG instruction, the processor’s LOCK signal is 
automatically asserted.（注意这里：不需要lock前缀，因为xchg指令会自动声明LOCK，至于是锁缓存行还是总线根据不同平台而定）。
```

总结：

我给出答案都会给出一条完整的链路，程序界不是神学，一定是一条完整的证据链，通过源码和官方手册还有输出结果，我觉得连源码都不信的兄弟，可能是因为达克效应太严重。毕竟：talk is cheap ， show me the code。我们可以通过：-XX:+UnlockDiagnosticVMOption -XX:+PrintAssembly -Xcomp -XX:+LogCompilation -XX:LogFile=jit.log 参数来查看JIT的编译汇编代码，因为volatile关键字可以进行解析执行，所以JIT中可以观察到store的过程：lock addl rsp,0 对栈顶执行空操作，该指令相当于指令屏障，这里是和linux内核相违背的，Hotspot中生成这个指令的性能高于mfence，但是linux内核说 lock addl rsp,0 性能低于 mfence。我觉得是不是让两边人互相battle？我通过实验证明：lock addl rsp,0 性能高，这是为何呢？在现代CPU中由于MESI协议的出现，我们可以直接操作缓存行，而不是总线，所以当缓存行中数据存在时，性能高于mfence，因为mfence指令除了会刷新MOB（CPU中内存顺序buffer，在intel中我们只有store buffer会出现指令乱序现象，事实上根本不是执行乱序，因为ROB（指令执行顺序）的存在，读者感兴趣的话可以看intel CPU架构去学习），还会将[分支预测](https://www.zhihu.com/search?q=%E5%88%86%E6%94%AF%E9%A2%84%E6%B5%8B&search_source=Entity&hybrid_search_source=Entity&hybrid_search_extra=%7B%22sourceType%22%3A%22answer%22%2C%22sourceId%22%3A2125171030%7D)，指令预取和预译码的缓存全部刷新并且会导致指令流水线清空，这将会导致性能下降。所以lock addl rsp,0 性能高与mfence指令，读者可以自行写c代码时间。好久没写回复了，但是看到一些没有源码的争论着实看不下去，所以给出自己的见解，至于老哥在源码面前还说：不是执行代码的，那么我建议您在去新公司工作从git上拉去代码，也别看了，你咋知道那是运行代码对吧？顺带推荐一下我即将出版的书：《深入理解Java高并发》，该书已通过清华大学出版社的所有流程，十月左右发售，里面详细介绍了Java到Hotspot、Glibc的所有与多线程相关的框架原理和CPU底层知识。感谢大家的耐心阅读。本人特别喜欢研究底层源码，志同道合的道友可以加我微信：18510746130，来一起交流，同时附上B站的链接：[哈士奇-柏羲的个人空间_哔哩哔哩_Bilibili](https://link.zhihu.com/?target=https%3A//space.bilibili.com/232459430)，里面有我自己录制的一些视频和文章，欢迎大家前来探讨。

==========================================================

上面或许不知道C++的模板类的特化给我反应很难看懂，那位兄台又问我看没看过输出putOrderedObject和putObjectVolatile的输出，害，不相信源码，那么我现在就继续给出输出。我下载了大家常用的JDK1.8的源码，并且编译并且通过调试给出结果和运行截图吧。我们先分析下JDK1.8的代码，这个比较简单：

先看函数原型：

```
public native void    putObjectVolatile(Object o, long offset, Object x);
public native void    putOrderedObject(Object o, long offset, Object x);
```

然后我们来看unsafe.cpp对这两个函数的实现：

```
{CC"putOrderedObject",   CC"("OBJ"J"OBJ")V",         FN_PTR(Unsafe_SetOrderedObject)},
{CC"putObjectVolatile",CC"("OBJ"J"OBJ")V",  FN_PTR(Unsafe_SetObjectVolatile)},
```

可以看到在定义中，一个调用Unsafe_SetOrderedObject，一个调用Unsafe_SetObjectVolatile，我们先来看Unsafe_SetOrderedObject：

```
UNSAFE_ENTRY(void, Unsafe_SetOrderedObject(JNIEnv *env, jobject unsafe, jobject obj, jlong offset, jobject x_h))
  UnsafeWrapper("Unsafe_SetOrderedObject");
  oop x = JNIHandles::resolve(x_h);
  oop p = JNIHandles::resolve(obj);
  void* addr = index_oop_from_field_offset_long(p, offset);
  // storestore屏障
  OrderAccess::release();
  if (UseCompressedOops) {
    oop_store((narrowOop*)addr, x);
  } else {
    oop_store((oop*)addr, x);
  }
  // storeload屏障
  OrderAccess::fence();
UNSAFE_END
```

我们由此得知，对于putOrderedObject来说直接就是等同于volatile的写操作，因为volatile的写会在前面和后面分别添加storestore屏障和[storeload屏障](https://www.zhihu.com/search?q=storeload%E5%B1%8F%E9%9A%9C&search_source=Entity&hybrid_search_source=Entity&hybrid_search_extra=%7B%22sourceType%22%3A%22answer%22%2C%22sourceId%22%3A2125171030%7D)，而这两个方法对于x86来说，因为x86架构是TSO模型只存在[storeload重排序](https://www.zhihu.com/search?q=storeload%E9%87%8D%E6%8E%92%E5%BA%8F&search_source=Entity&hybrid_search_source=Entity&hybrid_search_extra=%7B%22sourceType%22%3A%22answer%22%2C%22sourceId%22%3A2125171030%7D)（MOB单元导致），所以实现如下：

```
// OrderAccess::release()的实现
inline void OrderAccess::release() {
  // 等同于空操作
  volatile jint local_dummy = 0;
}
// OrderAccess::fence();
inline void OrderAccess::fence() {
  // 只有多处理器环境下才需要使用lock前缀
  if (os::is_MP()) {
    // always use locked addl since mfence is sometimes expensive
// 在64位寄存器以r开头，32位以e开头，这里的rsp和esp使用判断宏定义的原理在此
#ifdef AMD64
    __asm__ volatile ("lock; addl $0,0(%%rsp)" : : : "cc", "memory");
#else
    __asm__ volatile ("lock; addl $0,0(%%esp)" : : : "cc", "memory");
#endif
  }
}
```

接下来我们来看Unsafe_SetObjectVolatile的实现：

```
UNSAFE_ENTRY(void, Unsafe_SetObjectVolatile(JNIEnv *env, jobject unsafe, jobject obj, jlong offset, jobject x_h))
  UnsafeWrapper("Unsafe_SetObjectVolatile");
  oop x = JNIHandles::resolve(x_h);
  oop p = JNIHandles::resolve(obj);
  void* addr = index_oop_from_field_offset_long(p, offset);
  OrderAccess::release();
  if (UseCompressedOops) {
    oop_store((narrowOop*)addr, x);
  } else {
    oop_store((oop*)addr, x);
  }
  OrderAccess::fence();
UNSAFE_END
```

实现与Unsafe_SetOrderedObject一模一样，不做过多解释。

好的，以上为JDK1.8的源码讲解，相比较来说没有使用C++的模板技巧，所以较为好懂，我编译了JDK1.8的代码，我们来看样例代码：

```
import sun.misc.Unsafe;

import java.lang.reflect.Field;

public class Demo {
    static volatile Object obj;
    static long offset;
    public static Unsafe UNSAFE;

    static {
        try {
            Field theUnsafe = Unsafe.class.getDeclaredField("theUnsafe");
            theUnsafe.setAccessible(true);
            UNSAFE = (Unsafe) theUnsafe.get(null);
            offset = UNSAFE.staticFieldOffset(Demo.class.getDeclaredField("obj"));
        } catch (Exception e) {
            throw new RuntimeException(e);
        }
    }

    public static void demo() {
         UNSAFE.putOrderedObject(Demo.class, offset, new Object());
        UNSAFE.putObjectVolatile(Demo.class, offset, new Object());
    }

    public static void main(String[] args) throws Exception {
        demo();
    }
}
```

很简单，然后我们使用javac编译，将其放到vmware 调试的eclipse路径中。字节码如下（为什么要复制出来，额，你懂得，避免杠精说我造假对吧）：

![](https://picx.zhimg.com/50/v2-2d526168a601247a963188ce835e4b50_720w.jpg?source=1940ef5c)

接下来我们开始调试，首先分别在unsafe.cpp的这两个方法中打上断点，如下图所示，成功停在断点处。

![](https://picx.zhimg.com/50/v2-b099f7fe1a4c3d1822d6a1bb2a5e9dd5_720w.jpg?source=1940ef5c)

接下来我们继续跟进OrderAccess::release()和OrderAccess::fence()方法调试过程：

![](https://pic1.zhimg.com/50/v2-96a1095c138e7ddac2bfc12c71fb375a_720w.jpg?source=1940ef5c)

![](https://picx.zhimg.com/50/v2-34effe63ce903dbf1e8c0425f4b9a6b9_720w.jpg?source=1940ef5c)

那么很明显了，我的结论没问题，那么由于Unsafe_SetObjectVolatile和Unsafe_SetOrderedObject一模一样，所以是不是证明，在Unsafe_SetObjectVolatile处断点停下来，那么就表明以下两个方法作用相同：

```
UNSAFE.putOrderedObject(ThreadDemo.class, offset, new Object());
UNSAFE.putObjectVolatile(ThreadDemo.class, offset, new Object());
```

![](https://picx.zhimg.com/50/v2-a352daad6e6af6e50108bc3294174841_720w.jpg?source=1940ef5c)

好的，如上图所示，停下来了。那么以下闹剧，言语就终结吧，源码和运行也给出了。

![](https://pic1.zhimg.com/50/v2-9af72989605b7d99b78db10f08b4934e_720w.jpg?source=1940ef5c)

![](https://pic1.zhimg.com/50/v2-8bd46e3c3af6516df3e929c1f3253b7c_720w.jpg?source=1940ef5c)

再说一下：计算机科学是一个严谨的学科，没有源码佐证，不要轻易断下结论，容易闹出笑话，同时陷入达克效应，迷失自我。最后的最后：在JDK 12以上版本，才用的是结合了lock前缀功能的xchg指令，而JDK 1.8 使用的则是明确的storestore屏障和storeload屏障。

putOrderedObject是putObjectVolatile的内存非立即可见版本；lazySet是使用Unsafe.putOrderedObject方法，这个方法在对低延迟代码是很有用的，它能够实现非堵塞的写入，这些写入不会被Java的JIT[重新排序指令](https://www.zhihu.com/search?q=%E9%87%8D%E6%96%B0%E6%8E%92%E5%BA%8F%E6%8C%87%E4%BB%A4&search_source=Entity&hybrid_search_source=Entity&hybrid_search_extra=%7B%22sourceType%22%3A%22answer%22%2C%22sourceId%22%3A181649359%7D)(instruction [reordering](https://www.zhihu.com/search?q=reordering&search_source=Entity&hybrid_search_source=Entity&hybrid_search_extra=%7B%22sourceType%22%3A%22answer%22%2C%22sourceId%22%3A181649359%7D))，这样它使用快速的存储-存储(store-store) [barrier](https://www.zhihu.com/search?q=barrier&search_source=Entity&hybrid_search_source=Entity&hybrid_search_extra=%7B%22sourceType%22%3A%22answer%22%2C%22sourceId%22%3A181649359%7D), 而不是较慢的存储-加载(store-load) barrier, 后者总是用在volatile的写操作上，这种性能提升是有代价的，虽然便宜，也就是写后结果并不会被其他线程看到，甚至是自己的线程，通常是几纳秒后被其他线程看到，这个时间比较短，所以代价可以忍受。

实名反对最高赞回答，简直胡扯。

volatile 读/写动作对应c++的memory_order_seq_cst，而putOrdered对应memory_order__release，在x86架构下是sfence，arm架构下是store_store屏障。_

ETIN说法也有点问题，“也就是写后结果并不会被其他线程看到”，并不正确。如果不会立即看到直接用[编译器](https://www.zhihu.com/search?q=%E7%BC%96%E8%AF%91%E5%99%A8&search_source=Entity&hybrid_search_source=Entity&hybrid_search_extra=%7B%22sourceType%22%3A%22answer%22%2C%22sourceId%22%3A181461594%7D)屏障就够了，对应[java9](https://www.zhihu.com/search?q=java9&search_source=Entity&hybrid_search_source=Entity&hybrid_search_extra=%7B%22sourceType%22%3A%22answer%22%2C%22sourceId%22%3A181461594%7D)的putObjectOpaque。标准库里面有一段类似的注释误导性非常强，但是他的着眼点在于写屏障应该和[读屏障](https://www.zhihu.com/search?q=%E8%AF%BB%E5%B1%8F%E9%9A%9C&search_source=Entity&hybrid_search_source=Entity&hybrid_search_extra=%7B%22sourceType%22%3A%22answer%22%2C%22sourceId%22%3A181461594%7D)配套使用，单独使用不给任何保证。

这里的区别在于，[volatile](https://www.zhihu.com/search?q=volatile&search_source=Entity&hybrid_search_source=Entity&hybrid_search_extra=%7B%22sourceType%22%3A%22answer%22%2C%22sourceId%22%3A181461594%7D)写是全屏障，而同步读的部分是不必要的。

![](https://pic1.zhimg.com/50/v2-7745d85c5d263ed6cb7adf4a482cd7eb_720w.jpg?source=1940ef5c)

![](https://pic1.zhimg.com/50/v2-e6bd87d192701135d8d241d3bb6cb4e2_720w.jpg?source=1940ef5c)

本回答参考了两位大神的答案和交流指导，受益匪浅。

本回答仅仅是对于

两位大神的答案的一点补充。

本答案基于本人没仔细研究的 Java 8 哈，题目中 putOrderedObject 在 Java 9+ 之后的新内存访问模型中已经被移除了。如果有错误欢迎大家指正。

在 JIT 优化之前，如

所述，底层是一模一样的。

在运行一定次数之后，C1 开始优化，优化后的代码也是一模一样的。这个地方对应代码：

`vmSymbols.hpp` 中可以看到两个方法的 intrinsic key 分别是：

```
do_intrinsic(_putObjectVolatile,        sun_misc_Unsafe,        putObjectVolatile_name, putObject_signature,   F_RN)



do_intrinsic(_putOrderedObject,         sun_misc_Unsafe,        putOrderedObject_name, putOrderedObject_signature, F_RN) \
   do_name(     putOrderedObject_name,                           "putOrderedObject")                                    \
   do_alias(    putOrderedObject_signature,                     /*(LObject;JLObject;)V*/ putObject_signature)    
```

根据这两个 Key 可以在 `c1_GraphBuilder.cpp` 中找到 C1 编译后的优化，两个方法分别是：

```
case vmIntrinsics::_putOrderedObject : return append_unsafe_put_obj(callee, T_OBJECT,  true); 

case vmIntrinsics::_putObjectVolatile : return append_unsafe_put_obj(callee, T_OBJECT,  true);
```

经过 C2 的优化后，两个方法实际执行的代码就不一样了，对应的是 **library_call.cpp** 中的：

```
case vmIntrinsics::_putOrderedObject:         return inline_unsafe_ordered_store(T_OBJECT);
case vmIntrinsics::_putObjectVolatile:        return inline_unsafe_access(!is_native_ptr,  is_store, T_OBJECT,   is_volatile);
```

这两个方法的源码分别是：

```
bool LibraryCallKit::inline_unsafe_ordered_store(BasicType type) {
  // This is another variant of inline_unsafe_access, differing in
  // that it always issues store-store ("release") barrier and ensures
  // store-atomicity (which only matters for "long").

  if (callee()->is_static())  return false;  // caller must have the capability!

#ifndef PRODUCT
  {
    ResourceMark rm;
    // Check the signatures.
    ciSignature* sig = callee()->signature();
#ifdef ASSERT
    BasicType rtype = sig->return_type()->basic_type();
    assert(rtype == T_VOID, "must return void");
    assert(sig->count() == 3, "has 3 arguments");
    assert(sig->type_at(0)->basic_type() == T_OBJECT, "base is object");
    assert(sig->type_at(1)->basic_type() == T_LONG, "offset is long");
#endif // ASSERT
  }
#endif //PRODUCT

  C->set_has_unsafe_access(true);  // Mark eventual nmethod as "unsafe".

  // Get arguments:
  Node* receiver = argument(0);  // type: oop
  Node* base     = argument(1);  // type: oop
  Node* offset   = argument(2);  // type: long
  Node* val      = argument(4);  // type: oop, int, or long

  // Null check receiver.
  receiver = null_check(receiver);
  if (stopped()) {
    return true;
  }

  // Build field offset expression.
  assert(Unsafe_field_offset_to_byte_offset(11) == 11, "fieldOffset must be byte-scaled");
  // 32-bit machines ignore the high half of long offsets
  offset = ConvL2X(offset);
  Node* adr = make_unsafe_address(base, offset);
  const TypePtr *adr_type = _gvn.type(adr)->isa_ptr();
  const Type *value_type = Type::get_const_basic_type(type);
  Compile::AliasType* alias_type = C->alias_type(adr_type);

  insert_mem_bar(Op_MemBarRelease);
  insert_mem_bar(Op_MemBarCPUOrder);
  // Ensure that the store is atomic for longs:
  const bool require_atomic_access = true;
  Node* store;
  if (type == T_OBJECT) // reference stores need a store barrier.
    store = store_oop_to_unknown(control(), base, adr, adr_type, val, type);
  else {
    store = store_to_memory(control(), adr, val, type, adr_type, require_atomic_access);
  }
  insert_mem_bar(Op_MemBarCPUOrder);
  return true;
}


bool LibraryCallKit::inline_unsafe_access(bool is_native_ptr, bool is_store, BasicType type, bool is_volatile) {
  if (callee()->is_static())  return false;  // caller must have the capability!

#ifndef PRODUCT
  {
    ResourceMark rm;
    // Check the signatures.
    ciSignature* sig = callee()->signature();
#ifdef ASSERT
    if (!is_store) {
      // Object getObject(Object base, int/long offset), etc.
      BasicType rtype = sig->return_type()->basic_type();
      if (rtype == T_ADDRESS_HOLDER && callee()->name() == ciSymbol::getAddress_name())
          rtype = T_ADDRESS;  // it is really a C void*
      assert(rtype == type, "getter must return the expected value");
      if (!is_native_ptr) {
        assert(sig->count() == 2, "oop getter has 2 arguments");
        assert(sig->type_at(0)->basic_type() == T_OBJECT, "getter base is object");
        assert(sig->type_at(1)->basic_type() == T_LONG, "getter offset is correct");
      } else {
        assert(sig->count() == 1, "native getter has 1 argument");
        assert(sig->type_at(0)->basic_type() == T_LONG, "getter base is long");
      }
    } else {
      // void putObject(Object base, int/long offset, Object x), etc.
      assert(sig->return_type()->basic_type() == T_VOID, "putter must not return a value");
      if (!is_native_ptr) {
        assert(sig->count() == 3, "oop putter has 3 arguments");
        assert(sig->type_at(0)->basic_type() == T_OBJECT, "putter base is object");
        assert(sig->type_at(1)->basic_type() == T_LONG, "putter offset is correct");
      } else {
        assert(sig->count() == 2, "native putter has 2 arguments");
        assert(sig->type_at(0)->basic_type() == T_LONG, "putter base is long");
      }
      BasicType vtype = sig->type_at(sig->count()-1)->basic_type();
      if (vtype == T_ADDRESS_HOLDER && callee()->name() == ciSymbol::putAddress_name())
        vtype = T_ADDRESS;  // it is really a C void*
      assert(vtype == type, "putter must accept the expected value");
    }
#endif // ASSERT
 }
#endif //PRODUCT

  C->set_has_unsafe_access(true);  // Mark eventual nmethod as "unsafe".

  Node* receiver = argument(0);  // type: oop

  // Build address expression.  See the code in inline_unsafe_prefetch.
  Node* adr;
  Node* heap_base_oop = top();
  Node* offset = top();
  Node* val;

  if (!is_native_ptr) {
    // The base is either a Java object or a value produced by Unsafe.staticFieldBase
    Node* base = argument(1);  // type: oop
    // The offset is a value produced by Unsafe.staticFieldOffset or Unsafe.objectFieldOffset
    offset = argument(2);  // type: long
    // We currently rely on the cookies produced by Unsafe.xxxFieldOffset
    // to be plain byte offsets, which are also the same as those accepted
    // by oopDesc::field_base.
    assert(Unsafe_field_offset_to_byte_offset(11) == 11,
           "fieldOffset must be byte-scaled");
    // 32-bit machines ignore the high half!
    offset = ConvL2X(offset);
    adr = make_unsafe_address(base, offset);
    heap_base_oop = base;
    val = is_store ? argument(4) : NULL;
  } else {
    Node* ptr = argument(1);  // type: long
    ptr = ConvL2X(ptr);  // adjust Java long to machine word
    adr = make_unsafe_address(NULL, ptr);
    val = is_store ? argument(3) : NULL;
  }

  const TypePtr *adr_type = _gvn.type(adr)->isa_ptr();

  // First guess at the value type.
  const Type *value_type = Type::get_const_basic_type(type);

  // Try to categorize the address.  If it comes up as TypeJavaPtr::BOTTOM,
  // there was not enough information to nail it down.
  Compile::AliasType* alias_type = C->alias_type(adr_type);
  assert(alias_type->index() != Compile::AliasIdxBot, "no bare pointers here");

  // We will need memory barriers unless we can determine a unique
  // alias category for this reference.  (Note:  If for some reason
  // the barriers get omitted and the unsafe reference begins to "pollute"
  // the alias analysis of the rest of the graph, either Compile::can_alias
  // or Compile::must_alias will throw a diagnostic assert.)
  bool need_mem_bar = (alias_type->adr_type() == TypeOopPtr::BOTTOM);

  // If we are reading the value of the referent field of a Reference
  // object (either by using Unsafe directly or through reflection)
  // then, if G1 is enabled, we need to record the referent in an
  // SATB log buffer using the pre-barrier mechanism.
  // Also we need to add memory barrier to prevent commoning reads
  // from this field across safepoint since GC can change its value.
  bool need_read_barrier = !is_native_ptr && !is_store &&
                           offset != top() && heap_base_oop != top();

  if (!is_store && type == T_OBJECT) {
    const TypeOopPtr* tjp = sharpen_unsafe_type(alias_type, adr_type, is_native_ptr);
    if (tjp != NULL) {
      value_type = tjp;
    }
  }

  receiver = null_check(receiver);
  if (stopped()) {
    return true;
  }
  // Heap pointers get a null-check from the interpreter,
  // as a courtesy.  However, this is not guaranteed by Unsafe,
  // and it is not possible to fully distinguish unintended nulls
  // from intended ones in this API.

  if (is_volatile) {
    // We need to emit leading and trailing CPU membars (see below) in
    // addition to memory membars when is_volatile. This is a little
    // too strong, but avoids the need to insert per-alias-type
    // volatile membars (for stores; compare Parse::do_put_xxx), which
    // we cannot do effectively here because we probably only have a
    // rough approximation of type.
    need_mem_bar = true;
    // For Stores, place a memory ordering barrier now.
    if (is_store)
      insert_mem_bar(Op_MemBarRelease);
  }

  // Memory barrier to prevent normal and 'unsafe' accesses from
  // bypassing each other.  Happens after null checks, so the
  // exception paths do not take memory state from the memory barrier,
  // so there's no problems making a strong assert about mixing users
  // of safe & unsafe memory.  Otherwise fails in a CTW of rt.jar
  // around 5701, class sun/reflect/UnsafeBooleanFieldAccessorImpl.
  if (need_mem_bar) insert_mem_bar(Op_MemBarCPUOrder);

  if (!is_store) {
    Node* p = make_load(control(), adr, value_type, type, adr_type, is_volatile);
    // load value
    switch (type) {
    case T_BOOLEAN:
    case T_CHAR:
    case T_BYTE:
    case T_SHORT:
    case T_INT:
    case T_LONG:
    case T_FLOAT:
    case T_DOUBLE:
      break;
    case T_OBJECT:
      if (need_read_barrier) {
        insert_pre_barrier(heap_base_oop, offset, p, !(is_volatile || need_mem_bar));
      }
      break;
    case T_ADDRESS:
      // Cast to an int type.
      p = _gvn.transform(new (C) CastP2XNode(NULL, p));
      p = ConvX2L(p);
      break;
    default:
      fatal(err_msg_res("unexpected type %d: %s", type, type2name(type)));
      break;
    }
    // The load node has the control of the preceding MemBarCPUOrder.  All
    // following nodes will have the control of the MemBarCPUOrder inserted at
    // the end of this method.  So, pushing the load onto the stack at a later
    // point is fine.
    set_result(p);
  } else {
    // place effect of store into memory
    switch (type) {
    case T_DOUBLE:
      val = dstore_rounding(val);
      break;
    case T_ADDRESS:
      // Repackage the long as a pointer.
      val = ConvL2X(val);
      val = _gvn.transform(new (C) CastX2PNode(val));
      break;
    }

    if (type != T_OBJECT ) {
      (void) store_to_memory(control(), adr, val, type, adr_type, is_volatile);
    } else {
      // Possibly an oop being stored to Java heap or native memory
      if (!TypePtr::NULL_PTR->higher_equal(_gvn.type(heap_base_oop))) {
        // oop to Java heap.
        (void) store_oop_to_unknown(control(), heap_base_oop, adr, adr_type, val, type);
      } else {
        // We can't tell at compile time if we are storing in the Java heap or outside
        // of it. So we need to emit code to conditionally do the proper type of
        // store.

        IdealKit ideal(this);
#define __ ideal.
        // QQQ who knows what probability is here??
        __ if_then(heap_base_oop, BoolTest::ne, null(), PROB_UNLIKELY(0.999)); {
          // Sync IdealKit and graphKit.
          sync_kit(ideal);
          Node* st = store_oop_to_unknown(control(), heap_base_oop, adr, adr_type, val, type);
          // Update IdealKit memory.
          __ sync_kit(this);
        } __ else_(); {
          __ store(__ ctrl(), adr, val, type, alias_type->index(), is_volatile);
        } __ end_if();
        // Final sync IdealKit and GraphKit.
        final_sync(ideal);
#undef __
      }
    }
  }

  if (is_volatile) {
    if (!is_store)
      insert_mem_bar(Op_MemBarAcquire);
    else
      insert_mem_bar(Op_MemBarVolatile);
  }

  if (need_mem_bar) insert_mem_bar(Op_MemBarCPUOrder);

  return true;
}
```

这个问题问的好，如果能理解这一点，Java 7 ConcurrentHashMap的技术细节也就了解的差不多了，putOrderedObject在[put方法](https://www.zhihu.com/search?q=put%E6%96%B9%E6%B3%95&search_source=Entity&hybrid_search_source=Entity&hybrid_search_extra=%7B%22sourceType%22%3A%22answer%22%2C%22sourceId%22%3A1831937506%7D)中出现的场景最典型：

```
final V put(K key, int hash, V value, boolean onlyIfAbsent) {
    HashEntry<K,V> node = tryLock() ? null : scanAndLockForPut(key, hash, value); 
    V oldValue;
    try {
        // 注意这行代码
        HashEntry<K,V>[] tab = table;
        int index = (tab.length - 1) & hash;
        HashEntry<K,V> first = entryAt(tab, index);
        for (HashEntry<K,V> e = first;;) {
            if (e != null) {
                K k; // 如果找到key相同的数据项，则直接替换
                if ((k = e.key) == key || (e.hash == hash && key.equals(k))) {
                    oldValue = e.value;
                    if (!onlyIfAbsent) {
                        e.value = value;
                        ++modCount; 
                    }
                    break;
                }
                e = e.next;
            }
            else {
                if (node != null)
                    // node不为空说明已经在自旋等待时初始化了，注意调用的是setNext，不是直接操作next
                    node.setNext(first); 
                else
                    // 否则，在这里新建一个HashEntry
                    node = new HashEntry<K,V>(hash, key, value, first);
                int c = count + 1; // 先加1
                if (c > threshold && tab.length < MAXIMUM_CAPACITY)
                    rehash(node);
                else
                    // 将新节点写入，注意这里调用的方法有门道
                    setEntryAt(tab, index, node); 
                ++modCount;
                count = c;
                oldValue = null;
                break;
            }
        }
    } finally {
        unlock();
    }
    return oldValue;
}
```

这段代码想要读懂，一定要注意其中不起眼的一行代码：

```
HashEntry<K,V>[] tab = table;
```

作者为什么要这么做呢，看看table的定义吧

```
transient volatile HashEntry<K,V>[] table;
```

`table`变量被关键字`volatile`修饰，`CPU`在处理`volatile`修饰的变量的时候采取下面的行为：

> **嗅探**  
> 每个处理器通过[嗅探](https://www.zhihu.com/search?q=%E5%97%85%E6%8E%A2&search_source=Entity&hybrid_search_source=Entity&hybrid_search_extra=%7B%22sourceType%22%3A%22answer%22%2C%22sourceId%22%3A1831937506%7D)在总线上传播的数据来检查自己缓存的值是不是过期了，当处理器发现自己缓存行对应的[内存地址](https://www.zhihu.com/search?q=%E5%86%85%E5%AD%98%E5%9C%B0%E5%9D%80&search_source=Entity&hybrid_search_source=Entity&hybrid_search_extra=%7B%22sourceType%22%3A%22answer%22%2C%22sourceId%22%3A1831937506%7D)被修改，就会将当前处理器的[缓存行](https://www.zhihu.com/search?q=%E7%BC%93%E5%AD%98%E8%A1%8C&search_source=Entity&hybrid_search_source=Entity&hybrid_search_extra=%7B%22sourceType%22%3A%22answer%22%2C%22sourceId%22%3A1831937506%7D)设置成无效状态，当处理器对这个数据进行修改操作的时候，会重新从系统内存中把数据读到处理器缓存里

因此直接读取这类变量的读取和写入比[普通变量](https://www.zhihu.com/search?q=%E6%99%AE%E9%80%9A%E5%8F%98%E9%87%8F&search_source=Entity&hybrid_search_source=Entity&hybrid_search_extra=%7B%22sourceType%22%3A%22answer%22%2C%22sourceId%22%3A1831937506%7D)的性能消耗更大，因此在`put`方法的开头将`table`变量赋值给一个普通的本地变量目的是为了消除`volatile`带来的性能损耗。这里就有另外一个问题：那这样做会不会导致`table`的语义改变，让别的线程读取不到最新的值呢？别着急，我们接着看。

注意`put`方法中的这个方法：`entryAt()`:

```
static final <K,V> HashEntry<K,V> entryAt(HashEntry<K,V>[] tab, int i) {
    return (tab == null) ? null : (HashEntry<K,V>) UNSAFE.getObjectVolatile(tab, ((long)i << TSHIFT) + TBASE);
}
```

这个方法的底层会调用`UNSAFE.getObjectVolatile`，这个方法的目的就是对于普通变量读取也能像`volatile`修饰的变量那样读取到最新的值，在前文中我们分析过，由于变量`tab`现在是一个普通的临时变量，如果直接调用`tab[i]`则大概率是拿不到最新的首节点的。细心的读者读到这里可能会想：Doug Lea是不是糊涂了，兜兜转换不是回到了原点么，为啥不刚开始就操作`volatile`变量呢，费了这老大劲。我们继续往下看。

在`put`方法的实现中，如果链表中没有`key`值相等的数据项，则会把新的数据项插入到链表头写入到数组中，其中调用的方法是：

```
static final <K,V> void setEntryAt(HashEntry<K,V>[] tab, int i, HashEntry<K,V> e) {
    UNSAFE.putOrderedObject(tab, ((long)i << TSHIFT) + TBASE, e);
}
```

`putOrderedObject`这个接口写入的数据不会马上被其他线程获取到，而是在`put`方法最后调用`unclock`后才会对其他线程可见，参见前文中对JMM的描述：

> 对一个变量执行unlock操作之前，必须先把此变量同步到主内存中（执行store和write操作）

这样的好处有两个，第一是性能，因为在持有锁的临界区不需要有[同步主存](https://www.zhihu.com/search?q=%E5%90%8C%E6%AD%A5%E4%B8%BB%E5%AD%98&search_source=Entity&hybrid_search_source=Entity&hybrid_search_extra=%7B%22sourceType%22%3A%22answer%22%2C%22sourceId%22%3A1831937506%7D)的操作，因此持有锁的时间更短。第二是保证了数据的一致性，在`put`操作的`finally`语句执行完之前，`put`新增的数据是不对其他线程展示的，这是`ConcurrentHashMap`实现[无锁读](https://www.zhihu.com/search?q=%E6%97%A0%E9%94%81%E8%AF%BB&search_source=Entity&hybrid_search_source=Entity&hybrid_search_extra=%7B%22sourceType%22%3A%22answer%22%2C%22sourceId%22%3A1831937506%7D)的关键原因。

我们在这里稍微总结一下`put`方法里面最重要的三个细节，首先将`volatile`变量转为普通变量提升性能，因为在`put`中需要读取到最新的数据，因此接下来调用`UNSAFE.getObjectVolatile`获取到最新的头结点，但是通过调用`UNSAFE.putOrderedObject`让变量写入主存的时间延迟到`put`方法的结尾，一来缩小临界区提升性能，而来也能保证其他线程读取到的是完整数据。

其实ConcurrentHashMap起码还有十个类似于这样的技术细节，详见下文：

![动图封面](https://picx.zhimg.com/50/v2-825a283c49836376fe756a8d8f9c8438_720w.jpg?source=1940ef5c)
