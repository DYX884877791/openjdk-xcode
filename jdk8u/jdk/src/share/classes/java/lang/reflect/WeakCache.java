/*
 * Copyright (c) 2013, Oracle and/or its affiliates. All rights reserved.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 * This code is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 only, as
 * published by the Free Software Foundation.  Oracle designates this
 * particular file as subject to the "Classpath" exception as provided
 * by Oracle in the LICENSE file that accompanied this code.
 *
 * This code is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * version 2 for more details (a copy is included in the LICENSE file that
 * accompanied this code).
 *
 * You should have received a copy of the GNU General Public License version
 * 2 along with this work; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA.
 *
 * Please contact Oracle, 500 Oracle Parkway, Redwood Shores, CA 94065 USA
 * or visit www.oracle.com if you need additional information or have any
 * questions.
 */
package java.lang.reflect;

import java.lang.ref.ReferenceQueue;
import java.lang.ref.WeakReference;
import java.util.Objects;
import java.util.concurrent.ConcurrentHashMap;
import java.util.concurrent.ConcurrentMap;
import java.util.function.BiFunction;
import java.util.function.Supplier;

/**
 * 弱缓存类，也就是说它是一个起着缓存作用的类，它用来存贮软引用类型的实例
 *
 * WeakCache不仅仅是一个二级缓存，而且还是一个考虑到内存使用的缓存，其内置了弱引用类型，在其内部，一级缓存使用了弱引用类型，二级缓存则使用了强引用类型。
 * 其中WeakCache不仅仅如此，还是用到了懒加载模式，对于被GC掉的实例，不会立马清理缓存，只有当我们在使用缓存的时候才会帮我们清理。
 *
 * Cache mapping pairs of {@code (key, sub-key) -> value}. Keys and values are
 * weakly but sub-keys are strongly referenced.  Keys are passed directly to
 * {@link #get} method which also takes a {@code parameter}. Sub-keys are
 * calculated from keys and parameters using the {@code subKeyFactory} function
 * passed to the constructor. Values are calculated from keys and parameters
 * using the {@code valueFactory} function passed to the constructor.
 * Keys can be {@code null} and are compared by identity while sub-keys returned by
 * {@code subKeyFactory} or values returned by {@code valueFactory}
 * can not be null. Sub-keys are compared using their {@link #equals} method.
 * Entries are expunged from cache lazily on each invocation to {@link #get},
 * {@link #containsValue} or {@link #size} methods when the WeakReferences to
 * keys are cleared. Cleared WeakReferences to individual values don't cause
 * expunging, but such entries are logically treated as non-existent and
 * trigger re-evaluation of {@code valueFactory} on request for their
 * key/subKey.
 *
 * @author Peter Levart
 * @param <K> type of keys
 * @param <P> type of parameters
 * @param <V> type of values
 */
final class WeakCache<K, P, V> {

    // Reference引用队列，
    // refQueue是弱引用中的引用队列，在创建CacheKey时传入。
    // 当gc处理了部分CacheKey时，refQueue中会有CacheKey的引用，取出来后在调用expungeFrom方法来清除过期的缓存。
    // 是对我们入参Key的一种引用队列，从这里你想到了什么，没错，这里是对一级缓存CacheKey的一种引用的监听队列，如果CacheKey被GC掉后会放入该队列中，
    // 然后我们通过嗅探就可以晓得哪些是被GC掉的实例了，方便我们进行后续的处理；
    private final ReferenceQueue<K> refQueue
        = new ReferenceQueue<>();
    // the key type is Object for supporting null key
    // 缓存的底层实现, key为一级缓存, value为二级缓存。 为了支持null, map的key类型设置为Object
    // 一级缓存的key其实就是CacheKey或者是K为空时默认的Object,二级缓存的key就是SubKeyFactory函数计算出的结果
    private final ConcurrentMap<Object, ConcurrentMap<Object, Supplier<V>>> map
        = new ConcurrentHashMap<>();
    // reverseMap记录了所有代理类生成器是否可用(记录已注册的 Supplier), 这是为了实现缓存的过期机制
    // 为啥明明有了二级缓存结构了，还非要弄一个单独的结果集来放所有的有效的Supplier呢？哈哈，没错，如果你是一个有经验的开发人员我觉得你一定遇到过而且经常遇到过一个问题，
    // 那就是如果存的舒服并且查询的时候也舒服，没错这个缓存就是为了查询起来比较舒服做的结果统计；
    private final ConcurrentMap<Supplier<V>, Boolean> reverseMap
        = new ConcurrentHashMap<>();
    // 生成二级缓存key的工厂, 这里传入的是KeyFactory
    private final BiFunction<K, P, ?> subKeyFactory;
    // 生成二级缓存value的工厂, 这里传入的是ProxyClassFactory
    private final BiFunction<K, P, V> valueFactory;

    /**
     * 构造器, 传入生成二级缓存key的工厂和生成二级缓存value的工厂
     *
     * Construct an instance of {@code WeakCache}
     *
     * @param subKeyFactory a function mapping a pair of
     *                      {@code (key, parameter) -> sub-key}
     * @param valueFactory  a function mapping a pair of
     *                      {@code (key, parameter) -> value}
     * @throws NullPointerException if {@code subKeyFactory} or
     *                              {@code valueFactory} is null.
     */
    public WeakCache(BiFunction<K, P, ?> subKeyFactory,
                     BiFunction<K, P, V> valueFactory) {
        this.subKeyFactory = Objects.requireNonNull(subKeyFactory);
        this.valueFactory = Objects.requireNonNull(valueFactory);
    }

    /**
     * WeakCache类的实质作用是缓存实现了代理目标接口的类的信息以及对这些类的一些操作，以及这些类的类加载器的。其中的重要的方法是get方法，该方法是来获取WeakCache缓存中与委托类的Class类对象的。
     *  get方法利用ConcurrentMap类来进行缓存，里面存贮Key对象以及Factory对象，Factory类用来存贮缓存的信息的，其实现了Supplier接口，
     * 其get方法里面调用了ProxyClassFactory类的apply方法来返回Class类对象，而且get方法是一个同步的方法，也表明WeakCache缓存是线程安全的。
     *
     * WeakCache的get方法并没有用锁进行同步，那它是怎样实现线程安全的呢？因为它的所有会进行修改的成员变量都使用了ConcurrentMap，这个类是线程安全的。
     * 因此它将自身的线程安全委托给了ConcurrentMap， get方法尽可能的将同步代码块缩小，这样可以有效提高WeakCache的性能。
     * 我们看到ClassLoader作为了一级缓存的key，这样可以首先根据ClassLoader筛选一遍，因为不同ClassLoader加载的类是不同的。
     * 然后它用接口数组来生成二级缓存的key，这里它进行了一些优化，因为大部分类都是实现了一个或两个接口，所以二级缓存key分为key0，key1，key2，keyX。
     * key0到key2分别表示实现了0到2个接口，keyX表示实现了3个或以上的接口，事实上大部分都只会用到key1和key2。
     * 这些key的生成工厂是在Proxy类中，通过WeakCache的构造器将key工厂传入。这里的二级缓存的值是一个Factory实例，最终代理类的值是通过Factory这个工厂来获得的。
     *
     * Look-up the value through the cache. This always evaluates the
     * {@code subKeyFactory} function and optionally evaluates
     * {@code valueFactory} function if there is no entry in the cache for given
     * pair of (key, subKey) or the entry has already been cleared.
     *
     * @param key       possibly null key
     * @param parameter parameter used together with key to create sub-key and
     *                  value (should not be null)
     * @return the cached value (never null)
     * @throws NullPointerException if {@code parameter} passed in or
     *                              {@code sub-key} calculated by
     *                              {@code subKeyFactory} or {@code value}
     *                              calculated by {@code valueFactory} is null.
     */
    public V get(K key, P parameter) {
        // 这里要求实现的接口不能为空
        Objects.requireNonNull(parameter);

        // 删除过期的缓存，清理被GC掉的实例
        expungeStaleEntries();

        // 包装key为CacheKey，创建CacheKey对象，Key不为空
        // 将ClassLoader包装成CacheKey, 作为一级缓存的key
        // 生成缓存key,正常来说应该位CacheKey类型，但是这里key允许为空,CacheKey里如果为空会给个Object全局唯一实例作为Key,
        // 所以是Object类型,这里CacheKey是一个弱引用类型
        Object cacheKey = CacheKey.valueOf(key, refQueue);

        // lazily install the 2nd level valuesMap for the particular cacheKey
        // 获取得到二级缓存
        ConcurrentMap<Object, Supplier<V>> valuesMap = map.get(cacheKey);
        // 如果根据ClassLoader没有获取到对应的值则创建一个ConcurrentMap对象
        if (valuesMap == null) {
            // 以CAS方式放入, 如果不存在则放入，否则返回原先的值
            ConcurrentMap<Object, Supplier<V>> oldValuesMap
                = map.putIfAbsent(cacheKey,
                                  valuesMap = new ConcurrentHashMap<>());
            // 如果oldValuesMap有值, 说明放入失败
            if (oldValuesMap != null) {
                valuesMap = oldValuesMap;
            }
        }

        // create subKey and retrieve the possible Supplier<V> stored by that
        // subKey from valuesMap
        // 通过SubKeyFactory创建key对象
        // 根据代理类实现的接口数组来生成二级缓存key, 分为key0, key1, key2, keyx
        Object subKey = Objects.requireNonNull(subKeyFactory.apply(key, parameter));
        // 这里通过subKey获取到二级缓存的值
        // 利用Key对象取出Supplier，Supplier是一个接口
        Supplier<V> supplier = valuesMap.get(subKey);
        Factory factory = null;

        // 这个循环提供了轮询机制, 如果条件为假就继续重试直到条件为真为止
        while (true) {
            // 如果通过subKey取出来的值不为空
            if (supplier != null) {
                // supplier might be a Factory or a CacheValue<V> instance
                // 从supplier接口的实现类获取Class类对象，实际调用了内部类Factory的get()方法，创建了一个代理类
                // 在这里supplier可能是一个Factory也可能会是一个CacheValue
                // 在这里不作判断, 而是在Supplier实现类的get方法里面进行验证
                V value = supplier.get();
                if (value != null) {
                    return value;
                }
            }
            // else no supplier in cache
            // or a supplier that returned null (could be a cleared CacheValue
            // or a Factory that wasn't successful in installing the CacheValue)

            // lazily construct a Factory
            //如果supplier为空，进行缓存，Factory是supplier接口的实现类，实例化内部类Factory
            if (factory == null) {
                // 新建一个Factory实例作为subKey对应的值
                factory = new Factory(key, parameter, subKey, valuesMap);
            }

            if (supplier == null) {
                // 到这里表明subKey没有对应的值, 就将factory作为subKey的值放入
                supplier = valuesMap.putIfAbsent(subKey, factory);
                if (supplier == null) {
                    // successfully installed Factory
                    // 赋值factory
                    // 到这里表明成功将factory放入缓存
                    supplier = factory;
                }
                // else retry with winning supplier
                // 否则, 可能期间有其他线程修改了值, 那么就不再继续给subKey赋值, 而是取出来直接用
            } else {
                // 期间可能其他线程修改了值, 那么就将原先的值替换
                // 假如不为空，且factory中value为空，则替换factory
                if (valuesMap.replace(subKey, supplier, factory)) {
                    // successfully replaced
                    // cleared CacheEntry / unsuccessful Factory
                    // with our Factory
                    // 成功将factory替换成新的值
                    supplier = factory;
                } else {
                    // retry with current supplier
                    // 替换失败, 继续使用原先的值
                    // 继续尝试获取接口实现类的supplier类对象
                    supplier = valuesMap.get(subKey);
                }
            }
        }
    }

    /**
     * Checks whether the specified non-null value is already present in this
     * {@code WeakCache}. The check is made using identity comparison regardless
     * of whether value's class overrides {@link Object#equals} or not.
     *
     * @param value the non-null value to check
     * @return true if given {@code value} is already cached
     * @throws NullPointerException if value is null
     */
    public boolean containsValue(V value) {
        Objects.requireNonNull(value);

        expungeStaleEntries();
        // 使用LookupValue代替CacheValue使用，简化操作。
        return reverseMap.containsKey(new LookupValue<>(value));
    }

    /**
     * Returns the current number of cached entries that
     * can decrease over time when keys/values are GC-ed.
     */
    public int size() {
        expungeStaleEntries();
        return reverseMap.size();
    }

    /**
     * 由于二级缓存中的Key使用了弱引用，所以在实际使用时gc的不定期处理会导致部分的缓存失效，通过这个方法就可以实现对失效缓存的清除。
     */
    private void expungeStaleEntries() {
        CacheKey<K> cacheKey;
        // 从引用队列中获取被GC掉的一级缓存CacheKey
        while ((cacheKey = (CacheKey<K>)refQueue.poll()) != null) {
            cacheKey.expungeFrom(map, reverseMap);
        }
    }

    /**
     * A factory {@link Supplier} that implements the lazy synchronized
     * construction of the value and installment of it into the cache.
     */
    private final class Factory implements Supplier<V> {

        // 一级缓存key, 根据ClassLoader生成
        private final K key;
        // 代理类实现的接口数组
        private final P parameter;
        // 二级缓存key, 根据接口数组生成
        private final Object subKey;
        // 二级缓存
        private final ConcurrentMap<Object, Supplier<V>> valuesMap;

        Factory(K key, P parameter, Object subKey,
                ConcurrentMap<Object, Supplier<V>> valuesMap) {
            this.key = key;
            this.parameter = parameter;
            this.subKey = subKey;
            this.valuesMap = valuesMap;
        }

        /**
         * get方法是使用synchronized关键字进行了同步。进行get方法后首先会去验证subKey对应的suppiler是否是工厂本身，如果不是就返回null，而WeakCache的get方法会继续进行重试。
         * 如果确实是工厂本身，那么就会委托ProxyClassFactory生成代理类，ProxyClassFactory是在构造WeakCache的时候传入的。
         * 所以这里解释了为什么最后会调用到Proxy的ProxyClassFactory这个内部工厂来生成代理类。生成代理类后使用弱引用进行包装并放入reverseMap中，最后会返回原装的代理类。
         */
        @Override
        public synchronized V get() { // serialize access
            // re-check
            // 这里再一次去二级缓存里面获取Supplier, 用来验证是否是Factory本身
            Supplier<V> supplier = valuesMap.get(subKey);
            if (supplier != this) {
                // something changed while we were waiting:
                // might be that we were replaced by a CacheValue
                // or were removed because of failure ->
                // return null to signal WeakCache.get() to retry
                // the loop
                // 在这里验证supplier是否是Factory实例本身, 如果不则返回null让调用者继续轮询重试
                // 期间supplier可能替换成了CacheValue, 或者由于生成代理类失败被从二级缓存中移除了
                return null;
            }
            // else still us (supplier == this)

            // create new value
            V value = null;
            try {
                // 委托valueFactory去生成代理类, 这里会通过传入的ProxyClassFactory去生成代理类
                value = Objects.requireNonNull(valueFactory.apply(key, parameter));
            } finally {
                // 如果生成代理类失败, 就将这个二级缓存删除
                if (value == null) { // remove us on failure
                    valuesMap.remove(subKey, this);
                }
            }
            // the only path to reach here is with non-null value
            // 只有value的值不为空才能到达这里
            assert value != null;

            // wrap value with CacheValue (WeakReference)
            // 使用弱引用包装生成的代理类
            CacheValue<V> cacheValue = new CacheValue<>(value);

            // put into reverseMap
            // 将cacheValue成功放入二级缓存后, 再对它进行标记
            reverseMap.put(cacheValue, Boolean.TRUE);

            // try replacing us with CacheValue (this should always succeed)
            // 将包装后的cacheValue放入二级缓存中, 这个操作必须成功, 否则就报错
            if (!valuesMap.replace(subKey, this, cacheValue)) {
                throw new AssertionError("Should not reach here");
            }

            // successfully replaced us with new CacheValue -> return the value
            // wrapped by it
            // 最后返回没有被弱引用包装的代理类
            return value;
        }
    }

    /**
     * Value接口继承自Supplier接口，实际上是为了实现get方法
     *
     * Common type of value suppliers that are holding a referent.
     * The {@link #equals} and {@link #hashCode} of implementations is defined
     * to compare the referent by identity.
     */
    private interface Value<V> extends Supplier<V> {}

    /**
     * 静态内部类，为了便于对CacheValue中的值进行判断，建立了LookupValue，也实现了Value接口，是CacheValue运算时的替代，实现方式也很相似。
     *
     * An optimized {@link Value} used to look-up the value in
     * {@link WeakCache#containsValue} method so that we are not
     * constructing the whole {@link CacheValue} just to look-up the referent.
     */
    private static final class LookupValue<V> implements Value<V> {
        // 存储实际的值
        private final V value;

        LookupValue(V value) {
            this.value = value;
        }

        // Value接口中的get方法，返回value的值
        @Override
        public V get() {
            return value;
        }

        @Override
        public int hashCode() {
            return System.identityHashCode(value); // compare by identity
        }

        @Override
        public boolean equals(Object obj) {
            return obj == this ||
                   obj instanceof Value &&
                   this.value == ((Value<?>) obj).get();  // compare by identity
        }
    }

    /**
     * A {@link Value} that weakly references the referent.
     */
    private static final class CacheValue<V>
        extends WeakReference<V> implements Value<V>
    {
        private final int hash;

        CacheValue(V value) {
            super(value);
            /**
             * 在Object类中的hashCode可以获取相应对象的hashCode，而这个identityHashCode也是可以获取对象的hashCode，那么两这有什么不同吗？
             * 从源码看两者都是本地方法（native），实际上获取时的结果是与hashCode无异的，但是这里的hashCode指的是原有的Object中的hashCode的方法，
             * 如果进行了重写就可能会有不同了，所以为了得到原有的Object中的hashCode的值，identityHashCode会比较方便。
             */
            this.hash = System.identityHashCode(value); // compare by identity
        }

        @Override
        public int hashCode() {
            return hash;
        }

        @Override
        public boolean equals(Object obj) {
            V value;
            return obj == this ||
                   obj instanceof Value &&
                   // cleared CacheValue is only equal to itself
                   (value = get()) != null &&
                   value == ((Value<?>) obj).get(); // compare by identity
        }
    }

    /**
     * CacheKey containing a weakly referenced {@code key}. It registers
     * itself with the {@code refQueue} so that it can be used to expunge
     * the entry when the {@link WeakReference} is cleared.
     */
    private static final class CacheKey<K> extends WeakReference<K> {

        // a replacement for null keys
        // 当Key为空时的占位实例Object作为键
        private static final Object NULL_KEY = new Object();

        static <K> Object valueOf(K key, ReferenceQueue<K> refQueue) {
            return key == null
                   // null key means we can't weakly reference it,
                   // so we use a NULL_KEY singleton as cache key
                   ? NULL_KEY
                   // non-null key requires wrapping with a WeakReference
                   : new CacheKey<>(key, refQueue);
        }

        private final int hash;

        private CacheKey(K key, ReferenceQueue<K> refQueue) {
            super(key, refQueue);
            this.hash = System.identityHashCode(key);  // compare by identity
        }

        @Override
        public int hashCode() {
            return hash;
        }

        @Override
        public boolean equals(Object obj) {
            K key;
            return obj == this ||
                   obj != null &&
                   obj.getClass() == this.getClass() &&
                   // cleared CacheKey is only equal to itself
                   (key = this.get()) != null &&
                   // compare key by identity
                   key == ((CacheKey<K>) obj).get();
        }

        /**
         * 通过这个方法可以将含有这个键值的相关缓存清除
         */
        void expungeFrom(ConcurrentMap<?, ? extends ConcurrentMap<?, ?>> map,
                         ConcurrentMap<?, Boolean> reverseMap) {
            // removing just by key is always safe here because after a CacheKey
            // is cleared and enqueue-ed it is only equal to itself
            // (see equals method)...
            // 直接从二级缓存中清除，并获取第二级的缓存map
            ConcurrentMap<?, ?> valuesMap = map.remove(this);
            // remove also from reverseMap if needed
            // 遍历第二级缓存并在reverseMap中清除
            if (valuesMap != null) {
                for (Object cacheValue : valuesMap.values()) {
                    reverseMap.remove(cacheValue);
                }
            }
        }
    }
}
