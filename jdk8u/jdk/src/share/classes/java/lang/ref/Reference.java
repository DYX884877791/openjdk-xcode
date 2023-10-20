/*
 * Copyright (c) 1997, 2017, Oracle and/or its affiliates. All rights reserved.
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

package java.lang.ref;

import sun.misc.Cleaner;
import sun.misc.JavaLangRefAccess;
import sun.misc.SharedSecrets;

/**
 * Reference是所有表示对象引用的类的抽象基类，定义了公共的方法，Reference所引用的对象称为referent。
 *  因为Reference的实现与JVM的垃圾回收机制是强相关的，所以不建议直接继承Reference，避免改变Reference的关键实现
 *
 * Abstract base class for reference objects.  This class defines the
 * operations common to all reference objects.  Because reference objects are
 * implemented in close cooperation with the garbage collector, this class may
 * not be subclassed directly.
 *
 * @author   Mark Reinhold
 * @since    1.2
 */

public abstract class Reference<T> {

    /**
     *  Reference定义了四种内部状态：
     *
     *  Active:     新创建的Reference实例就是Active状态，当垃圾回收器发现所引用的对象referent的可达状态发生变更了，可能将Reference实例的状态改成Pending或者Inactive，
     *      取决于Reference实例创建时是否传入ReferenceQueue实例引用。如果是从Active改成Pending，则垃圾回收器还会将该Reference实例加入到pending属性对应的Reference列表中
     *  Pending:    pending属性对应的Reference列表中的Reference实例的状态都是Pending，等待ReferenceHandler Thread将这些实例加入到queue队列中
     *  Enqueued:   queue属性对应的ReferenceQueue队列中的Reference实例的状态都是Enqueued，当实例从ReferenceQueue队列中移除就会变成Inactive。
     *      如果Reference实例在创建时没有传入ReferenceQueue，则永远不会处于Enqueued状态。
     *  Inactive:   变成Inactive以后，状态就不会再变更，等待垃圾回收器回收掉该实例
     *
     *  在不同的状态下，queue和next属性的赋值会发生变更，如下：
     *
     *  Active:     queue就是Reference实例创建时传入的ReferenceQueue引用，如果没有传入或者传入的是null，则为ReferenceQueue.NULL；此时next属性为null。
     *  Pending:    queue就是Reference实例创建时传入的ReferenceQueue引用，next属性是this
     *  Enqueued:   queue就是ReferenceQueue.ENQUEUED，next属性就是队列中的下一个元素，如果当前Reference实例就是最后一个，则是this
     *  Inactive:   queue就是ReferenceQueue.NULL，next属性就是this
     *
     * A Reference instance is in one of four possible internal states:
     *
     *     Active: Subject to special treatment by the garbage collector.  Some
     *     time after the collector detects that the reachability of the
     *     referent has changed to the appropriate state, it changes the
     *     instance's state to either Pending or Inactive, depending upon
     *     whether or not the instance was registered with a queue when it was
     *     created.  In the former case it also adds the instance to the
     *     pending-Reference list.  Newly-created instances are Active.
     *
     *     Pending: An element of the pending-Reference list, waiting to be
     *     enqueued by the Reference-handler thread.  Unregistered instances
     *     are never in this state.
     *
     *     Enqueued: An element of the queue with which the instance was
     *     registered when it was created.  When an instance is removed from
     *     its ReferenceQueue, it is made Inactive.  Unregistered instances are
     *     never in this state.
     *
     *     Inactive: Nothing more to do.  Once an instance becomes Inactive its
     *     state will never change again.
     *
     * The state is encoded in the queue and next fields as follows:
     *
     *     Active: queue = ReferenceQueue with which instance is registered, or
     *     ReferenceQueue.NULL if it was not registered with a queue; next =
     *     null.
     *
     *     Pending: queue = ReferenceQueue with which instance is registered;
     *     next = this
     *
     *     Enqueued: queue = ReferenceQueue.ENQUEUED; next = Following instance
     *     in queue, or this if at end of list.
     *
     *     Inactive: queue = ReferenceQueue.NULL; next = this.
     *
     * With this scheme the collector need only examine the next field in order
     * to determine whether a Reference instance requires special treatment: If
     * the next field is null then the instance is active; if it is non-null,
     * then the collector should treat the instance normally.
     *
     * To ensure that a concurrent collector can discover active Reference
     * objects without interfering with application threads that may apply
     * the enqueue() method to those objects, collectors should link
     * discovered objects through the discovered field. The discovered
     * field is also used for linking Reference objects in the pending list.
     */

    // 所引用的对象，下面的这4个成员变量(referent, queue, next, discovered)是绝对绝对不能改变他们的顺序的，如果它们的顺序发生变化了，你得在Hotspot的一个很偏僻的角落里把那个逻辑也改了，才能正确工作。
    private T referent;         /* Treated specially by GC */

    // 当所引用的对象referent被清理时用来保存Reference的队列，调用方可以通过ReferenceQueue的poll方法获取队列中的Reference实例，从而知道哪些referent对象被回收掉了
    volatile ReferenceQueue<? super T> queue;

    /**
     *  ReferenceQueue使用，通过next属性将所有加入到ReferenceQueue中的Reference实例构成一个链表
     *
     * When active:   NULL
     *     pending:   this
     *    Enqueued:   next reference in queue (or this if last)
     *    Inactive:   this
     */
    @SuppressWarnings("rawtypes")
    volatile Reference next;

    /**
     * JVM使用的，用于将所有的Pending状态的Reference实例构成一个链表
     *
     * When active:   next element in a discovered reference list maintained by GC (or this if last)
     *     pending:   next element in the pending list (or null if last)
     *   otherwise:   NULL
     */
    transient private Reference<T> discovered;  /* used by VM */


    /* Object used to synchronize with the garbage collector.  The collector
     * must acquire this lock at the beginning of each collection cycle.  It is
     * therefore critical that any code holding this lock complete as quickly
     * as possible, allocate no new objects, and avoid calling user code.
     */
    static private class Lock { }
    // 用来修改Pending状态的Reference实例链表的锁
    private static Lock lock = new Lock();


    /**
     * Pending状态的Reference实例链表的链表头元素，垃圾回收器发现某个对象只有Reference实例引用，就会把Reference对象加入到这个链表中，
     * 而ReferenceHandler Thread不断从这个链表中移除元素，将其加入到Reference实例创建时传入的ReferenceQueue队列中
     *
     * List of References waiting to be enqueued.  The collector adds
     * References to this list, while the Reference-handler thread removes
     * them.  This list is protected by the above lock object. The
     * list uses the discovered field to link its elements.
     */
    private static Reference<Object> pending = null;

    /**
     * ReferenceHandler继承自Thread，表示一个不断将Pending状态的Reference实例放入该实例创建时传入的ReferenceQueue实例中，
     * 所有处于Pending状态的Reference实例通过discovered实例属性构成链表，链表头就是Reference类的静态属性pending，在遍历链表时，
     * 如果链表为空则通过lock.wait()的方式等待；如果遍历的Reference实例是Cleaner，则调用其clean方法，用于清理资源清理。
     *
     * High-priority thread to enqueue pending References
     */
    private static class ReferenceHandler extends Thread {

        private static void ensureClassInitialized(Class<?> clazz) {
            try {
                Class.forName(clazz.getName(), true, clazz.getClassLoader());
            } catch (ClassNotFoundException e) {
                throw (Error) new NoClassDefFoundError(e.getMessage()).initCause(e);
            }
        }

        static {
            // pre-load and initialize InterruptedException and Cleaner classes
            // so that we don't get into trouble later in the run loop if there's
            // memory shortage while loading/initializing them lazily.
            // 预先加载并初始化两个类
            ensureClassInitialized(InterruptedException.class);
            ensureClassInitialized(Cleaner.class);
        }

        ReferenceHandler(ThreadGroup g, String name) {
            super(g, name);
        }

        public void run() {
            while (true) {
                // while true不断执行
                tryHandlePending(true);
            }
        }
    }

    /**
     * tryHandlePending方法是ReferenceHandler Thread调用的用于处理Pending状态的Reference实例的核心方法，是包级访问的。重点关注ReferenceHandler的实现。
     *
     * Try handle pending {@link Reference} if there is one.<p>
     * Return {@code true} as a hint that there might be another
     * {@link Reference} pending or {@code false} when there are no more pending
     * {@link Reference}s at the moment and the program can do some other
     * useful work instead of looping.
     *
     * @param waitForNotify if {@code true} and there was no pending
     *                      {@link Reference}, wait until notified from VM
     *                      or interrupted; if {@code false}, return immediately
     *                      when there is no pending {@link Reference}.
     * @return {@code true} if there was a {@link Reference} pending and it
     *         was processed, or we waited for notification and either got it
     *         or thread was interrupted before being notified;
     *         {@code false} otherwise.
     */
    static boolean tryHandlePending(boolean waitForNotify) {
        Reference<Object> r;
        Cleaner c;
        try {
            //注意lock和pending都是静态属性
            synchronized (lock) {
                if (pending != null) {
                    //如果存在待处理的Reference实例
                    r = pending;
                    // 'instanceof' might throw OutOfMemoryError sometimes
                    // so do this before un-linking 'r' from the 'pending' chain...
                    //Cleaner是PhantomReference的子类
                    c = r instanceof Cleaner ? (Cleaner) r : null;
                    // unlink 'r' from 'pending' chain
                    //通过discovered属性将所有Pending的Reference实例构成一个链表
                    //获取下一个处于Pending的Reference实例
                    pending = r.discovered;
                    //discovered属性置为空
                    r.discovered = null;
                } else {
                    // The waiting on the lock may cause an OutOfMemoryError
                    // because it may try to allocate exception objects.
                    //waitForNotify默认为true，阻塞当前线程直到其他线程唤醒
                    if (waitForNotify) {
                        lock.wait();
                    }
                    // retry if waited
                    return waitForNotify;
                }
            }
        } catch (OutOfMemoryError x) {
            // Give other threads CPU time so they hopefully drop some live references
            // and GC reclaims some space.
            // Also prevent CPU intensive spinning in case 'r instanceof Cleaner' above
            // persistently throws OOME for some time...
            //yield方法会让出当前线程的CPU处理时间，让垃圾回收线程获取更多的CPU时间，加速垃圾回收
            Thread.yield();
            // retry
            return true;
        } catch (InterruptedException x) {
            // retry
            return true;
        }

        // Fast path for cleaners
        if (c != null) {
            //pending属性是Cleaner，执行清理
            c.clean();
            return true;
        }

        //pending属性不是Cleaner
        //Pending状态下，r.queue就是最初r创建时传入的ReferenceQueue引用
        ReferenceQueue<? super Object> q = r.queue;
        //将r加入到queue 队列中
        if (q != ReferenceQueue.NULL) q.enqueue(r);
        return true;
    }

    // ReferenceHandler的启动是通过静态static块完成的
    static {
        ThreadGroup tg = Thread.currentThread().getThreadGroup();
        //往上遍历找到最初的父线程组
        for (ThreadGroup tgn = tg;
             tgn != null;
             tg = tgn, tgn = tg.getParent());
        //创建ReferenceHandler线程，并启动
        Thread handler = new ReferenceHandler(tg, "Reference Handler");
        /**
         * 最高优先级
         *
         * If there were a special system-only priority greater than
         * MAX_PRIORITY, it would be used here
         */
        handler.setPriority(Thread.MAX_PRIORITY);
        //后台线程
        handler.setDaemon(true);
        handler.start();

        // provide access in SharedSecrets
        //允许访问SharedSecrets
        SharedSecrets.setJavaLangRefAccess(new JavaLangRefAccess() {
            @Override
            public boolean tryHandlePendingReference() {
                return tryHandlePending(false);
            }
        });
    }

    /* -- Referent accessor and setters -- */

    /**
     * get方法返回所引用的对象referent
     *
     * Returns this reference object's referent.  If this reference object has
     * been cleared, either by the program or by the garbage collector, then
     * this method returns <code>null</code>.
     *
     * @return   The object to which this reference refers, or
     *           <code>null</code> if this reference object has been cleared
     */
    public T get() {
        return this.referent;
    }

    /**
     * clear方法用于将referent置为null
     *
     * Clears this reference object.  Invoking this method will not cause this
     * object to be enqueued.
     *
     * <p> This method is invoked only by Java code; when the garbage collector
     * clears references it does so directly, without invoking this method.
     */
    public void clear() {
        this.referent = null;
    }

    /* -- Queue operations -- */

    /**
     * isEnqueued方法判断当前Reference实例是否已加入queue队列中
     *
     * Tells whether or not this reference object has been enqueued, either by
     * the program or by the garbage collector.  If this reference object was
     * not registered with a queue when it was created, then this method will
     * always return <code>false</code>.
     *
     * @return   <code>true</code> if and only if this reference object has
     *           been enqueued
     */
    public boolean isEnqueued() {
        return (this.queue == ReferenceQueue.ENQUEUED);
    }

    /**
     * enqueue方法用于将当前Reference实例加入到创建时传入的queue队列中
     *
     * Clears this reference object and adds it to the queue with which
     * it is registered, if any.
     *
     * <p> This method is invoked only by Java code; when the garbage collector
     * enqueues references it does so directly, without invoking this method.
     *
     * @return   <code>true</code> if this reference object was successfully
     *           enqueued; <code>false</code> if it was already enqueued or if
     *           it was not registered with a queue when it was created
     */
    public boolean enqueue() {
        this.referent = null;
        return this.queue.enqueue(this);
    }


    /**
     * Throws {@link CloneNotSupportedException}. A {@code Reference} cannot be
     * meaningfully cloned. Construct a new {@code Reference} instead.
     *
     * @apiNote This method is defined in Java SE 8 Maintenance Release 4.
     *
     * @return  never returns normally
     * @throws  CloneNotSupportedException always
     *
     * @since 8
     */
    @Override
    protected Object clone() throws CloneNotSupportedException {
        throw new CloneNotSupportedException();
    }

    /* -- Constructors -- */

    Reference(T referent) {
        this(referent, null);
    }

    Reference(T referent, ReferenceQueue<? super T> queue) {
        this.referent = referent;
        this.queue = (queue == null) ? ReferenceQueue.NULL : queue;
    }
}
