/*
 * Copyright (c) 1997, 2022, Oracle and/or its affiliates. All rights reserved.
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

import java.security.PrivilegedAction;
import java.security.AccessController;
import sun.misc.JavaLangAccess;
import sun.misc.SharedSecrets;
import sun.misc.VM;

/**
 * Finalizer继承自FinalReference，FinalReference是一种特殊的引用类型，主要用来辅助实现Object finalization机制
 *
 * Finalizer是借助垃圾回收器对Reference实例的特殊处理机制实现的，每创建一个实现了finalize方法的对象时，JVM会通过调用Finalizer的register方法创建一个新的Finalizer实例，
 * 该对象就是Finalizer实例的referent对象，所有的Finalizer实例构成一个链表。当某个对象只被Finalizer实例所引用，则将对应的Finalizer实例加入到Reference维护的pending链表中，
 * 通过ReferenceHandler Thread将pending链表中的Finalizer实例加入到Finalizer定义的全局ReferenceQueue中。Finalizer自身会另外起一个新线程，FinalizerThread，
 * 不断的从全局的ReferenceQueue中取出带出来的Finalizer实例，然后将该实例从Finalizer链表中移除，最后调用对应对象的finalize方法执行资源的清理，并将对referent对象的引用置为null，
 * 保证该对象能够会回收掉。当JVM进程即将退出，JVM会通过java.lang.Runtime另起线程处理掉全局ReferenceQueue中未处理完的Finalizer实例，
 * 通过java.lang.Shutdown另起线程处理掉Finalizer链表中的Finalizer实例，即没有加入到Reference维护的pending链表中的Finalizer实例。
 */
final class Finalizer extends FinalReference<Object> { /* Package-private; must be in
                                                          same package as the Reference
                                                          class */

    // 全局的ReferenceQueue队列
    private static ReferenceQueue<Object> queue = new ReferenceQueue<>();
    // 所有Finalizer 实例构成的链表的头元素
    private static Finalizer unfinalized = null;
    // 修改链表的锁
    private static final Object lock = new Object();

    // 用来构成链表的表示下一个和上一个元素
    private Finalizer
        next = null,
        prev = null;

    private boolean hasBeenFinalized() {
        // next等于this说明该实例已经从链表中移除了，已经执行过Finalized方法了
        return (next == this);
    }

    private void add() {
        synchronized (lock) {
            // 获取锁，将this插入到链表的头部
            if (unfinalized != null) {
                this.next = unfinalized;
                unfinalized.prev = this;
            }
            unfinalized = this;
        }
    }

    private void remove() {
        synchronized (lock) {
            // 获取锁，将this从链表中移除
            if (unfinalized == this) {
                if (this.next != null) {
                    unfinalized = this.next;
                } else {
                    unfinalized = this.prev;
                }
            }
            if (this.next != null) {
                this.next.prev = this.prev;
            }
            if (this.prev != null) {
                this.prev.next = this.next;
            }
            // 将next和prev都指向自己，表示已经从链表中移除
            this.next = this;   /* Indicates that this has been finalized */
            this.prev = this;
        }
    }

    private Finalizer(Object finalizee) {
        super(finalizee, queue);
        // 执行构造方法的时候，会将当前实例加入到链表中
        add();
    }

    static ReferenceQueue<Object> getQueue() {
        return queue;
    }

    /**
     * Invoked by VM 在对象创建的时候由JVM调用
     * register是JVM创建对象时，如果该类实现了finalize方法，则以新创建的对象作为参数调用此方法创建一个Finalizer实例，并将其加入到Finalizer链表的头部
     */
    static void register(Object finalizee) {
        new Finalizer(finalizee);
    }

    private void runFinalizer(JavaLangAccess jla) {
        synchronized (this) {
            //hasBeenFinalized返回true，说明该元素已经从队列移除了，直接返回
            if (hasBeenFinalized()) return;
            //将当前实例从队列中移除
            remove();
        }
        try {
            //获取所引用的对象referent
            Object finalizee = this.get();
            if (finalizee != null && !(finalizee instanceof java.lang.Enum)) {
                //实际调用Object的finalize方法
                jla.invokeFinalize(finalizee);

                /* Clear stack slot containing this variable, to decrease
                   the chances of false retention with a conservative GC */
                /* 去掉对finalizee的引用，让GC回收掉该实例 */
                finalizee = null;
            }
        } catch (Throwable x) { }
        //将所引用的对象referent置为null，即去掉对referent的引用，让GC回收掉该实例
        super.clear();
    }

    /*
       在系统线程组下创建一个新的线程执行指定任务，并等待任务执行完成，之所以开启一个新的线程，是为了与已经死锁了或者停顿的finalizer thread隔离开来
       加速finalize方法的执行

       Create a privileged secondary finalizer thread in the system thread
       group for the given Runnable, and wait for it to complete.

       This method is used by runFinalization.

       It could have been implemented by offloading the work to the
       regular finalizer thread and waiting for that thread to finish.
       The advantage of creating a fresh thread, however, is that it insulates
       invokers of that method from a stalled or deadlocked finalizer thread.
     */
    private static void forkSecondaryFinalizer(final Runnable proc) {
        AccessController.doPrivileged(
            new PrivilegedAction<Void>() {
                public Void run() {
                    ThreadGroup tg = Thread.currentThread().getThreadGroup();
                    //从当前线程的线程组往上遍历找到最初的系统线程组
                    for (ThreadGroup tgn = tg;
                         tgn != null;
                         tg = tgn, tgn = tg.getParent());
                    //启动一个新线程执行proc任务
                    Thread sft = new Thread(tg, proc, "Secondary finalizer");
                    sft.start();
                    try {
                        //等待proc任务执行完成
                        sft.join();
                    } catch (InterruptedException x) {
                        //执行异常，中断当前线程
                        Thread.currentThread().interrupt();
                    }
                    return null;
                }});
    }

    /* Called by Runtime.runFinalization() */
    /**
     * runFinalization由Runtime.runFinalization()方法调用，负责清理掉queue中未处理的Finalizer实例
     * 调用forkSecondaryFinalizer方法执行清理任务，该方法会在系统线程组下另起一个线程执行指定任务，并等待该线程执行完成，如果执行异常，则终止当前线程。
     */
    static void runFinalization() {
        if (!VM.isBooted()) {
            return;
        }

        forkSecondaryFinalizer(new Runnable() {
            private volatile boolean running;
            public void run() {
                // in case of recursive call to run()
                if (running)
                    return;
                final JavaLangAccess jla = SharedSecrets.getJavaLangAccess();
                running = true;
                for (;;) {
                    //不断遍历queue中所有的Finalizer，然后执行finalize方法
                    Finalizer f = (Finalizer)queue.poll();
                    if (f == null) break;
                    f.runFinalizer(jla);
                }
            }
        });
    }

    /**
     * FinalizerThread就是一个不断循环的线程任务，从queue属性中获取待处理的Finalizer实例，将该实例从Finalizer链表中移除然后调用其finalize方法，
     * 最后将Finalizer实例对referent对象的引用置为null，从而保证GC能够正确回收该对象
     */
    private static class FinalizerThread extends Thread {
        //标记运行状态
        private volatile boolean running;
        FinalizerThread(ThreadGroup g) {
            super(g, "Finalizer");
        }
        public void run() {
            // in case of recursive call to run()
            //已运行
            if (running)
                return;

            // Finalizer thread starts before System.initializeSystemClass
            // is called.  Wait until JavaLangAccess is available
            //isBooted返回false表示JVM未初始化完成
            while (!VM.isBooted()) {
                // delay until VM completes initialization
                try {
                    //等待JVM初始化完成
                    VM.awaitBooted();
                } catch (InterruptedException x) {
                    // ignore and continue
                }
            }
            final JavaLangAccess jla = SharedSecrets.getJavaLangAccess();
            running = true;
            for (;;) {
                try {
                    //移除并返回链表头元素，如果为空则等待
                    Finalizer f = (Finalizer)queue.remove();
                    //执行Finalizer任务
                    f.runFinalizer(jla);
                } catch (InterruptedException x) {
                    // ignore and continue
                }
            }
        }
    }

    // FinalizerThread是通过静态static块启动的
    static {
        ThreadGroup tg = Thread.currentThread().getThreadGroup();
        for (ThreadGroup tgn = tg;
             tgn != null;
             tg = tgn, tgn = tg.getParent());
        Thread finalizer = new FinalizerThread(tg);
        finalizer.setPriority(Thread.MAX_PRIORITY - 2);
        finalizer.setDaemon(true);
        finalizer.start();
    }

}
