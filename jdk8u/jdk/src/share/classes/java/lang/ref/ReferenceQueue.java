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

import java.util.function.Consumer;

/**
 * 引用队列，在检测到适当的可到达性更改后，垃圾回收器将已注册的引用对象添加到该队列中
 *
 * ReferenceQueue主要用来通知Reference实例的使用方Reference实例对应的referent对象已经被回收掉了，允许使用方对Reference实例本身做适当的处理。
 * 注意ReferenceQueue本身并不直接持有Reference实例的引用，如果Reference实例本身变得不可达了，则无论Reference实例对应的referent对象被回收掉了，
 * Reference实例都不会被添加到ReferenceQueue中。
 *
 * ReferenceQueue跟正常的队列实现不同，ReferenceQueue依赖于Reference的next属性构成一个链表，链表头就是ReferenceQueue的静态head属性，
 * 加入到队列中实际就是插入到链表的头部。当Reference实例加入到ReferenceQueue中，Reference实例变成新的链表头，next属性就指向原来的链表头，
 * queue属性变成ENQUEUED，相关逻辑在enqueue方法中；当Reference实例从ReferenceQueue中移除时，next属性被重置为自己，原来的next属性变成新的链表头，queue属性变成NULL，相关逻辑在reallyPoll方法中。
 *
 * Reference queues, to which registered reference objects are appended by the
 * garbage collector after the appropriate reachability changes are detected.
 *
 * @author   Mark Reinhold
 * @since    1.2
 */

public class ReferenceQueue<T> {

    /**
     * Constructs a new reference-object queue.
     */
    public ReferenceQueue() { }

    private static class Null<S> extends ReferenceQueue<S> {
        boolean enqueue(Reference<? extends S> r) {
            return false;
        }
    }

    // 如果Reference实例的queue等于NULL，则表示该实例已经从队列中移除
    static ReferenceQueue<Object> NULL = new Null<>();
    // 如果Reference实例的queue等于ENQUEUED，则表示该实例已经加入到队列中
    static ReferenceQueue<Object> ENQUEUED = new Null<>();

    static private class Lock { };
    // 改写队列的锁
    private Lock lock = new Lock();
    // Reference链表的头部
    private volatile Reference<? extends T> head = null;
    // 表示Reference链表的长度
    private long queueLength = 0;

    // enqueue方法将某个Reference实例加入到队列中，要求获取lock属性的锁
    boolean enqueue(Reference<? extends T> r) { /* Called only by Reference class */
        synchronized (lock) {
            // Check that since getting the lock this reference hasn't already been
            // enqueued (and even then removed)
            ReferenceQueue<?> queue = r.queue;
            if ((queue == NULL) || (queue == ENQUEUED)) {
                return false;
            }
            assert queue == this;
            //将r插入到链表的头部，r的queue置为ENQUEUED，表示其已经加入到队列中，r的next属性置为它自己
            r.queue = ENQUEUED;
            //插入第一个元素时，head等于null，此时r的next属性就是r，插入以后的元素时，next属性就是head
            r.next = (head == null) ? r : head;
            head = r;
            queueLength++;
            if (r instanceof FinalReference) {
                //增加FinalReference的计数
                sun.misc.VM.addFinalRefCount(1);
            }
            //唤醒其他的等待线程
            lock.notifyAll();
            return true;
        }
    }

    // reallyPoll方法移除并返回链表的头部，也要求获取lock属性的锁
    private Reference<? extends T> reallyPoll() {       /* Must hold lock */
        Reference<? extends T> r = head;
        if (r != null) {
            @SuppressWarnings("unchecked")
            Reference<? extends T> rn = r.next;
            //将链表头从链表中移除，移除的Reference实例的queue会被置为NULL，next置为它自己
            head = (rn == r) ? null : rn;
            r.queue = NULL;
            r.next = r;
            queueLength--;
            if (r instanceof FinalReference) {
                //FinalReference的计数器减1
                sun.misc.VM.addFinalRefCount(-1);
            }
            return r;
        }
        return null;
    }

    /**
     * poll和remove这两方法都是移除并返回链表的头元素，区别在于poll方法不会阻塞，立即返回null，remove方法会阻塞当前线程，
     * 直到当前线程获取了一个Reference实例或者累计等待时间超过了指定时间，remove方法还有一个没有参数的重载版本，会阻塞当前线程，直到当前线程被唤醒，
     * 等待时间无限制，如果被唤醒了链表头还是null则返回null，即只等待一次。
     */

    /**
     * 从当前队列中移除并返回链表头元素，如果为空则返回null，WeakHashMap中就是调用此方法遍历队列中所有的Reference实例
     *
     * Polls this queue to see if a reference object is available.  If one is
     * available without further delay then it is removed from the queue and
     * returned.  Otherwise this method immediately returns <tt>null</tt>.
     *
     * @return  A reference object, if one was immediately available,
     *          otherwise <code>null</code>
     */
    public Reference<? extends T> poll() {
        if (head == null)
            //队列为空，返回null
            return null;
        synchronized (lock) {
            //获取队列的锁，移除并返回链表头元素
            return reallyPoll();
        }
    }

    /**
     * 移除并返回队列的头元素，最多等待timeout，如果timeout等于0，则只等待一次直到线程被唤醒，等待时间无限制
     *
     * Removes the next reference object in this queue, blocking until either
     * one becomes available or the given timeout period expires.
     *
     * <p> This method does not offer real-time guarantees: It schedules the
     * timeout as if by invoking the {@link Object#wait(long)} method.
     *
     * @param  timeout  If positive, block for up to <code>timeout</code>
     *                  milliseconds while waiting for a reference to be
     *                  added to this queue.  If zero, block indefinitely.
     *
     * @return  A reference object, if one was available within the specified
     *          timeout period, otherwise <code>null</code>
     *
     * @throws  IllegalArgumentException
     *          If the value of the timeout argument is negative
     *
     * @throws  InterruptedException
     *          If the timeout wait is interrupted
     */
    public Reference<? extends T> remove(long timeout)
        throws IllegalArgumentException, InterruptedException
    {
        if (timeout < 0) {
            throw new IllegalArgumentException("Negative timeout value");
        }
        synchronized (lock) {
            Reference<? extends T> r = reallyPoll();
            if (r != null) return r;
            long start = (timeout == 0) ? 0 : System.nanoTime();
            //相当于while死循环
            for (;;) {
                //等待最多timeout，如果timeout为0，则等待其他线程调用notify或者notifyAll
                lock.wait(timeout);
                //移除并返回链表头部元素
                r = reallyPoll();
                if (r != null) return r;
                if (timeout != 0) {
                    //检查是否等待超时
                    long end = System.nanoTime();
                    //如果timeout为0，则start为0，timeout算出来的就是一个负值，会立即返回null，即只wait一次
                    //如果timeout不为0，则可能wait多次，直到多次wait的累计时间大于设定的值，返回null
                    timeout -= (end - start) / 1000_000;
                    if (timeout <= 0) return null;
                    //继续下一次wait
                    start = end;
                }
            }
        }
    }

    /**
     * remove(long timeout)的重载版本，timeout固定传0
     *
     * Removes the next reference object in this queue, blocking until one
     * becomes available.
     *
     * @return A reference object, blocking until one becomes available
     * @throws  InterruptedException  If the wait is interrupted
     */
    public Reference<? extends T> remove() throws InterruptedException {
        return remove(0);
    }

    /**
     * forEach方法用于遍历链表中的所有的Reference实例，通常用于调试目的，要求调用方不能保持对Reference实例的引用，避免影响其正常销毁
     *
     * Iterate queue and invoke given action with each Reference.
     * Suitable for diagnostic purposes.
     * WARNING: any use of this method should make sure to not
     * retain the referents of iterated references (in case of
     * FinalReference(s)) so that their life is not prolonged more
     * than necessary.
     */
    void forEach(Consumer<? super Reference<? extends T>> action) {
        //注意执行forEach时不要求获取锁，所以读取的元素可能已经从队列中移除了
        for (Reference<? extends T> r = head; r != null;) {
            action.accept(r);
            @SuppressWarnings("unchecked")
            Reference<? extends T> rn = r.next;
            if (rn == r) {
                if (r.queue == ENQUEUED) {
                    // still enqueued -> we reached end of chain
                    //说明已经遍历到队列最后一个元素，将r置为null
                    r = null;
                } else {
                    // already dequeued: r.queue == NULL; ->
                    // restart from head when overtaken by queue poller(s)
                    //r.queue等于NULL，说明r已经从队列中移除了，需要从head开始重新遍历，以后r后面多个元素都可能被移除了，而且
                    //此时也无法获取下一个遍历元素的引用
                    r = head;
                }
            } else {
                // next in chain
                r = rn;
            }
        }
    }
}
