/*
 * Copyright (c) 2003, 2013, Oracle and/or its affiliates. All rights reserved.
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

package sun.misc;

import java.lang.ref.*;
import java.security.AccessController;
import java.security.PrivilegedAction;


/**
 * Cleaner继承自PhantomReference，其源码可以参考OpenJDK jdk\src\share\classes\sun\misc\Cleaner.java。
 * Cleaner表示一种更轻量更健壮的资源清理方式，相对于Object的finalization机制，轻量是因为Cleaner不是JVM创建的，不需要借助JNI调用创建，
 * 执行资源清理的代码是ReferenceHandler Thread调用的而非finalizer Thread；
 * 健壮是因为Cleaner继承自PhantomReference，是最弱的一种引用类型，可以避免恶心的顺序问题。
 * Cleaner封装了执行资源清理任务的逻辑，具体的资源清理逻辑通过创建Cleaner时的方法入参Runnable方法指定，Cleaner保证执行资源清理任务是线程安全的，
 * 即会捕获所有的异常，且保证只执行一次。一旦垃圾回收器发现Cleaner实例是phantom-reachable，即没有其他实例强引用该实例，
 * 垃圾回收器就会把Cleaner实例加入到Reference的pending队列中，由ReferenceHandler Thread负责调用其clean方法执行资源清理动作。
 * Cleaner并不能完全替代Object的finalization机制，使用Cleaner时要求其资源清理逻辑比较简单，否则容易阻塞ReferenceHandler Thread，阻塞其他的资源清理任务执行。
 *
 * 可以参考java.nio.DirectByteBuffer类中Cleaner的应用
 *
 * General-purpose phantom-reference-based cleaners.
 *
 * <p> Cleaners are a lightweight and more robust alternative to finalization.
 * They are lightweight because they are not created by the VM and thus do not
 * require a JNI upcall to be created, and because their cleanup code is
 * invoked directly by the reference-handler thread rather than by the
 * finalizer thread.  They are more robust because they use phantom references,
 * the weakest type of reference object, thereby avoiding the nasty ordering
 * problems inherent to finalization.
 *
 * <p> A cleaner tracks a referent object and encapsulates a thunk of arbitrary
 * cleanup code.  Some time after the GC detects that a cleaner's referent has
 * become phantom-reachable, the reference-handler thread will run the cleaner.
 * Cleaners may also be invoked directly; they are thread safe and ensure that
 * they run their thunks at most once.
 *
 * <p> Cleaners are not a replacement for finalization.  They should be used
 * only when the cleanup code is extremely simple and straightforward.
 * Nontrivial cleaners are inadvisable since they risk blocking the
 * reference-handler thread and delaying further cleanup and finalization.
 *
 *
 * @author Mark Reinhold
 */

public class Cleaner
    extends PhantomReference<Object>
{

    // Dummy reference queue, needed because the PhantomReference constructor
    // insists that we pass a queue.  Nothing will ever be placed on this queue
    // since the reference handler invokes cleaners explicitly.
    // 因为PhantomReference的构造方法要求必须传入ReferenceQueue参数，所以这里声明了一个，但是实际上并不会往里面添加Cleaner实例
    // 因为ReferenceHandler Thread会直接调用Cleaner实例的clean方法，不会将其加入到dummyQueue队列中
    private static final ReferenceQueue<Object> dummyQueue = new ReferenceQueue<>();

    // Doubly-linked list of live cleaners, which prevents the cleaners
    // themselves from being GC'd before their referents
    // Cleaner链表的链表头
    static private Cleaner first = null;

    /**
     * Cleaner通过prev，next属性内部维护了一个双向链表，其中静态属性first就是链表头，即所有的Cleaner实例通过链表维护着引用关系，
     * 但是这种引用是phantom-reachable的，一旦某个Cleaner实例没有强引用则会被垃圾回收器加入到Reference的pending队列中，等待被处理。
     * Cleaner的核心方法有两个，一是创建Cleaner实例的create方法，该方法会将创建的实例加入到链表中；另外一个是执行资源清理的clean方法，
     * 该方法将当前实例从链表中移除，然后执行Cleaner实例创建时传入的thunk，如果出现异常则打印err日志并导致JVM进程终止。
     *
     */
    private Cleaner
        next = null,
        prev = null;

    // add方法将cl加入到链表的头部
    private static synchronized Cleaner add(Cleaner cl) {
        if (first != null) {
            cl.next = first;
            first.prev = cl;
        }
        first = cl;
        return cl;
    }

    //remove方法将cl从链表中移除
    private static synchronized boolean remove(Cleaner cl) {

        // If already removed, do nothing
        //说明cl已经从链表移除了，不需要再处理
        if (cl.next == cl)
            return false;

        // Update list
        if (first == cl) {
            if (cl.next != null)
                first = cl.next;
            else
                first = cl.prev;
        }
        if (cl.next != null)
            cl.next.prev = cl.prev;
        if (cl.prev != null)
            cl.prev.next = cl.next;

        // Indicate removal by pointing the cleaner to itself
        //从链表移除后会将cl的prev和next都指向它自己
        cl.next = cl;
        cl.prev = cl;
        return true;

    }

    private final Runnable thunk;

    private Cleaner(Object referent, Runnable thunk) {
        super(referent, dummyQueue);
        this.thunk = thunk;
    }

    /**
     *
     * 核心入口方法，创建Cleaner，thunk就是具体的执行资源清理的逻辑
     * Creates a new cleaner.
     *
     * @param  ob the referent object to be cleaned
     * @param  thunk
     *         The cleanup code to be run when the cleaner is invoked.  The
     *         cleanup code is run directly from the reference-handler thread,
     *         so it should be as simple and straightforward as possible.
     *
     * @return  The new cleaner
     */
    public static Cleaner create(Object ob, Runnable thunk) {
        if (thunk == null)
            return null;
        return add(new Cleaner(ob, thunk));
    }

    /**
     * Runs this cleaner, if it has not been run before.
     */
    public void clean() {
        //首先从队列移除当前Cleaner实例
        if (!remove(this))
            return;
        try {
            //执行资源清理
            thunk.run();
        } catch (final Throwable x) {
            //捕获所有异常，打印err日志并退出
            AccessController.doPrivileged(new PrivilegedAction<Void>() {
                    public Void run() {
                        if (System.err != null)
                            new Error("Cleaner terminated abnormally", x)
                                .printStackTrace();
                        System.exit(1);
                        return null;
                    }});
        }
    }

}
