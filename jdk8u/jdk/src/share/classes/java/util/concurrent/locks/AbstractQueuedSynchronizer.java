/*
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

/*
 * This file is available under and governed by the GNU General Public
 * License version 2 only, as published by the Free Software Foundation.
 * However, the following notice accompanied the original version of this
 * file:
 *
 * Written by Doug Lea with assistance from members of JCP JSR-166
 * Expert Group and released to the public domain, as explained at
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

package java.util.concurrent.locks;
import java.util.concurrent.TimeUnit;
import java.util.ArrayList;
import java.util.Collection;
import java.util.Date;
import sun.misc.Unsafe;

/**
 * 当多个线程同时获取同一个锁的时候，没有获取到锁的线程需要排队等待，等锁被释放的时候，队列中的某个线程被唤醒，然后获取锁。AQS中就维护了这样一个同步队列（CLH队列）。
 *
 * AQS内部维护了一个FIFO队列来管理锁。线程首先会尝试获取锁，如果失败，则将当前线程以及等待状态等信息包成一个Node节点放入同步队列阻塞起来，当持有锁的线程释放锁时，就会唤醒队列中的后继线程。
 *
 * 站在使用者的角度，AQS的功能可以分为两类：独占功能和共享功能，它的所有子类中，要么实现并使用了它独占功能的API，要么使用了共享锁的功能，而不会同时使用两套API，
 * 即便是它最有名的子类ReentrantReadWriteLock，也是通过两个内部类：读锁和写锁，分别实现的两套API来实现的，
 * 为什么这么做，后面我们再分析，到目前为止，我们只需要明白AQS在功能上有独占控制和共享控制两种功能即可
 *
 * AQS中采用了一个state的状态位+一个FIFO的队列的方式，记录了锁的获取，释放等，这个state不一定用来代指锁，ReentrantLock用它来表示线程已经重复获取该锁的次数，
 * Semaphore用它来表示剩余的许可数量，FutureTask用它来表示任务的状态（尚未开始，正在运行，已完成以及以取消）。
 * 同时，在AQS中也看到了很多CAS的操作。AQS有两个功能：独占功能和共享功能，而ReentranLock就是AQS独占功能的体现，而CountDownLatch则是共享功能的体现
 *
 * 模版方法
 *  在实现同步组件的时候，对于内部的AQS的实现类按照需要重写可重写的方法，但是同步组件开放出来的方法中还是直接调用的AQS提供的模板方法。
 *
 *  这些模板方法同样是final的，这些模版方法包含了对上面的可重写方法的调用，以及后续处理，比如失败处理！
 *
 *  AQS的模板方法基本上分为3类：
 *
 *  1. 独占式获取与释放同步状态
 *  2. 共享式获取与释放同步状态
 *  3. 查询同步队列中的等待线程情况
 *      独占方式
 *          acquire(int arg)：独占式获取同步状态，如果当前线程获取同步状态成功，则由该方法返回，否则，将会进入同步队列等待。该方法不会响应中断。该方法内部调用了可重写的tryAcquire方法。
 *          acquireInterruptibly(int arg)：与acquire方法相同，但是该方法响应中断，当前线程未获取到同步状态而进入同步队列中，如果当前被中断，则该方法会抛出InterruptedException并返回。
 *          tryAcquireNanos(int arg,long nanos)：在acquireInterruptibly方法基础上增加了超时限制，如果当前线程在超时时间内没有获取到同步状态，那么将会返回false，获取到了返回true。
 *          release(int arg)：独占式的释放同步状态，该方法会在释放同步状态之后，将同步队列中第一个结点包含的线程唤醒。该方法内部调用了可重写的tryRelease方法。
 *      共享方式
 *          acquireShared(int arg)：共享式获取同步状态，如果当前线程未获取到同步状态，将会进入同步队列等待。与独占式的不同是同一时刻可以有多个线程获取到同步状态。该方法不会响应中断。该方法内部调用了可重写的tryAcquireShared方法。
 *          acquireSharedInterruptibly(int arg)：与acquireShared (int arg) 相同，但是该方法响应中断，当前线程未获取到同步状态而进入同步队列中，如果当前被中断，则该方法会抛出InterruptedException并返回。
 *          tryAcquireSharedNanos(int arg,long nanos)：在acquireSharedInterruptibly方法基础上增加了超时限制，如果当前线程在超时时间内没有获取到同步状态，那么将会返回false，获取到了返回true。
 *          releaseShared(int arg)：共享式释放同步状态，该方法会在释放同步状态之后，尝试唤醒同步队列中的后继节点中的线程。该方法内部调用了可重写的tryReleaseShared方法。
 *      获取线程等待情况
 *          getQueuedThreads()：获取等待在同步队列上的线程集合。
 *
 * Provides a framework for implementing blocking locks and related
 * synchronizers (semaphores, events, etc) that rely on
 * first-in-first-out (FIFO) wait queues.  This class is designed to
 * be a useful basis for most kinds of synchronizers that rely on a
 * single atomic {@code int} value to represent state. Subclasses
 * must define the protected methods that change this state, and which
 * define what that state means in terms of this object being acquired
 * or released.  Given these, the other methods in this class carry
 * out all queuing and blocking mechanics. Subclasses can maintain
 * other state fields, but only the atomically updated {@code int}
 * value manipulated using methods {@link #getState}, {@link
 * #setState} and {@link #compareAndSetState} is tracked with respect
 * to synchronization.
 *
 * <p>Subclasses should be defined as non-public internal helper
 * classes that are used to implement the synchronization properties
 * of their enclosing class.  Class
 * {@code AbstractQueuedSynchronizer} does not implement any
 * synchronization interface.  Instead it defines methods such as
 * {@link #acquireInterruptibly} that can be invoked as
 * appropriate by concrete locks and related synchronizers to
 * implement their public methods.
 *
 * <p>This class supports either or both a default <em>exclusive</em>
 * mode and a <em>shared</em> mode. When acquired in exclusive mode,
 * attempted acquires by other threads cannot succeed. Shared mode
 * acquires by multiple threads may (but need not) succeed. This class
 * does not &quot;understand&quot; these differences except in the
 * mechanical sense that when a shared mode acquire succeeds, the next
 * waiting thread (if one exists) must also determine whether it can
 * acquire as well. Threads waiting in the different modes share the
 * same FIFO queue. Usually, implementation subclasses support only
 * one of these modes, but both can come into play for example in a
 * {@link ReadWriteLock}. Subclasses that support only exclusive or
 * only shared modes need not define the methods supporting the unused mode.
 *
 * <p>This class defines a nested {@link ConditionObject} class that
 * can be used as a {@link Condition} implementation by subclasses
 * supporting exclusive mode for which method {@link
 * #isHeldExclusively} reports whether synchronization is exclusively
 * held with respect to the current thread, method {@link #release}
 * invoked with the current {@link #getState} value fully releases
 * this object, and {@link #acquire}, given this saved state value,
 * eventually restores this object to its previous acquired state.  No
 * {@code AbstractQueuedSynchronizer} method otherwise creates such a
 * condition, so if this constraint cannot be met, do not use it.  The
 * behavior of {@link ConditionObject} depends of course on the
 * semantics of its synchronizer implementation.
 *
 * <p>This class provides inspection, instrumentation, and monitoring
 * methods for the internal queue, as well as similar methods for
 * condition objects. These can be exported as desired into classes
 * using an {@code AbstractQueuedSynchronizer} for their
 * synchronization mechanics.
 *
 * <p>Serialization of this class stores only the underlying atomic
 * integer maintaining state, so deserialized objects have empty
 * thread queues. Typical subclasses requiring serializability will
 * define a {@code readObject} method that restores this to a known
 * initial state upon deserialization.
 *
 * <h3>Usage</h3>
 *
 * <p>To use this class as the basis of a synchronizer, redefine the
 * following methods, as applicable, by inspecting and/or modifying
 * the synchronization state using {@link #getState}, {@link
 * #setState} and/or {@link #compareAndSetState}:
 *
 * <ul>
 * <li> {@link #tryAcquire}
 * <li> {@link #tryRelease}
 * <li> {@link #tryAcquireShared}
 * <li> {@link #tryReleaseShared}
 * <li> {@link #isHeldExclusively}
 * </ul>
 *
 * Each of these methods by default throws {@link
 * UnsupportedOperationException}.  Implementations of these methods
 * must be internally thread-safe, and should in general be short and
 * not block. Defining these methods is the <em>only</em> supported
 * means of using this class. All other methods are declared
 * {@code final} because they cannot be independently varied.
 *
 * <p>You may also find the inherited methods from {@link
 * AbstractOwnableSynchronizer} useful to keep track of the thread
 * owning an exclusive synchronizer.  You are encouraged to use them
 * -- this enables monitoring and diagnostic tools to assist users in
 * determining which threads hold locks.
 *
 * <p>Even though this class is based on an internal FIFO queue, it
 * does not automatically enforce FIFO acquisition policies.  The core
 * of exclusive synchronization takes the form:
 *
 * <pre>
 * Acquire:
 *     while (!tryAcquire(arg)) {
 *        <em>enqueue thread if it is not already queued</em>;
 *        <em>possibly block current thread</em>;
 *     }
 *
 * Release:
 *     if (tryRelease(arg))
 *        <em>unblock the first queued thread</em>;
 * </pre>
 *
 * (Shared mode is similar but may involve cascading signals.)
 *
 * <p id="barging">Because checks in acquire are invoked before
 * enqueuing, a newly acquiring thread may <em>barge</em> ahead of
 * others that are blocked and queued.  However, you can, if desired,
 * define {@code tryAcquire} and/or {@code tryAcquireShared} to
 * disable barging by internally invoking one or more of the inspection
 * methods, thereby providing a <em>fair</em> FIFO acquisition order.
 * In particular, most fair synchronizers can define {@code tryAcquire}
 * to return {@code false} if {@link #hasQueuedPredecessors} (a method
 * specifically designed to be used by fair synchronizers) returns
 * {@code true}.  Other variations are possible.
 *
 * <p>Throughput and scalability are generally highest for the
 * default barging (also known as <em>greedy</em>,
 * <em>renouncement</em>, and <em>convoy-avoidance</em>) strategy.
 * While this is not guaranteed to be fair or starvation-free, earlier
 * queued threads are allowed to recontend before later queued
 * threads, and each recontention has an unbiased chance to succeed
 * against incoming threads.  Also, while acquires do not
 * &quot;spin&quot; in the usual sense, they may perform multiple
 * invocations of {@code tryAcquire} interspersed with other
 * computations before blocking.  This gives most of the benefits of
 * spins when exclusive synchronization is only briefly held, without
 * most of the liabilities when it isn't. If so desired, you can
 * augment this by preceding calls to acquire methods with
 * "fast-path" checks, possibly prechecking {@link #hasContended}
 * and/or {@link #hasQueuedThreads} to only do so if the synchronizer
 * is likely not to be contended.
 *
 * <p>This class provides an efficient and scalable basis for
 * synchronization in part by specializing its range of use to
 * synchronizers that can rely on {@code int} state, acquire, and
 * release parameters, and an internal FIFO wait queue. When this does
 * not suffice, you can build synchronizers from a lower level using
 * {@link java.util.concurrent.atomic atomic} classes, your own custom
 * {@link java.util.Queue} classes, and {@link LockSupport} blocking
 * support.
 *
 * <h3>Usage Examples</h3>
 *
 * <p>Here is a non-reentrant mutual exclusion lock class that uses
 * the value zero to represent the unlocked state, and one to
 * represent the locked state. While a non-reentrant lock
 * does not strictly require recording of the current owner
 * thread, this class does so anyway to make usage easier to monitor.
 * It also supports conditions and exposes
 * one of the instrumentation methods:
 *
 *  <pre> {@code
 * class Mutex implements Lock, java.io.Serializable {
 *
 *   // Our internal helper class
 *   private static class Sync extends AbstractQueuedSynchronizer {
 *     // Reports whether in locked state
 *     protected boolean isHeldExclusively() {
 *       return getState() == 1;
 *     }
 *
 *     // Acquires the lock if state is zero
 *     public boolean tryAcquire(int acquires) {
 *       assert acquires == 1; // Otherwise unused
 *       if (compareAndSetState(0, 1)) {
 *         setExclusiveOwnerThread(Thread.currentThread());
 *         return true;
 *       }
 *       return false;
 *     }
 *
 *     // Releases the lock by setting state to zero
 *     protected boolean tryRelease(int releases) {
 *       assert releases == 1; // Otherwise unused
 *       if (getState() == 0) throw new IllegalMonitorStateException();
 *       setExclusiveOwnerThread(null);
 *       setState(0);
 *       return true;
 *     }
 *
 *     // Provides a Condition
 *     Condition newCondition() { return new ConditionObject(); }
 *
 *     // Deserializes properly
 *     private void readObject(ObjectInputStream s)
 *         throws IOException, ClassNotFoundException {
 *       s.defaultReadObject();
 *       setState(0); // reset to unlocked state
 *     }
 *   }
 *
 *   // The sync object does all the hard work. We just forward to it.
 *   private final Sync sync = new Sync();
 *
 *   public void lock()                { sync.acquire(1); }
 *   public boolean tryLock()          { return sync.tryAcquire(1); }
 *   public void unlock()              { sync.release(1); }
 *   public Condition newCondition()   { return sync.newCondition(); }
 *   public boolean isLocked()         { return sync.isHeldExclusively(); }
 *   public boolean hasQueuedThreads() { return sync.hasQueuedThreads(); }
 *   public void lockInterruptibly() throws InterruptedException {
 *     sync.acquireInterruptibly(1);
 *   }
 *   public boolean tryLock(long timeout, TimeUnit unit)
 *       throws InterruptedException {
 *     return sync.tryAcquireNanos(1, unit.toNanos(timeout));
 *   }
 * }}</pre>
 *
 * <p>Here is a latch class that is like a
 * {@link java.util.concurrent.CountDownLatch CountDownLatch}
 * except that it only requires a single {@code signal} to
 * fire. Because a latch is non-exclusive, it uses the {@code shared}
 * acquire and release methods.
 *
 *  <pre> {@code
 * class BooleanLatch {
 *
 *   private static class Sync extends AbstractQueuedSynchronizer {
 *     boolean isSignalled() { return getState() != 0; }
 *
 *     protected int tryAcquireShared(int ignore) {
 *       return isSignalled() ? 1 : -1;
 *     }
 *
 *     protected boolean tryReleaseShared(int ignore) {
 *       setState(1);
 *       return true;
 *     }
 *   }
 *
 *   private final Sync sync = new Sync();
 *   public boolean isSignalled() { return sync.isSignalled(); }
 *   public void signal()         { sync.releaseShared(1); }
 *   public void await() throws InterruptedException {
 *     sync.acquireSharedInterruptibly(1);
 *   }
 * }}</pre>
 *
 * @since 1.5
 * @author Doug Lea
 */
public abstract class AbstractQueuedSynchronizer
    extends AbstractOwnableSynchronizer
    implements java.io.Serializable {

    private static final long serialVersionUID = 7373984972572414691L;

    /**
     * Creates a new {@code AbstractQueuedSynchronizer} instance
     * with initial synchronization state of zero.
     */
    protected AbstractQueuedSynchronizer() { }

    /**
     * 当共享资源被某个线程占有，其他请求该资源的线程将会阻塞，从而进入同步队列。
     * AQS 中的同步队列通过链表实现，下面的内部类 Node 便是其实现的载体
     *
     * Wait queue node class.
     *
     * <p>The wait queue is a variant of a "CLH" (Craig, Landin, and
     * Hagersten) lock queue. CLH locks are normally used for
     * spinlocks.  We instead use them for blocking synchronizers, but
     * use the same basic tactic of holding some of the control
     * information about a thread in the predecessor of its node.  A
     * "status" field in each node keeps track of whether a thread
     * should block.  A node is signalled when its predecessor
     * releases.  Each node of the queue otherwise serves as a
     * specific-notification-style monitor holding a single waiting
     * thread. The status field does NOT control whether threads are
     * granted locks etc though.  A thread may try to acquire if it is
     * first in the queue. But being first does not guarantee success;
     * it only gives the right to contend.  So the currently released
     * contender thread may need to rewait.
     *
     * <p>To enqueue into a CLH lock, you atomically splice it in as new
     * tail. To dequeue, you just set the head field.
     * <pre>
     *      +------+  prev +-----+       +-----+
     * head |      | <---- |     | <---- |     |  tail
     *      +------+       +-----+       +-----+
     * </pre>
     *
     * <p>Insertion into a CLH queue requires only a single atomic
     * operation on "tail", so there is a simple atomic point of
     * demarcation from unqueued to queued. Similarly, dequeuing
     * involves only updating the "head". However, it takes a bit
     * more work for nodes to determine who their successors are,
     * in part to deal with possible cancellation due to timeouts
     * and interrupts.
     *
     * <p>The "prev" links (not used in original CLH locks), are mainly
     * needed to handle cancellation. If a node is cancelled, its
     * successor is (normally) relinked to a non-cancelled
     * predecessor. For explanation of similar mechanics in the case
     * of spin locks, see the papers by Scott and Scherer at
     * http://www.cs.rochester.edu/u/scott/synchronization/
     *
     * <p>We also use "next" links to implement blocking mechanics.
     * The thread id for each node is kept in its own node, so a
     * predecessor signals the next node to wake up by traversing
     * next link to determine which thread it is.  Determination of
     * successor must avoid races with newly queued nodes to set
     * the "next" fields of their predecessors.  This is solved
     * when necessary by checking backwards from the atomically
     * updated "tail" when a node's successor appears to be null.
     * (Or, said differently, the next-links are an optimization
     * so that we don't usually need a backward scan.)
     *
     * <p>Cancellation introduces some conservatism to the basic
     * algorithms.  Since we must poll for cancellation of other
     * nodes, we can miss noticing whether a cancelled node is
     * ahead or behind us. This is dealt with by always unparking
     * successors upon cancellation, allowing them to stabilize on
     * a new predecessor, unless we can identify an uncancelled
     * predecessor who will carry this responsibility.
     *
     * <p>CLH queues need a dummy header node to get started. But
     * we don't create them on construction, because it would be wasted
     * effort if there is never contention. Instead, the node
     * is constructed and head and tail pointers are set upon first
     * contention.
     *
     * <p>Threads waiting on Conditions use the same nodes, but
     * use an additional link. Conditions only need to link nodes
     * in simple (non-concurrent) linked queues because they are
     * only accessed when exclusively held.  Upon await, a node is
     * inserted into a condition queue.  Upon signal, the node is
     * transferred to the main queue.  A special value of status
     * field is used to mark which queue a node is on.
     *
     * <p>Thanks go to Dave Dice, Mark Moir, Victor Luchangco, Bill
     * Scherer and Michael Scott, along with members of JSR-166
     * expert group, for helpful ideas, discussions, and critiques
     * on the design of this class.
     */
    static final class Node {
        /**
         * 用于标记一个节点在共享模式下等待
         * 共享模式下构造的结点，用来标记该线程是获取共享资源时被阻塞挂起后放入AQS 队列的
         *
         * Marker to indicate a node is waiting in shared mode
         */
        static final Node SHARED = new Node();

        /**
         * 用于标记一个节点在独占模式下等待
         * 独占模式下构造的结点，用来标记该线程是获取独占资源时被阻塞挂起后放入AQS 队列的
         *
         * Marker to indicate a node is waiting in exclusive mode
         */
        static final Node EXCLUSIVE = null;

        /**
         * 表明当前节点的线程已被取消
         * 当前线程因为超时或者中断被取消。这是一个终结态，也就是状态到此为止
         *
         * waitStatus value to indicate thread has cancelled
         */
        static final int CANCELLED =  1;

        /**
         * 表明下一个节点需要前一节点唤醒，这样下一个节点便可以安心睡眠了
         * 当前线程的后继线程被阻塞或者即将被阻塞，当前线程释放锁或者取消后需要唤醒后继线程。
         * 这个状态一般都是后继线程来设置前驱节点的
         *
         * waitStatus value to indicate successor's thread needs unparking
         */
        static final int SIGNAL    = -1;

        /**
         * 当前线程在condition队列中
         * 表明线程在等待条件，条件队列才用的上，如ReentrantLock的Condition
         *
         * waitStatus value to indicate thread is waiting on condition
         */
        static final int CONDITION = -2;

        /**
         * 用于将唤醒后继线程传递下去，这个状态的引入是为了完善和增强共享锁的唤醒机制。
         * 在一个节点成为头节点之前，是不会跃迁为此状态的
         * 表明下一个共享节点应该被无条件传播，当需要唤醒下一个共享节点时，会一直传播唤醒下一个直到非共享节点
         *
         * waitStatus value to indicate the next acquireShared should
         * unconditionally propagate
         */
        static final int PROPAGATE = -3;

        /**
         * 等待状态，该变量用于描述节点的状态，为什么需要这个状态呢？
         * 原因是：AQS的队列中，在有并发时，肯定会存取一定数量的节点，每个节点[G4] 代表了一个线程的状态，有的线程可能“等不及”获取锁了，需要放弃竞争，
         * 退出队列，有的线程在等待一些条件满足，满足后才恢复执行（这里的描述很像某个J.U.C包下的工具类，ReentrantLock的Condition，
         * 事实上，Condition同样也是AQS的子类）等等，总之，各个线程有各个线程的状态，但总需要一个变量来描述它，这个变量就叫waitStatus,它有四种状态：
         *  　　1. 节点取消-CANCELLED
         *  　　2. 节点等待触发-SIGNAL
         *  　　3. 节点等待条件-CONDITION
         *  　　4. 节点状态需要向后传播-PROPAGATE
         *  只有当前节点的前一个节点为SIGNAL时，才能当前节点才能被挂起。
         *
         * Status field, taking on only the values:
         *   SIGNAL:     The successor of this node is (or will soon be)
         *               blocked (via park), so the current node must
         *               unpark its successor when it releases or
         *               cancels. To avoid races, acquire methods must
         *               first indicate they need a signal,
         *               then retry the atomic acquire, and then,
         *               on failure, block.
         *   CANCELLED:  This node is cancelled due to timeout or interrupt.
         *               Nodes never leave this state. In particular,
         *               a thread with cancelled node never again blocks.
         *   CONDITION:  This node is currently on a condition queue.
         *               It will not be used as a sync queue node
         *               until transferred, at which time the status
         *               will be set to 0. (Use of this value here has
         *               nothing to do with the other uses of the
         *               field, but simplifies mechanics.)
         *   PROPAGATE:  A releaseShared should be propagated to other
         *               nodes. This is set (for head node only) in
         *               doReleaseShared to ensure propagation
         *               continues, even if other operations have
         *               since intervened.
         *   0:          None of the above
         *
         * The values are arranged numerically to simplify use.
         * Non-negative values mean that a node doesn't need to
         * signal. So, most code doesn't need to check for particular
         * values, just for sign.
         *
         * The field is initialized to 0 for normal sync nodes, and
         * CONDITION for condition nodes.  It is modified using CAS
         * (or when possible, unconditional volatile writes).
         */
        volatile int waitStatus;

        /**
         * 前驱节点
         *
         * Link to predecessor node that current node/thread relies on
         * for checking waitStatus. Assigned during enqueuing, and nulled
         * out (for sake of GC) only upon dequeuing.  Also, upon
         * cancellation of a predecessor, we short-circuit while
         * finding a non-cancelled one, which will always exist
         * because the head node is never cancelled: A node becomes
         * head only as a result of successful acquire. A
         * cancelled thread never succeeds in acquiring, and a thread only
         * cancels itself, not any other node.
         */
        volatile Node prev;

        /**
         * 后继节点
         *
         * Link to the successor node that the current node/thread
         * unparks upon release. Assigned during enqueuing, adjusted
         * when bypassing cancelled predecessors, and nulled out (for
         * sake of GC) when dequeued.  The enq operation does not
         * assign next field of a predecessor until after attachment,
         * so seeing a null next field does not necessarily mean that
         * node is at end of queue. However, if a next field appears
         * to be null, we can scan prev's from the tail to
         * double-check.  The next field of cancelled nodes is set to
         * point to the node itself instead of null, to make life
         * easier for isOnSyncQueue.
         */
        volatile Node next;

        /**
         * 节点对应的线程
         *
         * The thread that enqueued this node.  Initialized on
         * construction and nulled out after use.
         */
        volatile Thread thread;

        /**
         * 等待队列中的后继节点，如果当前结点是共享模式的，那么这个字段是一个SHARED常量
         * 在独占锁模式下永远为null，仅仅起到一个标记作用，没有实际意义。
         *
         * Link to next node waiting on condition, or the special
         * value SHARED.  Because condition queues are accessed only
         * when holding in exclusive mode, we just need a simple
         * linked queue to hold nodes while they are waiting on
         * conditions. They are then transferred to the queue to
         * re-acquire. And because conditions can only be exclusive,
         * we save a field by using special value to indicate shared
         * mode.
         */
        Node nextWaiter;

        /**
         * 当前节点是否处于共享模式等待
         * 如果是共享模式下等待，那么返回true（因为上面的Node nextWaiter字段在共享模式下是一个SHARED常量）
         *
         * Returns true if node is waiting in shared mode.
         */
        final boolean isShared() {
            return nextWaiter == SHARED;
        }

        /**
         * 获取前驱节点，如果为空的话抛出空指针异常
         *
         * Returns previous node, or throws NullPointerException if null.
         * Use when predecessor cannot be null.  The null check could
         * be elided, but is present to help the VM.
         *
         * @return the predecessor of this node
         */
        final Node predecessor() throws NullPointerException {
            Node p = prev;
            if (p == null)
                throw new NullPointerException();
            else
                return p;
        }

        Node() {    // Used to establish initial head or SHARED marker
        }

        /**
         * addWaiter方法会调用此构造函数
         */
        Node(Thread thread, Node mode) {     // Used by addWaiter
            this.nextWaiter = mode;
            this.thread = thread;
        }

        /**
         * Condition会调用此构造函数
         */
        Node(Thread thread, int waitStatus) { // Used by Condition
            this.waitStatus = waitStatus;
            this.thread = thread;
        }
    }

    /**
     * 队列的头节点的指针。
     * 除了初始化之外，它只能通过setHead方法进行修改。注意:如果head存在，它的waitStatus保证不会被取消
     *
     * AQS中保持的对同步队列的引用
     * 队列头结点，实际上是一个哨兵结点，不代表任何线程，head所指向的Node的thread属性永远是null。
     *
     * Head of the wait queue, lazily initialized.  Except for
     * initialization, it is modified only via method setHead.  Note:
     * If head exists, its waitStatus is guaranteed not to be
     * CANCELLED.
     */
    private transient volatile Node head;

    /**
     * 队列的尾节点的指针。
     * 等待队列的尾部，懒初始化，之后只在enq方法加入新节点时修改
     *
     * Tail of the wait queue, lazily initialized.  Modified only via
     * method enq to add new wait node.
     */
    private transient volatile Node tail;

    /**
     * 同步器的状态
     * volatile修饰， 标识同步状态，state为0表示锁空闲，state>0表示锁被持有，可以大于1，表示被重入
     *
     * The synchronization state.
     */
    private volatile int state;

    /**
     * Returns the current value of synchronization state.
     * This operation has memory semantics of a {@code volatile} read.
     * @return current state value
     */
    protected final int getState() {
        return state;
    }

    /**
     * Sets the value of synchronization state.
     * This operation has memory semantics of a {@code volatile} write.
     * @param newState the new state value
     */
    protected final void setState(int newState) {
        state = newState;
    }

    /**
     * Atomically sets synchronization state to the given updated
     * value if the current state value equals the expected value.
     * This operation has memory semantics of a {@code volatile} read
     * and write.
     *
     * @param expect the expected value
     * @param update the new value
     * @return {@code true} if successful. False return indicates that the actual
     *         value was not equal to the expected value.
     */
    protected final boolean compareAndSetState(int expect, int update) {
        // See below for intrinsics setup to support this
        return unsafe.compareAndSwapInt(this, stateOffset, expect, update);
    }

    // Queuing utilities

    /**
     * The number of nanoseconds for which it is faster to spin
     * rather than to use timed park. A rough estimate suffices
     * to improve responsiveness with very short timeouts.
     */
    static final long spinForTimeoutThreshold = 1000L;

    /**
     * 通过循环+CAS在队列中成功插入一个节点后返回
     *
     * Inserts node into queue, initializing if necessary. See picture above.
     * @param node the node to insert
     * @return node's predecessor
     */
    private Node enq(final Node node) {
        // 进入死循环
        for (;;) {
            Node t = tail;
            // 如果尾节点为null，说明队列为空
            // 初始化head和tail
            if (t == null) { // Must initialize
                // 此时通过CAS增加一个头结点，并且tail也指向头结点，之后下一次循环
                if (compareAndSetHead(new Node()))
                    // 这里tail和head都指向一个头节点...
                    tail = head;
            } else {
                // 否则，把当前线程的node插入到尾节点的后面
                /*
                 * AQS的精妙在于很多细节代码，比如需要用CAS往队尾里增加一个元素
                 * 此处的else分支是先在CAS的if前设置node.prev = t，而不是在CAS成功之后再设置。
                 * 一方面是基于CAS的双向链表插入目前没有完美的解决方案，另一方面这样子做的好处是：
                 * 保证每时每刻tail.prev都不会是一个null值，否则如果node.prev = t
                 * 放在下面if的里面，会导致一个瞬间tail.prev = null，这样会使得队列不完整
                 */
                node.prev = t;
                // CAS设置tail为node，成功后把老的tail也就是t连接到node
                if (compareAndSetTail(t, node)) {
                    t.next = node;
                    // 并返回插入结点的前一个节点
                    return t;
                }
            }
        }
    }

    /**
     * 在队列中新增一个节点
     * addWaiter方法负责把当前无法获得锁的线程包装为一个Node添加到队尾
     *
     * 其中参数mode是独占锁还是共享锁，默认为null，独占锁。追加到队尾的动作分两步：
     *  1. 如果当前队尾已经存在(tail!=null)，则使用CAS把当前线程更新为Tail
     *  2. 如果当前Tail为null或则线程调用CAS设置队尾失败，则通过enq方法继续设置Tail
     *
     * Creates and enqueues node for current thread and given mode.
     *
     * @param mode Node.EXCLUSIVE for exclusive, Node.SHARED for shared
     * @return the new node
     */
    private Node addWaiter(Node mode) {
        // 把当前线程按照指定的模式包装成1个Node
        Node node = new Node(Thread.currentThread(), mode);
        // 快速尝试
        // Try the fast path of enq; backup to full enq on failure
        // 用pred表示队列中的尾节点
        Node pred = tail;
        // 如果尾节点不为空
        if (pred != null) {
            node.prev = pred;
            // 通过CAS在队尾插入当前节点
            // 通过CAS操作把node插入到列表的尾部，并把尾节点指向node如果失败，说明有并发，此时调用enq
            if (compareAndSetTail(pred, node)) {
                pred.next = node;
                return node;
            }
        }
        // 初始情况或者在快速尝试失败后插入节点
        // 如果队列为空，或者CAS失败，进入enq中死循环，“自旋”方式修改。
        enq(node);
        return node;
    }

    /**
     * 将指定的node设置为head节点，该node节点所包装的thread与前驱节点都置为null
     *
     * Sets head of queue to be node, thus dequeuing. Called only by
     * acquire methods.  Also nulls out unused fields for sake of GC
     * and to suppress unnecessary signals and traversals.
     *
     * @param node the node
     */
    private void setHead(Node node) {
        head = node;
        node.thread = null;
        node.prev = null;
    }

    /**
     * 用于唤醒参数结点的某个非取消的后继结点，该方法在很多地方法都被调用，大概步骤：
     *
     *  如果当前结点的状态小于0，那么CAS设置为0，表示后继结点可以继续尝试获取锁。
     *  如果当前结点的后继s为null或者状态为取消CANCELLED，则将s先指向null；然后从tail开始到node之间倒序向前查找，找到离tail最远的非取消结点赋给s。需要从后向前遍历，因为同步队列只保证结点前驱关系的正确性。
     *  如果s不为null，那么状态肯定不是取消CANCELLED，则直接唤醒s的线程，调用LockSupport.unpark方法唤醒，被唤醒的结点将从被park的位置继续执行！
     *
     * Wakes up node's successor, if one exists.
     *
     * @param node the node
     */
    private void unparkSuccessor(Node node) {
        /*
         * If status is negative (i.e., possibly needing signal) try
         * to clear in anticipation of signalling.  It is OK if this
         * fails or if status is changed by waiting thread.
         */
        int ws = node.waitStatus;
        // 如果当前结点的状态小于0，那么CAS设置为0，表示后继结点线程可以先尝试获锁，而不是直接挂起。
        // 尝试将node的等待状态置为0,这样的话,后继争用线程可以有机会再尝试获取一次锁
        if (ws < 0)
            compareAndSetWaitStatus(node, ws, 0);

        /*
         * 先获取node的直接后继
         *
         * Thread to unpark is held in successor, which is normally
         * just the next node.  But if cancelled or apparently null,
         * traverse backwards from tail to find the actual
         * non-cancelled successor.
         */
        Node s = node.next;
        /*
         * 这里的逻辑就是如果node.next存在并且状态不为取消，则直接唤醒s即可
         * 否则需要从tail开始向前找到node之后最近的非取消节点。
         *
         * 这里为什么要从tail开始向前查找也是值得琢磨的:
         * 如果读到s == null，不代表node就为tail，参考addWaiter以及enq函数中的我的注释。
         * 不妨考虑到如下场景：
         * 1. node某时刻为tail
         * 2. 有新线程通过addWaiter中的if分支或者enq方法添加自己
         * 3. compareAndSetTail成功
         * 4. 此时这里的Node s = node.next读出来s == null，但事实上node已经不是tail，它有后继了!
         */
        if (s == null || s.waitStatus > 0) {
            s = null;
            for (Node t = tail; t != null && t != node; t = t.prev)
                if (t.waitStatus <= 0)
                    s = t;
        }
        // 如果s不为null，那么状态肯定不是取消CANCELLED，则直接唤醒s的线程，调用LockSupport.unpark方法唤醒，被唤醒的结点将从被park的位置向后执行！
        if (s != null)
            LockSupport.unpark(s.thread);
    }

    /**
     * 这是共享锁中的核心唤醒函数，主要做的事情就是唤醒下一个线程或者设置传播状态。
     * 后继线程被唤醒后，会尝试获取共享锁，如果成功之后，则又会调用setHeadAndPropagate,将唤醒传播下去。
     * 这个函数的作用是保障在acquire和release存在竞争的情况下，保证队列中处于等待状态的节点能够有办法被唤醒。
     *
     * 尝试唤醒一个后继线程，被唤醒的线程会尝试获取共享锁，如果成功之后，则又会有可能调用setHeadAndPropagate，将唤醒传播下去。
     * 独占锁只有在一个线程释放所之后才会唤醒下一个线程，而共享锁在一个线程在获取到锁和释放掉锁锁之后，都可能会调用这个方法唤醒下一个线程
     * 因为在共享锁模式下，锁可以被多个线程所共同持有，既然当前线程已经拿到共享锁了，那么就可以直接通知后继结点来获取锁，而不必等待锁被释放的时候再通知。
     *
     * Release action for shared mode -- signals successor and ensures
     * propagation. (Note: For exclusive mode, release just amounts
     * to calling unparkSuccessor of head if it needs signal.)
     */
    private void doReleaseShared() {
        /*
         * 一个死循环，跳出循环的条件就是最下面的break
         *
         * 以下的循环做的事情就是，在队列存在后继线程的情况下，唤醒后继线程；
         * 或者由于多线程同时释放共享锁由于处在中间过程，读到head节点等待状态为0的情况下，
         * 虽然不能unparkSuccessor，但为了保证唤醒能够正确稳固传递下去，设置节点状态为PROPAGATE。
         * 这样的话获取锁的线程在执行setHeadAndPropagate时可以读到PROPAGATE，从而由获取锁的线程去释放后继等待线程
         *
         * Ensure that a release propagates, even if there are other
         * in-progress acquires/releases.  This proceeds in the usual
         * way of trying to unparkSuccessor of head if it needs
         * signal. But if it does not, status is set to PROPAGATE to
         * ensure that upon release, propagation continues.
         * Additionally, we must loop in case a new node is added
         * while we are doing this. Also, unlike other uses of
         * unparkSuccessor, we need to know if CAS to reset status
         * fails, if so rechecking.
         */
        for (;;) {
            // 获取当前的head，每次循环读取最新的head
            Node h = head;
            // 如果队列中存在后继线程。
            // 如果h不为null且h不为tail，表示队列至少有两个结点，那么尝试唤醒head后继结点线程
            if (h != null && h != tail) {
                int ws = h.waitStatus;
                // 如果头结点的状态为SIGNAL，那么表示后继结点需要被唤醒
                if (ws == Node.SIGNAL) {
                    // 尝试CAS设置h的状态从Node.SIGNAL变成0
                    // 可能存在多线程操作，但是只会有一条成功
                    if (!compareAndSetWaitStatus(h, Node.SIGNAL, 0))
                        // 失败的线程结束本次循环，继续下一次循环
                        continue;            // loop to recheck cases
                    // 成功的那一条线程会调用unparkSuccessor方法唤醒head的一个没有取消的后继结点
                    // 对于一个head，只需要一条线程去唤醒该head的后继就行了。上面的CAS就是保证unparkSuccessor方法对于一个head只执行一次
                    unparkSuccessor(h);
                }
                // 如果h节点的状态为0，需要设置为PROPAGATE用以保证唤醒的传播。
                // 如果h状态为0，那说明后继结点线程已经是唤醒状态了或者将会被唤醒，不需要该线程来唤醒
                // 那么尝试设置h状态从0变成PROPAGATE，如果失败则继续下一次循环，此时设置PROPAGATE状态能保证唤醒操作能够传播下去
                // 因为后继结点成为头结点时，在setHeadAndPropagate方法中能够读取到原head结点的PROPAGATE状态<0，从而让它可以尝试唤醒后继结点（如果存在）
                else if (ws == 0 &&
                         !compareAndSetWaitStatus(h, 0, Node.PROPAGATE))
                    // 失败的线程结束本次循环，继续下一次循环
                    continue;                // loop on failed CAS
            }
            // 检查h是否仍然是head，如果不是的话需要再进行循环。
            // 执行到这一步说明在上面的判断中队列可能只有一个结点，或者unparkSuccessor方法调用完毕，或h状态为PROPAGATE(不需要继续唤醒后继)
            // 再次检查h是否仍然是最新的head，如果不是的话需要再进行循环；如果是的话说明head没有变化，退出循环
            if (h == head)                   // loop if head changed
                break;
        }
    }

    /**
     * 这个函数做的事情有两件:
     * 1. 在获取共享锁成功后，设置head节点
     * 2. 根据调用tryAcquireShared返回的状态以及节点本身的等待状态来判断是否需要唤醒后继线程
     *
     * 相比于setHead方法，在设置head之后多执行了一步propagate操作：
     *
     *  1. 和setHead方法一样设置新head结点信息
     *  2. 根据传播状态判断是否要唤醒后继结点。
     *
     * Sets head of queue, and checks if successor may be waiting
     * in shared mode, if so propagating if either propagate > 0 or
     * PROPAGATE status was set.
     *
     * @param node the node
     * @param propagate the return value from a tryAcquireShared
     */
    private void setHeadAndPropagate(Node node, int propagate) {
        // 把当前的head封闭在方法栈上，用以下面的条件检查
        Node h = head; // Record old head for check below
        setHead(node);
        /*
         * propagate是tryAcquireShared的返回值，这是决定是否传播唤醒的依据之一。
         * h.waitStatus为SIGNAL或者PROPAGATE时也根据node的下一个节点共享来决定是否传播唤醒，
         * 这里为什么不能只用propagate > 0来决定是否可以传播在本文下面的思考问题中有相关讲述
         *
         * Try to signal next queued node if:
         *   Propagation was indicated by caller,
         *     or was recorded (as h.waitStatus either before
         *     or after setHead) by a previous operation
         *     (note: this uses sign-check of waitStatus because
         *      PROPAGATE status may transition to SIGNAL.)
         * and
         *   The next node is waiting in shared mode,
         *     or we don't know, because it appears null
         *
         * The conservatism in both of these checks may cause
         * unnecessary wake-ups, but only when there are multiple
         * racing acquires/releases, so most need signals now or soon
         * anyway.
         */
        if (propagate > 0 || h == null || h.waitStatus < 0 ||
            (h = head) == null || h.waitStatus < 0) {
            Node s = node.next;
            if (s == null || s.isShared())
                doReleaseShared();
        }
    }

    // Utilities for various versions of acquire

    /**
     * 该方法实现某个node取消获取锁，取消获取锁请求
     * 由于独占式可中断获取锁的方法中，线程被中断而抛出异常的情况比较常见，因此这里分析finally中cancelAcquire的源码。
     * cancelAcquire方法用于取消结点获取锁的请求，参数为需要取消的结点node，大概步骤为：
     *
     *  1. node记录的线程thread置为null
     *  2. 跳过已取消的前置结点。由node向前查找，直到找到一个状态小于等于0的结点pred (即找一个没有取消的结点)，更新node.prev为找到的pred。
     *  3. node的等待状态waitStatus置为CANCELLED，即取消请求锁。
     *  4. 如果node是尾结点，那么尝试CAS更新tail指向pred，成功之后继续CAS设置pred.next为null。
     *  5. 否则，说明node不是尾结点或者CAS失败(可能存在对尾结点的并发操作)：
     *      1. 如果node不是head的后继 并且 (pred的状态为SIGNAL或者将pred的waitStatus置为SIGNAL成功) 并且 pred记录的线程不为null。那么设置pred.next指向node.next。最后node.next指向node自己。
     *      2. 否则，说明node是head的后继 或者pred状态设置失败 或者 pred记录的线程为null。那么调用unparkSuccessor唤醒node的一个没取消的后继结点。最后node.next指向node自己。
     *
     * Cancels an ongoing attempt to acquire.
     *
     * @param node the node
     */
    private void cancelAcquire(Node node) {
        // Ignore if node doesn't exist
        if (node == null)
            return;

        // 1. node记录的线程thread置为null
        node.thread = null;

        /*
         * 2. 类似于shouldParkAfterFailedAcquire方法中查找有效前驱的代码:
         *   do {
         *       node.prev = pred = pred.prev;
         *   } while (pred.waitStatus > 0);
         *   pred.next = node;
         *
         *   这里同样由node向前查找，直到找到一个状态小于等于0的结点(即没有被取消的结点)，作为前驱
         *   但是这里只更新了node.prev,没有更新pred.next
         */

        // Skip cancelled predecessors
        // 遍历并更新节点前驱，把node的prev指向前部第一个非取消节点
        Node pred = node.prev;
        while (pred.waitStatus > 0)
            node.prev = pred = pred.prev;

        // predNext is the apparent node to unsplice. CASes below will
        // fail if not, in which case, we lost race vs another cancel
        // or signal, so no further action is necessary.
        // 记录pred节点的后继为predNext，后续CAS会用到
        Node predNext = pred.next;

        // Can use unconditional write instead of CAS here.
        // After this atomic step, other Nodes can skip past us.
        // Before, we are free of interference from other threads.
        // 3. 直接把当前节点的等待状态置为取消(取消请求锁)，后继节点即便也在cancel可以跨越node节点
        node.waitStatus = Node.CANCELLED;

        // If we are the tail, remove ourselves.
        /*
         * 4. 如果当前结点是尾结点，那么尝试CAS更新tail指向pred，成功之后继续CAS设置pred.next为null。
         *
         * 如果CAS将tail从node置为pred节点了
         * 则剩下要做的事情就是尝试用CAS将pred节点的next更新为null以彻底切断pred和node的联系。
         * 这样一来就断开了pred与pred的所有后继节点，这些节点由于变得不可达，最终会被回收掉。
         * 由于node没有后继节点，所以这种情况到这里整个cancel就算是处理完毕了。
         */
        if (node == tail && compareAndSetTail(node, pred)) {
            // 这里的CAS更新pred的next即使失败了也没关系，说明有其它新入队线程或者其它取消线程更新掉了。
            compareAndSetNext(pred, predNext, null);
        } else {
            // 5. 否则，说明node不是尾结点或者CAS失败(可能存在对尾结点的并发操作)，这种情况要做的事情是把pred和node的后继非取消结点拼起来。
            // If successor needs signal, try to set pred's next-link
            // so it will get one. Otherwise wake it up to propagate.
            // 如果node还有后继节点，这种情况要做的事情是把pred和后继非取消节点拼起来
            int ws;
            /*
             * 5.1 如果node不是head的后继 并且 (pred的状态为SIGNAL或者将pred的waitStatus置为SIGNAL成功) 并且 pred记录的线程不为null。
             *   那么设置pred.next指向node.next。这里没有设置prev，但是没关系。
             *   此时pred的后继变成了node的后继—next，后续next结点如果获取到锁，那么在shouldParkAfterFailedAcquire方法中查找有效前驱时，
             *   也会找到这个没取消的pred，同时将next.prev指向pred，也就设置了prev关系了。
             */
            if (pred != head &&
                ((ws = pred.waitStatus) == Node.SIGNAL ||
                 (ws <= 0 && compareAndSetWaitStatus(pred, ws, Node.SIGNAL))) &&
                pred.thread != null) {
                // 获取next结点
                Node next = node.next;
                /*
                 * 如果next结点存在且未被取消
                 * 如果node的后继节点next非取消状态的话，则用CAS尝试把pred的后继置为node的后继节点
                 * 这里if条件为false或者CAS失败都没关系，这说明可能有多个线程在取消，总归会有一个能成功的
                 */
                if (next != null && next.waitStatus <= 0)
                    // 那么CAS设置perd.next指向node.next
                    compareAndSetNext(pred, predNext, next);
            } else {
                /*
                 * 5.2 否则，说明node是head的后继 或者pred状态设置失败 或者 pred记录的线程为null。
                 *
                 * 此时需要调用unparkSuccessor方法尝试唤醒node结点的后继结点，因为node作为head的后继结点是唯一有资格取尝试获取锁的结点。
                 * 如果外部线程A释放锁，但是还没有调用unpark唤醒node的时候，此时node被中断或者发生异常，这时node将会调用cancelAcquire取消，结点内部的记录线程变成null，
                 * 此时就是算A线程的unpark方法执行，也只是LockSupport.unpark(null)而已，也就不会唤醒任何结点了
                 * 那么node后面的结点也不会被唤醒了，队列就失活了；如果在这种情况下，在node将会调用cancelAcquire取消的代码中
                 * 调用一次unparkSuccessor，那么将唤醒被取消结点的后继结点，让后继结点可以尝试获取锁，从而保证队列活性！
                 *
                 * 前面对node进行取消的代码中，并没有将node彻底移除队列，
                 * 而被唤醒的结点会尝试获取锁，而在在获取到锁之后，在
                 * setHead(node);
                 * p.next = null; // help GC
                 * 部分，可能将这些被取消的结点清除
                 *
                 */
                unparkSuccessor(node);
            }

            /*
             * 最后node.next指向node自身，方便后续GC时直接销毁无效结点
             *        同时也是为了Condition的isOnSyncQueue方法，判断一个原先属于条件队列的结点是否转移到了同步队列。
             *        因为同步队列中会用到结点的next域，取消结点的next也有值的话，可以断言next域有值的结点一定在同步队列上。
             *        这里也能看出来，遍历的时候应该采用倒序遍历，否则采用正序遍历可能出现死循环
             */
            node.next = node; // help GC
        }
    }

    /**
     * 根据前驱节点中的waitStatus来判断是否需要阻塞当前线程
     *
     * shouldParkAfterFailedAcquire方法在没有获取到锁之后调用，用于判断当前结点是否需要被挂起。大概步骤如下：
     *
     * 如果前驱结点已经是SIGNAL(-1)状态，即表示当前结点可以挂起，返回true，方法结束；
     * 否则，如果前驱结点状态大于0，即 Node.CANCELLED，表示前驱结点放弃了锁的等待，那么由该前驱向前查找，直到找到一个状态小于等于0的结点，当前结点排在该结点后面，返回false，方法结束；
     * 否则，前驱结点的状态既不是SIGNAL(-1)，也不是CANCELLED(1)，尝试CAS设置前驱结点的状态为SIGNAL(-1)，返回false，方法结束！
     * 只有前驱结点状态为SIGNAL时，当前结点才能安心挂起，否则一直自旋！
     *
     * 从这里能看出来，一个结点的SIGNAL状态一般都是由它的后继结点设置的，但是这个状态却是表示后继结点的状态，表示的意思就是前驱结点如果释放了锁，那么就有义务唤醒后继结点！
     *
     *
     * Checks and updates status for a node that failed to acquire.
     * Returns true if thread should block. This is the main signal
     * control in all acquire loops.  Requires that pred == node.prev.
     *
     * @param pred node's predecessor holding status
     * @param node the node
     * @return {@code true} if thread should block
     */
    private static boolean shouldParkAfterFailedAcquire(Node pred, Node node) {
        // 获取 前驱的waitStatus_等待状态
        // 回顾创建结点时候,并没有给waitStatus赋值，因此每一个结点最开始的时候waitStatus的值都为0
        int ws = pred.waitStatus;
        // 如果前驱结点已经是SIGNAL状态，即表示当前结点可以挂起
        if (ws == Node.SIGNAL)
            /*
             * 前驱节点已经设置为SIGNAL状态，在释放锁的时候会唤醒后继节点，
             * 所以后继节点（也就是当前节点）现在可以阻塞自己
             *
             * This node has already set status asking a release
             * to signal it, so it can safely park.
             */
            return true;
        // 如果前驱结点状态大于0，即 Node.CANCELLED 表示前驱结点放弃了锁的等待
        if (ws > 0) {
            /*
             * 前驱节点状态为取消，向前遍历，更新当前节点的前驱为往前第一个非取消节点。
             * 当前线程会之后会再次回到循环并尝试获取锁
             * 由该前驱向前查找，直到找到一个状态小于等于0的结点(即没有被取消的结点)，当前结点成为该结点的后驱，这一步很重要，可能会清理一段被取消了的结点，并且如果该前驱释放了锁，还会唤醒它的后继，保持队列活性
             *
             * Predecessor was cancelled. Skip over predecessors and
             * indicate retry.
             */
            do {
                node.prev = pred = pred.prev;
            } while (pred.waitStatus > 0);
            pred.next = node;
        } else { // 否则，前驱结点的状态既不是SIGNAL(-1)，也不是CANCELLED(1)
            /*
             * 等待状态为0或者PROPAGATE(-3)，设置前驱的等待状态为SIGNAL,
             * 并且之后会回到循环再次重试获取锁
             * 前驱结点的状态CAS设置为SIGNAL(-1)，可能失败，但没关系，因为失败之后会一直循环
             *
             * waitStatus must be 0 or PROPAGATE.  Indicate that we
             * need a signal, but don't park yet.  Caller will need to
             * retry to make sure it cannot acquire before parking.
             */
            compareAndSetWaitStatus(pred, ws, Node.SIGNAL);
        }
        // 返回false，表示当前结点不能挂起
        return false;
    }

    /**
     * Convenience method to interrupt current thread.
     */
    static void selfInterrupt() {
        Thread.currentThread().interrupt();
    }

    /**
     * parkAndCheckInterrupt挂起线程&判断中断状态
     *
     * Convenience method to park and then check if interrupted
     *
     * @return {@code true} if interrupted
     */
    private final boolean parkAndCheckInterrupt() {
        // 使用LockSupport.park(this)挂起该线程，不再执行后续的步骤、代码。直到该线程被中断或者被唤醒（unpark）
        LockSupport.park(this);
        // 如果该线程被中断或者唤醒，那么返回Thread.interrupted()方法的返回值，该方法用于判断前线程的中断状态，并且清除该中断状态，
        // 即如果该线程因为被中断而唤醒，则中断状态为true，将中断状态重置为false，并返回true，如果该线程不是因为中断被唤醒，则中断状态为false，并返回false。
        return Thread.interrupted();
    }

    /*
     * Various flavors of acquire, varying in exclusive/shared and
     * control modes.  Each is mostly the same, but annoyingly
     * different.  Only a little bit of factoring is possible due to
     * interactions of exception mechanics (including ensuring that we
     * cancel if tryAcquire throws exception) and other control, at
     * least not without hurting performance too much.
     */

    /**
     * 在队列中的节点通过此方法获取锁
     * 获取锁，这里如果获取不到也会被阻塞，也会返回当前线程是否被中断，如果被中断设置中断模式。
     *
     * Acquires in exclusive uninterruptible mode for thread already in
     * queue. Used by condition wait methods as well as acquire.
     *
     * @param node the node
     * @param arg the acquire argument
     * @return {@code true} if interrupted while waiting
     */
    final boolean acquireQueued(final Node node, int arg) {
        boolean failed = true;
        try {
            boolean interrupted = false;
            for (;;) {
                // 拿到当前节点的前驱节点p
                final Node p = node.predecessor();
                /*
                 * 检测当前节点的前驱节点是否为head，这是试获取锁的资格。
                 * 如果是的话，说明他是队列中第一个“有效的”节点，因此尝试获取，则调用子类重写的tryAcquire方法尝试获取锁，成功则将head置为当前节点
                 */
                if (p == head && tryAcquire(arg)) {
                    // 获取到锁之后，就将自己设置为头结点（哨兵结点），线程出队列
                    setHead(node);
                    // 前驱结点（原哨兵结点）的链接置空，由JVM回收
                    p.next = null; // help GC
                    // 获取锁是否失败改成false，表示成功获取到了锁
                    failed = false;
                    // 返回interrupted，即返回线程是否被中断
                    return interrupted;
                }
                // 前驱结点不是头结点或者获取同步状态失败
                /*
                 * 如果未成功获取锁，则根据前驱节点判断是否要阻塞当前线程。如果需要，借助JUC包下的LockSupport类的静态方法Park挂起当前线程，直到被唤醒。
                 * 如果阻塞过程中被中断，则置interrupted标志位为true。
                 * shouldParkAfterFailedAcquire方法在前驱状态不为SIGNAL的情况下都会循环重试获取锁
                 */
                if (shouldParkAfterFailedAcquire(p, node) &&
                    parkAndCheckInterrupt())
                    /*
                     * 到这一步，说明是当前结点（线程）因为被中断而唤醒，那就改变自己的中断标志位状态信息为true
                     * 然后又从新开始循环，直到获取到锁，才能返回
                     */
                    interrupted = true;
            }
        } finally { // 线程获取到锁或者发生异常之后都会执行的finally语句块
            // 如果有异常
            /*
             * 如果failed为true，表示获取锁失败，即对应发生异常的情况，
             * 这里发生异常的情况只有在tryAcquire方法和predecessor方法中可能会抛出异常，此时还没有获得锁，failed=true
             * 那么执行cancelAcquire方法，该方法用于取消该线程获取锁的请求，将该结点的线程状态改为CANCELLED，并尝试移除结点（如果是尾结点）
             * 另外，在超时等待获取锁的的方法中，如果超过时间没有获取到锁，也会调用该方法
             *
             * 如果failed为false，表示获取到了锁，那么该方法直接结束，继续往下执行；
             */
            if (failed)
                // 取消请求，对应到队列操作，就是将当前节点从队列中移除。
                cancelAcquire(node);
        }
    }

    /**
     * doAcquireInterruptibly会首先判断线程是否是中断状态，如果是则直接返回并抛出异常其他不步骤和独占式不可中断获取锁基本原理一致。
     * 还有一点的区别就是在后续挂起的线程因为线程被中断而返回时的处理方式不一样：
     *
     *  独占式不可中断获取锁仅仅是记录该状态，interrupted = true，紧接着又继续循环获取锁；
     *  独占式可中断获取锁则直接抛出异常，因此会直接跳出循环去执行finally代码块。
     *
     * Acquires in exclusive interruptible mode.
     * @param arg the acquire argument
     */
    private void doAcquireInterruptibly(int arg)
        throws InterruptedException {
        // 同样调用addWaiter将当前线程构造成结点加入到同步队列尾部
        final Node node = addWaiter(Node.EXCLUSIVE);
        // 获取锁失败标志，默认为true
        boolean failed = true;
        try {
            // 和独占式不可中断方法acquireQueued一样，循环获取锁
            for (;;) {
                final Node p = node.predecessor();
                if (p == head && tryAcquire(arg)) {
                    setHead(node);
                    p.next = null; // help GC
                    failed = false;
                    return;
                }
                if (shouldParkAfterFailedAcquire(p, node) &&
                    parkAndCheckInterrupt())
                    /*
                     * 这里就是区别所在，独占不可中断式方法acquireQueued中
                     * 如果线程被中断，此处仅仅会记录该状态，interrupted = true，紧接着又继续循环获取锁
                     *
                     * 但是在该独占可中断式的锁获取方法中
                     * 如果线程被中断，此处直接抛出异常，因此会直接跳出循环去执行finally代码块
                     */
                    throw new InterruptedException();
            }
        } finally { // 获取到锁或者抛出异常都会执行finally代码块
            // 如果获取锁失败。可能就是线程被中断了，那么执行cancelAcquire方法取消该结点对锁的请求，该线程结束
            if (failed)
                cancelAcquire(node);
        }
    }

    /**
     * Acquires in exclusive timed mode.
     *
     * @param arg the acquire argument
     * @param nanosTimeout max wait time
     * @return {@code true} if acquired
     */
    private boolean doAcquireNanos(int arg, long nanosTimeout)
            throws InterruptedException {
        if (nanosTimeout <= 0L)
            return false;
        final long deadline = System.nanoTime() + nanosTimeout;
        final Node node = addWaiter(Node.EXCLUSIVE);
        boolean failed = true;
        try {
            for (;;) {
                final Node p = node.predecessor();
                if (p == head && tryAcquire(arg)) {
                    setHead(node);
                    p.next = null; // help GC
                    failed = false;
                    return true;
                }
                nanosTimeout = deadline - System.nanoTime();
                if (nanosTimeout <= 0L)
                    return false;
                if (shouldParkAfterFailedAcquire(p, node) &&
                    nanosTimeout > spinForTimeoutThreshold)
                    LockSupport.parkNanos(this, nanosTimeout);
                if (Thread.interrupted())
                    throw new InterruptedException();
            }
        } finally {
            if (failed)
                cancelAcquire(node);
        }
    }

    /**
     * 类似于独占式获取锁acquire方法中的addWaiter和acquireQueued方法的组合版本！
     *
     *  大概步骤如下：
     *      1. 调用addWaiter方法，将当前线程封装为Node.SHARED模式的Node结点后加入到AQS 同步队列的尾部，即表示共享模式。
     *      2. 后面就是类似于acquireQueued方法的逻辑，结点自旋尝试获取共享锁。如果还是获取不到，那么最终使用park方法挂起自己等待被唤醒。
     *
     * 每个结点可以尝试获取锁的要求是前驱结点是头结点，那么它本身就是整个队列中的第二个结点，每个获得锁的结点都一定是成为过头结点。那么如果某第二个结点因为不满足条件没有获取到共享锁而被挂起，那么即使后续结点满足条件也一定不能获取到共享锁。
     *
     * Acquires in shared uninterruptible mode.
     * @param arg the acquire argument
     */
    private void doAcquireShared(int arg) {
        // 1 addWaiter方法逻辑，和独占式获取的区别1 ：以共享模式Node.SHARED添加结点
        final Node node = addWaiter(Node.SHARED);
        /*
         * 2 下面就是类似于acquireQueued方法的逻辑
         * 区别在于获取到锁之后acquireQueued调用setHead方法，这里调用setHeadAndPropagate方法
         */
        // 当前线程获取锁失败的标志
        boolean failed = true;
        try {
            // 当前线程的中断标志
            boolean interrupted = false;
            for (;;) {
                // 获取前驱结点
                final Node p = node.predecessor();
                // 当前驱结点是头结点的时候就会以共享的方式去尝试获取锁
                if (p == head) {
                    int r = tryAcquireShared(arg);
                    // 返回值如果大于等于0,则表示获取到了锁
                    // 一旦共享获取成功，设置新的头结点，并且唤醒后继线程
                    if (r >= 0) {
                        // 和独占式获取的区别2 ：修改当前的头结点，根据传播状态判断是否要唤醒后继结点。
                        setHeadAndPropagate(node, r);
                        // 释放掉已经获取到锁的前驱结点
                        p.next = null; // help GC
                        // 检查设置中断标志
                        if (interrupted)
                            selfInterrupt();
                        failed = false;
                        return;
                    }
                }
                // 判断是否应该挂起，以及挂起的方法，和acquireQueued方法的逻辑完全一致，不会响应中断
                if (shouldParkAfterFailedAcquire(p, node) &&
                    parkAndCheckInterrupt())
                    interrupted = true;
            }
        } finally {
            if (failed)
                cancelAcquire(node);
        }
    }

    /**
     * Acquires in shared interruptible mode.
     * @param arg the acquire argument
     */
    private void doAcquireSharedInterruptibly(int arg)
        throws InterruptedException {
        final Node node = addWaiter(Node.SHARED);
        boolean failed = true;
        try {
            for (;;) {
                final Node p = node.predecessor();
                if (p == head) {
                    int r = tryAcquireShared(arg);
                    if (r >= 0) {
                        setHeadAndPropagate(node, r);
                        p.next = null; // help GC
                        failed = false;
                        return;
                    }
                }
                if (shouldParkAfterFailedAcquire(p, node) &&
                    parkAndCheckInterrupt())
                    throw new InterruptedException();
            }
        } finally {
            if (failed)
                cancelAcquire(node);
        }
    }

    /**
     * 在支持响应中断的基础上， 增加了超时获取的特性。
     *
     *    该方法在自旋过程中，当结点的前驱结点为头结点时尝试获取锁，如果获取成功则从该方法返回，这个过程和共享式式同步获取的过程类似，但是在锁获取失败的处理上有所不同。
     *
     *    如果当前线程获取锁失败，则判断是否超时（nanosTimeout小于等于0表示已经超时），如果没有超时，重新计算超时间隔nanosTimeout，然后使当前线程等待nanosTimeout纳秒（当已到设置的超时时间，该线程会从LockSupport.parkNanos(Objectblocker,long nanos)方法返回）。
     *
     *    如果nanosTimeout小于等于spinForTimeoutThreshold（1000纳秒）时，将不会使该线程进行超时等待，而是进入快速的自旋过程。原因在于，非常短的超时等待无法做到十分精确，如果这时再进行超时等待，相反会让nanosTimeout的超时从整体上表现得反而不精确。
     *
     *    因此，在超时非常短的场景下，AQS会进入无条件的快速自旋而不是挂起线程。
     *
     * Acquires in shared timed mode.
     *
     * @param arg the acquire argument
     * @param nanosTimeout max wait time
     * @return {@code true} if acquired
     */
    private boolean doAcquireSharedNanos(int arg, long nanosTimeout)
            throws InterruptedException {
        // 剩余超时时间小于等于0的，直接返回
        if (nanosTimeout <= 0L)
            return false;
        // 能够等待获取的最后纳秒时间
        final long deadline = System.nanoTime() + nanosTimeout;
        // 同样调用addWaiter将当前线程构造成结点加入到同步队列尾部
        final Node node = addWaiter(Node.SHARED);
        boolean failed = true;
        try {
            // 和共享式式不可中断方法doAcquireShared一样，自旋获取锁
            for (;;) {
                final Node p = node.predecessor();
                if (p == head) {
                    int r = tryAcquireShared(arg);
                    if (r >= 0) {
                        setHeadAndPropagate(node, r);
                        p.next = null; // help GC
                        failed = false;
                        return true;
                    }
                }
                // 这里就是区别所在
                // 如果新的剩余超时时间小于0，则退出循环，返回false，表示没获取到锁
                nanosTimeout = deadline - System.nanoTime();
                if (nanosTimeout <= 0L)
                    return false;
                // 如果需要挂起 并且 剩余nanosTimeout大于spinForTimeoutThreshold，即大于1000纳秒
                if (shouldParkAfterFailedAcquire(p, node) &&
                    nanosTimeout > spinForTimeoutThreshold)
                    // 那么调用LockSupport.parkNanos方法将当前线程挂起nanosTimeout
                    LockSupport.parkNanos(this, nanosTimeout);
                // 如果线程被中断了，那么直接抛出异常
                if (Thread.interrupted())
                    throw new InterruptedException();
            }
        } finally { // 获取到锁、超时时间到了、抛出异常都会执行finally代码块
            /*
             * 如果获取锁失败。可能就是线程被中断了，那么执行cancelAcquire方法取消该结点对锁的请求，该线程结束
             * 或者是超时时间到了，那么执行cancelAcquire方法取消该结点对锁的请求，将返回false
             */
            if (failed)
                cancelAcquire(node);
        }
    }

    // Main exported methods
    // AQS可重写的方法如下所示，至少有5个：
    // 下面的acquire和release系列方法并不是一定代表着锁的获取和释放，它的具体含义要看同步组件的具体实现，因为AQS不仅仅可被用来实现锁，还可以被用来实现其他的同步组件，因此这得获取和释放的应该被叫做资源。

    /**
     * 独占式获取锁，该方法需要查询当前状态并判断锁是否符合预期，然后再进行CAS设置锁。返回true则成功，否则失败。
     *
     * Attempts to acquire in exclusive mode. This method should query
     * if the state of the object permits it to be acquired in the
     * exclusive mode, and if so to acquire it.
     *
     * <p>This method is always invoked by the thread performing
     * acquire.  If this method reports failure, the acquire method
     * may queue the thread, if it is not already queued, until it is
     * signalled by a release from some other thread. This can be used
     * to implement method {@link Lock#tryLock()}.
     *
     * <p>The default
     * implementation throws {@link UnsupportedOperationException}.
     *
     * @param arg the acquire argument. This value is always the one
     *        passed to an acquire method, or is the value saved on entry
     *        to a condition wait.  The value is otherwise uninterpreted
     *        and can represent anything you like.   参数，在实现的时候可以传递自己想要的数据
     * @return {@code true} if successful. Upon success, this object has
     *         been acquired.   返回true则成功，否则失败。
     * @throws IllegalMonitorStateException if acquiring would place this
     *         synchronizer in an illegal state. This exception must be
     *         thrown in a consistent fashion for synchronization to work
     *         correctly.
     * @throws UnsupportedOperationException if exclusive mode is not supported
     */
    protected boolean tryAcquire(int arg) {
        throw new UnsupportedOperationException();
    }

    /**
     * 独占式释放锁，等待获取锁的线程将有机会获取锁。返回true则成功，否则失败。
     *
     * Attempts to set the state to reflect a release in exclusive
     * mode.
     *
     * <p>This method is always invoked by the thread performing release.
     *
     * <p>The default implementation throws
     * {@link UnsupportedOperationException}.
     *
     * @param arg the release argument. This value is always the one
     *        passed to a release method, or the current state value upon
     *        entry to a condition wait.  The value is otherwise
     *        uninterpreted and can represent anything you like.   参数，在实现的时候可以传递自己想要的数据
     * @return {@code true} if this object is now in a fully released
     *         state, so that any waiting threads may attempt to acquire;
     *         and {@code false} otherwise.     返回true则成功，否则失败。
     * @throws IllegalMonitorStateException if releasing would place this
     *         synchronizer in an illegal state. This exception must be
     *         thrown in a consistent fashion for synchronization to work
     *         correctly.
     * @throws UnsupportedOperationException if exclusive mode is not supported
     */
    protected boolean tryRelease(int arg) {
        throw new UnsupportedOperationException();
    }

    /**
     * 共享式获取锁，返回大于等于0的值表示获取成功，否则失败。
     *
     * @param arg 参数，在实现的时候可以传递自己想要的数据
     * @return 返回大于等于0的值表示获取成功，否则失败。
     * 如果返回值小于0，表示当前线程共享锁失败
     * 如果返回值大于0，表示当前线程共享锁成功，并且接下来其他线程尝试获取共享锁的行为很可能成功
     * 如果返回值等于0，表示当前线程共享锁成功，但是接下来其他线程尝试获取共享锁的行为会失败（实际上也有可能成功）
     *
     * Attempts to acquire in shared mode. This method should query if
     * the state of the object permits it to be acquired in the shared
     * mode, and if so to acquire it.
     *
     * <p>This method is always invoked by the thread performing
     * acquire.  If this method reports failure, the acquire method
     * may queue the thread, if it is not already queued, until it is
     * signalled by a release from some other thread.
     *
     * <p>The default implementation throws {@link
     * UnsupportedOperationException}.
     *
     * @param arg the acquire argument. This value is always the one
     *        passed to an acquire method, or is the value saved on entry
     *        to a condition wait.  The value is otherwise uninterpreted
     *        and can represent anything you like.
     * @return a negative value on failure; zero if acquisition in shared
     *         mode succeeded but no subsequent shared-mode acquire can
     *         succeed; and a positive value if acquisition in shared
     *         mode succeeded and subsequent shared-mode acquires might
     *         also succeed, in which case a subsequent waiting thread
     *         must check availability. (Support for three different
     *         return values enables this method to be used in contexts
     *         where acquires only sometimes act exclusively.)  Upon
     *         success, this object has been acquired.
     * @throws IllegalMonitorStateException if acquiring would place this
     *         synchronizer in an illegal state. This exception must be
     *         thrown in a consistent fashion for synchronization to work
     *         correctly.
     * @throws UnsupportedOperationException if shared mode is not supported
     */
    protected int tryAcquireShared(int arg) {
        throw new UnsupportedOperationException();
    }

    /**
     * 共享式释放锁。返回true成功，否则失败。
     *
     * Attempts to set the state to reflect a release in shared mode.
     *
     * <p>This method is always invoked by the thread performing release.
     *
     * <p>The default implementation throws
     * {@link UnsupportedOperationException}.
     *
     * @param arg the release argument. This value is always the one
     *        passed to a release method, or the current state value upon
     *        entry to a condition wait.  The value is otherwise
     *        uninterpreted and can represent anything you like.    参数，在实现的时候可以传递自己想要的数据
     * @return {@code true} if this release of shared mode may permit a
     *         waiting acquire (shared or exclusive) to succeed; and
     *         {@code false} otherwise      返回true成功，否则失败。
     * @throws IllegalMonitorStateException if releasing would place this
     *         synchronizer in an illegal state. This exception must be
     *         thrown in a consistent fashion for synchronization to work
     *         correctly.
     * @throws UnsupportedOperationException if shared mode is not supported
     */
    protected boolean tryReleaseShared(int arg) {
        throw new UnsupportedOperationException();
    }

    /**
     * 当前AQS是否在独占模式下被线程占用，一般表示是否被前当线程独占；如果同步是以独占方式进行的，则返回true；其他情况则返回 false
     *
     * Returns {@code true} if synchronization is held exclusively with
     * respect to the current (calling) thread.  This method is invoked
     * upon each call to a non-waiting {@link ConditionObject} method.
     * (Waiting methods instead invoke {@link #release}.)
     *
     * <p>The default implementation throws {@link
     * UnsupportedOperationException}. This method is invoked
     * internally only within {@link ConditionObject} methods, so need
     * not be defined if conditions are not used.
     *
     * @return {@code true} if synchronization is held exclusively;
     *         {@code false} otherwise      如果同步是以独占方式进行的，则返回true；其他情况则返回 false
     * @throws UnsupportedOperationException if conditions are not supported
     */
    protected boolean isHeldExclusively() {
        throw new UnsupportedOperationException();
    }

    /**
     * 首先尝试获取一次锁，如果成功，则返回；
     * 否则会把当前线程包装成Node插入到队列中，在队列中会检测是否为head的直接后继，并尝试获取锁,
     * 如果获取失败，则阻塞当前线程，直至被 "释放锁的线程" 唤醒或者被中断，随后再次尝试获取锁，如此反复
     *
     * Acquires in exclusive mode, ignoring interrupts.  Implemented
     * by invoking at least once {@link #tryAcquire},
     * returning on success.  Otherwise the thread is queued, possibly
     * repeatedly blocking and unblocking, invoking {@link
     * #tryAcquire} until success.  This method can be used
     * to implement method {@link Lock#lock}.
     *
     * @param arg the acquire argument.  This value is conveyed to
     *        {@link #tryAcquire} but is otherwise uninterpreted and
     *        can represent anything you like.
     */
    public final void acquire(int arg) {
        // 在acquire中，首先调用tryAcquire，目的尝试获取锁，如果获取不到，就调用addWaiter创建一个waiter（当前线程）放置到队列中，然后自身阻塞，那我们来看看如何尝试获取锁
        // 这个tryAcquire需要由子类实现...
        // 如果获取锁成功，tryAcquire方法返回true，则不操作；
        // 如果获取锁失败，则调用addWaiter并采取Node.EXCLUSIVE模式把当前线程放到队列中去，
        // mode是一个表示Node类型的字段，仅仅表示这个节点是独占的还是共享的，这里是独占的，
        // 在完成了线程节点的插入之后，还需要做一件事：将当前线程挂起！这里在acquireQueued内通过parkAndCheckInterrupt将线程挂起
        if (!tryAcquire(arg) &&
            acquireQueued(addWaiter(Node.EXCLUSIVE), arg))
            // selfInterrupt是acquire中最后可能调用的一个方法，顾名思义，用于自我中断，什么意思呢，就是根据!tryAcquire和acquireQueued返回值判断是否需要设置中断标志位。
            // 只有tryAcquire尝试失败，并且acquireQueued方法true时，才表示该线程是被中断过了的，但是在parkAndCheckInterrupt里面判断中断标志位之后又重置的中断标志位（interrupted方法会重置中断标志位）。
            // 虽然看起来没啥用，但是本着负责的态度，还是将中断标志位记录下来。那么此时重新设置该线程的中断标志位为true。
            selfInterrupt();
    }

    /**
     * 在JDK1.5之前，当一个线程获取不到锁而被阻塞在synchronized之外时，如果对该线程进行中断操作，此时该线程的中断标志位会被修改，但线程依旧会阻塞在synchronized上，等待着获取锁，即无法响应中断。
     *
     *  上面分析的独占式获取锁的方法acquire，同样是不会响应中断的。但是AQS提供了另外一个acquireInterruptibly模版方法，调用该方法的线程在等待获取锁时，如果当前线程被中断，会立刻返回，并抛出InterruptedException。
     *
     * Acquires in exclusive mode, aborting if interrupted.
     * Implemented by first checking interrupt status, then invoking
     * at least once {@link #tryAcquire}, returning on
     * success.  Otherwise the thread is queued, possibly repeatedly
     * blocking and unblocking, invoking {@link #tryAcquire}
     * until success or the thread is interrupted.  This method can be
     * used to implement method {@link Lock#lockInterruptibly}.
     *
     * @param arg the acquire argument.  This value is conveyed to
     *        {@link #tryAcquire} but is otherwise uninterpreted and
     *        can represent anything you like.
     * @throws InterruptedException if the current thread is interrupted
     */
    public final void acquireInterruptibly(int arg)
            throws InterruptedException {
        // 如果当前线程被中断，直接抛出异常
        if (Thread.interrupted())
            throw new InterruptedException();
        // 尝试获取锁
        if (!tryAcquire(arg))
            // 如果没获取到锁，那么调用AQS 可被中断的方法
            doAcquireInterruptibly(arg);
    }

    /**
     * Attempts to acquire in exclusive mode, aborting if interrupted,
     * and failing if the given timeout elapses.  Implemented by first
     * checking interrupt status, then invoking at least once {@link
     * #tryAcquire}, returning on success.  Otherwise, the thread is
     * queued, possibly repeatedly blocking and unblocking, invoking
     * {@link #tryAcquire} until success or the thread is interrupted
     * or the timeout elapses.  This method can be used to implement
     * method {@link Lock#tryLock(long, TimeUnit)}.
     *
     * @param arg the acquire argument.  This value is conveyed to
     *        {@link #tryAcquire} but is otherwise uninterpreted and
     *        can represent anything you like.
     * @param nanosTimeout the maximum number of nanoseconds to wait
     * @return {@code true} if acquired; {@code false} if timed out
     * @throws InterruptedException if the current thread is interrupted
     */
    public final boolean tryAcquireNanos(int arg, long nanosTimeout)
            throws InterruptedException {
        if (Thread.interrupted())
            throw new InterruptedException();
        return tryAcquire(arg) ||
            doAcquireNanos(arg, nanosTimeout);
    }

    /**
     *
     * release独占式锁释放
     *  当前线程获取到锁并执行了相应逻辑之后，就需要释放锁，使得后续结点能够继续获取锁。通过调用AQS的release(int arg)模版方法可以独占式的释放锁，在该方法大概步骤如下：
     *
     *  尝试使用tryRelease(arg)释放锁，该方法在最开始我们就讲过，是自己实现的方法，通常来说就是将state值为0或者减少、清除当前获得锁的线程等等，如果符合自己的逻辑，锁释放成功则返回true，否则返回false；
     *  如果tryRelease释放成功返回true，判断如果head不为null且head的状态不为0，那么尝试调用unparkSuccessor方法唤醒头结点之后的一个非取消状态(非CANCELLED状态)的后继结点，让其可以进行锁获取。返回true，方法结束；
     *  如果tryRelease释放失败，那么返回false，方法结束。
     *
     * Releases in exclusive mode.  Implemented by unblocking one or
     * more threads if {@link #tryRelease} returns true.
     * This method can be used to implement method {@link Lock#unlock}.
     *
     * @param arg the release argument.  This value is conveyed to
     *        {@link #tryRelease} but is otherwise uninterpreted and
     *        can represent anything you like.
     * @return the value returned from {@link #tryRelease}
     */
    public final boolean release(int arg) {
        // 先调用tryRelease方法，需要子类重写...
        if (tryRelease(arg)) {
            // 此时已经释放了锁，然后便通知队列头部的线程去获取锁...
            /*
             * 此时的head节点可能有3种情况:
             * 1. null (AQS的head延迟初始化+无竞争的情况)
             * 2. 当前线程在获取锁时new出来的节点通过setHead设置的
             * 3. 由于通过tryRelease已经完全释放掉了独占锁，有新的节点在acquireQueued中获取到了独占锁，并设置了head
             *
             * 第三种情况可以再分为两种情况：
             *     情况一：
             *     		时刻1：线程A通过acquireQueued，持锁成功，set了head
             *          时刻2：线程B通过tryAcquire试图获取独占锁失败失败，进入acquiredQueued
             *          时刻3：线程A通过tryRelease释放了独占锁
             *          时刻4：线程B通过acquireQueued中的tryAcquire获取到了独占锁并调用setHead
             *          时刻5：线程A读到了此时的head实际上是线程B对应的node
             *     情况二：
             *     		时刻1：线程A通过tryAcquire直接持锁成功，head为null
             *          时刻2：线程B通过tryAcquire试图获取独占锁失败失败，入队过程中初始化了head，进入acquiredQueued
             *          时刻3：线程A通过tryRelease释放了独占锁，此时线程B还未开始tryAcquire
             *          时刻4：线程A读到了此时的head实际上是线程B初始化出来的傀儡head
             */
            Node h = head;
            // head节点状态不会是CANCELLED，所以这里h.waitStatus != 0相当于h.waitStatus < 0
            if (h != null && h.waitStatus != 0)
                // 唤醒头结点之后的一个处于等待锁状态的后继结点
                unparkSuccessor(h);
            return true;
        }
        return false;
    }

    /**
     * 获取共享锁的实现，不响应中断
     *  共享锁允许多个线程持有，如果要使用AQS中的共享锁，在实现 tryAcquireShared方法 时需要注意，返回负数表示获取失败，返回0表示成功，但是后继争用线程不会成功，返回正数表示获取成功，并且后继争用线程也可能成功。
     *
     * Acquires in shared mode, ignoring interrupts.  Implemented by
     * first invoking at least once {@link #tryAcquireShared},
     * returning on success.  Otherwise the thread is queued, possibly
     * repeatedly blocking and unblocking, invoking {@link
     * #tryAcquireShared} until success.
     *
     * @param arg the acquire argument.  This value is conveyed to
     *        {@link #tryAcquireShared} but is otherwise uninterpreted
     *        and can represent anything you like.
     */
    public final void acquireShared(int arg) {
        // 尝试调用tryAcquireShared方法获取锁
        // 获取成功（返回值大于等于0）则直接返回；
        if (tryAcquireShared(arg) < 0)
            // 失败则调用doAcquireShared方法将当前线程封装为Node.SHARED类型的Node 结点后加入到AQS 同步队列的尾部，
            // 然后"自旋"尝试获取同步状态，如果还是获取不到，那么最终使用park方法挂起自己。
            doAcquireShared(arg);
    }

    /**
     * Acquires in shared mode, aborting if interrupted.  Implemented
     * by first checking interrupt status, then invoking at least once
     * {@link #tryAcquireShared}, returning on success.  Otherwise the
     * thread is queued, possibly repeatedly blocking and unblocking,
     * invoking {@link #tryAcquireShared} until success or the thread
     * is interrupted.
     * @param arg the acquire argument.
     * This value is conveyed to {@link #tryAcquireShared} but is
     * otherwise uninterpreted and can represent anything
     * you like.
     * @throws InterruptedException if the current thread is interrupted
     */
    public final void acquireSharedInterruptibly(int arg)
            throws InterruptedException {
        if (Thread.interrupted())
            throw new InterruptedException();
        if (tryAcquireShared(arg) < 0)
            doAcquireSharedInterruptibly(arg);
    }

    /**
     *  共享式超时获取锁tryAcquireSharedNanos模版方法可以被视作共享式响应中断获取锁acquireSharedInterruptibly方法的“增强版”，支持中断，支持超时时间！
     *
     * Attempts to acquire in shared mode, aborting if interrupted, and
     * failing if the given timeout elapses.  Implemented by first
     * checking interrupt status, then invoking at least once {@link
     * #tryAcquireShared}, returning on success.  Otherwise, the
     * thread is queued, possibly repeatedly blocking and unblocking,
     * invoking {@link #tryAcquireShared} until success or the thread
     * is interrupted or the timeout elapses.
     *
     * @param arg the acquire argument.  This value is conveyed to
     *        {@link #tryAcquireShared} but is otherwise uninterpreted
     *        and can represent anything you like.
     * @param nanosTimeout the maximum number of nanoseconds to wait
     * @return {@code true} if acquired; {@code false} if timed out
     * @throws InterruptedException if the current thread is interrupted
     */
    public final boolean tryAcquireSharedNanos(int arg, long nanosTimeout)
            throws InterruptedException {
        // 最开始就检查一次，如果当前线程是被中断状态，则清除已中断状态，并抛出异常
        if (Thread.interrupted())
            throw new InterruptedException();
        // 下面是一个||运算进行短路连接的代码
        // tryAcquireShared尝试获取锁，获取到了直接返回true
        // 获取不到（左边表达式为false） 就执行doAcquireSharedNanos方法
        return tryAcquireShared(arg) >= 0 ||
            doAcquireSharedNanos(arg, nanosTimeout);
    }

    /**
     * Releases in shared mode.  Implemented by unblocking one or more
     * threads if {@link #tryReleaseShared} returns true.
     *
     * @param arg the release argument.  This value is conveyed to
     *        {@link #tryReleaseShared} but is otherwise uninterpreted
     *        and can represent anything you like.
     * @return the value returned from {@link #tryReleaseShared}
     */
    public final boolean releaseShared(int arg) {
        if (tryReleaseShared(arg)) {
            doReleaseShared();
            return true;
        }
        return false;
    }

    // Queue inspection methods

    /**
     * Queries whether any threads are waiting to acquire. Note that
     * because cancellations due to interrupts and timeouts may occur
     * at any time, a {@code true} return does not guarantee that any
     * other thread will ever acquire.
     *
     * <p>In this implementation, this operation returns in
     * constant time.
     *
     * @return {@code true} if there may be other threads waiting to acquire
     */
    public final boolean hasQueuedThreads() {
        return head != tail;
    }

    /**
     * Queries whether any threads have ever contended to acquire this
     * synchronizer; that is if an acquire method has ever blocked.
     *
     * <p>In this implementation, this operation returns in
     * constant time.
     *
     * @return {@code true} if there has ever been contention
     */
    public final boolean hasContended() {
        return head != null;
    }

    /**
     * Returns the first (longest-waiting) thread in the queue, or
     * {@code null} if no threads are currently queued.
     *
     * <p>In this implementation, this operation normally returns in
     * constant time, but may iterate upon contention if other threads are
     * concurrently modifying the queue.
     *
     * @return the first (longest-waiting) thread in the queue, or
     *         {@code null} if no threads are currently queued
     */
    public final Thread getFirstQueuedThread() {
        // handle only fast path, else relay
        return (head == tail) ? null : fullGetFirstQueuedThread();
    }

    /**
     * Version of getFirstQueuedThread called when fastpath fails
     */
    private Thread fullGetFirstQueuedThread() {
        /*
         * The first node is normally head.next. Try to get its
         * thread field, ensuring consistent reads: If thread
         * field is nulled out or s.prev is no longer head, then
         * some other thread(s) concurrently performed setHead in
         * between some of our reads. We try this twice before
         * resorting to traversal.
         */
        Node h, s;
        Thread st;
        if (((h = head) != null && (s = h.next) != null &&
             s.prev == head && (st = s.thread) != null) ||
            ((h = head) != null && (s = h.next) != null &&
             s.prev == head && (st = s.thread) != null))
            return st;

        /*
         * Head's next field might not have been set yet, or may have
         * been unset after setHead. So we must check to see if tail
         * is actually first node. If not, we continue on, safely
         * traversing from tail back to head to find first,
         * guaranteeing termination.
         */

        Node t = tail;
        Thread firstThread = null;
        while (t != null && t != head) {
            Thread tt = t.thread;
            if (tt != null)
                firstThread = tt;
            t = t.prev;
        }
        return firstThread;
    }

    /**
     * Returns true if the given thread is currently queued.
     *
     * <p>This implementation traverses the queue to determine
     * presence of the given thread.
     *
     * @param thread the thread
     * @return {@code true} if the given thread is on the queue
     * @throws NullPointerException if the thread is null
     */
    public final boolean isQueued(Thread thread) {
        if (thread == null)
            throw new NullPointerException();
        for (Node p = tail; p != null; p = p.prev)
            if (p.thread == thread)
                return true;
        return false;
    }

    /**
     * Returns {@code true} if the apparent first queued thread, if one
     * exists, is waiting in exclusive mode.  If this method returns
     * {@code true}, and the current thread is attempting to acquire in
     * shared mode (that is, this method is invoked from {@link
     * #tryAcquireShared}) then it is guaranteed that the current thread
     * is not the first queued thread.  Used only as a heuristic in
     * ReentrantReadWriteLock.
     */
    final boolean apparentlyFirstQueuedIsExclusive() {
        Node h, s;
        return (h = head) != null &&
            (s = h.next)  != null &&
            !s.isShared()         &&
            s.thread != null;
    }

    /**
     * 主要是判断当前线程是否位于同步队列中的第一个。如果是则返回true，否则返回false。
     *
     * Queries whether any threads have been waiting to acquire longer
     * than the current thread.
     *
     * <p>An invocation of this method is equivalent to (but may be
     * more efficient than):
     *  <pre> {@code
     * getFirstQueuedThread() != Thread.currentThread() &&
     * hasQueuedThreads()}</pre>
     *
     * <p>Note that because cancellations due to interrupts and
     * timeouts may occur at any time, a {@code true} return does not
     * guarantee that some other thread will acquire before the current
     * thread.  Likewise, it is possible for another thread to win a
     * race to enqueue after this method has returned {@code false},
     * due to the queue being empty.
     *
     * <p>This method is designed to be used by a fair synchronizer to
     * avoid <a href="AbstractQueuedSynchronizer#barging">barging</a>.
     * Such a synchronizer's {@link #tryAcquire} method should return
     * {@code false}, and its {@link #tryAcquireShared} method should
     * return a negative value, if this method returns {@code true}
     * (unless this is a reentrant acquire).  For example, the {@code
     * tryAcquire} method for a fair, reentrant, exclusive mode
     * synchronizer might look like this:
     *
     *  <pre> {@code
     * protected boolean tryAcquire(int arg) {
     *   if (isHeldExclusively()) {
     *     // A reentrant acquire; increment hold count
     *     return true;
     *   } else if (hasQueuedPredecessors()) {
     *     return false;
     *   } else {
     *     // try to acquire normally
     *   }
     * }}</pre>
     *
     * @return {@code true} if there is a queued thread preceding the
     *         current thread, and {@code false} if the current thread
     *         is at the head of the queue or the queue is empty
     * @since 1.7
     */
    public final boolean hasQueuedPredecessors() {
        // The correctness of this depends on head being initialized
        // before tail and on head.next being accurate if the current
        // thread is first in queue.
        Node t = tail; // Read fields in reverse initialization order
        Node h = head;
        Node s;
        return h != t &&
            ((s = h.next) == null || s.thread != Thread.currentThread());
    }


    // Instrumentation and monitoring methods

    /**
     * Returns an estimate of the number of threads waiting to
     * acquire.  The value is only an estimate because the number of
     * threads may change dynamically while this method traverses
     * internal data structures.  This method is designed for use in
     * monitoring system state, not for synchronization
     * control.
     *
     * @return the estimated number of threads waiting to acquire
     */
    public final int getQueueLength() {
        int n = 0;
        for (Node p = tail; p != null; p = p.prev) {
            if (p.thread != null)
                ++n;
        }
        return n;
    }

    /**
     * Returns a collection containing threads that may be waiting to
     * acquire.  Because the actual set of threads may change
     * dynamically while constructing this result, the returned
     * collection is only a best-effort estimate.  The elements of the
     * returned collection are in no particular order.  This method is
     * designed to facilitate construction of subclasses that provide
     * more extensive monitoring facilities.
     *
     * @return the collection of threads
     */
    public final Collection<Thread> getQueuedThreads() {
        ArrayList<Thread> list = new ArrayList<Thread>();
        for (Node p = tail; p != null; p = p.prev) {
            Thread t = p.thread;
            if (t != null)
                list.add(t);
        }
        return list;
    }

    /**
     * Returns a collection containing threads that may be waiting to
     * acquire in exclusive mode. This has the same properties
     * as {@link #getQueuedThreads} except that it only returns
     * those threads waiting due to an exclusive acquire.
     *
     * @return the collection of threads
     */
    public final Collection<Thread> getExclusiveQueuedThreads() {
        ArrayList<Thread> list = new ArrayList<Thread>();
        for (Node p = tail; p != null; p = p.prev) {
            if (!p.isShared()) {
                Thread t = p.thread;
                if (t != null)
                    list.add(t);
            }
        }
        return list;
    }

    /**
     * Returns a collection containing threads that may be waiting to
     * acquire in shared mode. This has the same properties
     * as {@link #getQueuedThreads} except that it only returns
     * those threads waiting due to a shared acquire.
     *
     * @return the collection of threads
     */
    public final Collection<Thread> getSharedQueuedThreads() {
        ArrayList<Thread> list = new ArrayList<Thread>();
        for (Node p = tail; p != null; p = p.prev) {
            if (p.isShared()) {
                Thread t = p.thread;
                if (t != null)
                    list.add(t);
            }
        }
        return list;
    }

    /**
     * Returns a string identifying this synchronizer, as well as its state.
     * The state, in brackets, includes the String {@code "State ="}
     * followed by the current value of {@link #getState}, and either
     * {@code "nonempty"} or {@code "empty"} depending on whether the
     * queue is empty.
     *
     * @return a string identifying this synchronizer, as well as its state
     */
    public String toString() {
        int s = getState();
        String q  = hasQueuedThreads() ? "non" : "";
        return super.toString() +
            "[State = " + s + ", " + q + "empty queue]";
    }


    // Internal support methods for Conditions

    /**
     * 查找当前节点是否在同步阻塞队列中，方法先是快速判断，判断不了再进行遍历查找
     *
     * Returns true if a node, always one that was initially placed on
     * a condition queue, is now waiting to reacquire on sync queue.
     * @param node the node
     * @return true if is reacquiring
     */
    final boolean isOnSyncQueue(Node node) {
        //判断当前节点是否是CONDITION或者前置节点是否为空如果为空直接返回false
        if (node.waitStatus == Node.CONDITION || node.prev == null)
            return false;
        //如果下个节点存在，则在同步阻塞队列中返回true
        if (node.next != null) // If has successor, it must be on queue
            return true;
        /*
         * node.prev can be non-null, but not yet on queue because
         * the CAS to place it on queue can fail. So we have to
         * traverse from tail to make sure it actually made it.  It
         * will always be near the tail in calls to this method, and
         * unless the CAS failed (which is unlikely), it will be
         * there, so we hardly ever traverse much.
         */
        //遍历查找当前节点是否在同步阻塞队列中
        return findNodeFromTail(node);
    }

    /**
     * Returns true if node is on sync queue by searching backwards from tail.
     * Called only when needed by isOnSyncQueue.
     * @return true if present
     */
    private boolean findNodeFromTail(Node node) {
        Node t = tail;
        for (;;) {
            if (t == node)
                return true;
            if (t == null)
                return false;
            t = t.prev;
        }
    }

    /**
     * Transfers a node from a condition queue onto sync queue.
     * Returns true if successful.
     * @param node the node
     * @return true if successfully transferred (else the node was
     * cancelled before signal)
     */
    final boolean transferForSignal(Node node) {
        /*
         * If cannot change waitStatus, the node has been cancelled.
         */
        if (!compareAndSetWaitStatus(node, Node.CONDITION, 0))
            return false;

        /*
         * Splice onto queue and try to set waitStatus of predecessor to
         * indicate that thread is (probably) waiting. If cancelled or
         * attempt to set waitStatus fails, wake up to resync (in which
         * case the waitStatus can be transiently and harmlessly wrong).
         */
        Node p = enq(node);
        int ws = p.waitStatus;
        if (ws > 0 || !compareAndSetWaitStatus(p, ws, Node.SIGNAL))
            LockSupport.unpark(node.thread);
        return true;
    }

    /**
     * Transfers node, if necessary, to sync queue after a cancelled wait.
     * Returns true if thread was cancelled before being signalled.
     *
     * @param node the node
     * @return true if cancelled before the node was signalled
     */
    final boolean transferAfterCancelledWait(Node node) {
        // 用CAS方式将节点状态改成CONDITION，并加入到同步阻塞队列中返回true
        if (compareAndSetWaitStatus(node, Node.CONDITION, 0)) {
            enq(node);
            return true;
        }
        /*
         * If we lost out to a signal(), then we can't proceed
         * until it finishes its enq().  Cancelling during an
         * incomplete transfer is both rare and transient, so just
         * spin.
         */
        //如果不能加入到同步阻塞队列就自旋一直等待加入
        while (!isOnSyncQueue(node))
            Thread.yield();
        return false;
    }

    /**
     * 释放锁使同步阻塞队列的下个节点线程能获取锁
     *
     * Invokes release with current state value; returns saved state.
     * Cancels node and throws exception on failure.
     * @param node the condition node for this wait
     * @return previous sync state
     */
    final int fullyRelease(Node node) {
        boolean failed = true;
        try {
            //获取阻塞队列中当前线程节点的锁状态值
            int savedState = getState();
            //释放当前线程节点锁
            if (release(savedState)) {
                failed = false;
                return savedState;
            } else {
                throw new IllegalMonitorStateException();
            }
        } finally {
            //释放失败讲节点等待状态设置成关闭
            if (failed)
                node.waitStatus = Node.CANCELLED;
        }
    }

    // Instrumentation methods for conditions

    /**
     * Queries whether the given ConditionObject
     * uses this synchronizer as its lock.
     *
     * @param condition the condition
     * @return {@code true} if owned
     * @throws NullPointerException if the condition is null
     */
    public final boolean owns(ConditionObject condition) {
        return condition.isOwnedBy(this);
    }

    /**
     * Queries whether any threads are waiting on the given condition
     * associated with this synchronizer. Note that because timeouts
     * and interrupts may occur at any time, a {@code true} return
     * does not guarantee that a future {@code signal} will awaken
     * any threads.  This method is designed primarily for use in
     * monitoring of the system state.
     *
     * @param condition the condition
     * @return {@code true} if there are any waiting threads
     * @throws IllegalMonitorStateException if exclusive synchronization
     *         is not held
     * @throws IllegalArgumentException if the given condition is
     *         not associated with this synchronizer
     * @throws NullPointerException if the condition is null
     */
    public final boolean hasWaiters(ConditionObject condition) {
        if (!owns(condition))
            throw new IllegalArgumentException("Not owner");
        return condition.hasWaiters();
    }

    /**
     * Returns an estimate of the number of threads waiting on the
     * given condition associated with this synchronizer. Note that
     * because timeouts and interrupts may occur at any time, the
     * estimate serves only as an upper bound on the actual number of
     * waiters.  This method is designed for use in monitoring of the
     * system state, not for synchronization control.
     *
     * @param condition the condition
     * @return the estimated number of waiting threads
     * @throws IllegalMonitorStateException if exclusive synchronization
     *         is not held
     * @throws IllegalArgumentException if the given condition is
     *         not associated with this synchronizer
     * @throws NullPointerException if the condition is null
     */
    public final int getWaitQueueLength(ConditionObject condition) {
        if (!owns(condition))
            throw new IllegalArgumentException("Not owner");
        return condition.getWaitQueueLength();
    }

    /**
     * Returns a collection containing those threads that may be
     * waiting on the given condition associated with this
     * synchronizer.  Because the actual set of threads may change
     * dynamically while constructing this result, the returned
     * collection is only a best-effort estimate. The elements of the
     * returned collection are in no particular order.
     *
     * @param condition the condition
     * @return the collection of threads
     * @throws IllegalMonitorStateException if exclusive synchronization
     *         is not held
     * @throws IllegalArgumentException if the given condition is
     *         not associated with this synchronizer
     * @throws NullPointerException if the condition is null
     */
    public final Collection<Thread> getWaitingThreads(ConditionObject condition) {
        if (!owns(condition))
            throw new IllegalArgumentException("Not owner");
        return condition.getWaitingThreads();
    }

    /**
     * AQS内部类，ConditionObject实现了Condition接口
     *
     * Condition implementation for a {@link
     * AbstractQueuedSynchronizer} serving as the basis of a {@link
     * Lock} implementation.
     *
     * <p>Method documentation for this class describes mechanics,
     * not behavioral specifications from the point of view of Lock
     * and Condition users. Exported versions of this class will in
     * general need to be accompanied by documentation describing
     * condition semantics that rely on those of the associated
     * {@code AbstractQueuedSynchronizer}.
     *
     * <p>This class is Serializable, but all fields are transient,
     * so deserialized conditions have no waiters.
     */
    public class ConditionObject implements Condition, java.io.Serializable {
        private static final long serialVersionUID = 1173984872572414699L;
        /**
         * 条件队列第一个节点
         *
         * First node of condition queue.
         */
        private transient Node firstWaiter;
        /**
         * 条件队列最后一个节点
         *
         * Last node of condition queue.
         */
        private transient Node lastWaiter;

        /**
         * Creates a new {@code ConditionObject} instance.
         */
        public ConditionObject() { }

        // Internal methods

        /**
         * 创建以当前线程为基础的节点并把节点加入等待队列的尾部待其他线程处理。
         *
         * Adds a new waiter to wait queue.
         * @return its new wait node
         */
        private Node addConditionWaiter() {
            //获取等待队列尾部节点
            Node t = lastWaiter;
            // If lastWaiter is cancelled, clean out.
            //如果尾部状态不为CONDITION，如果已经被"激活"，清理之，然后重新获取尾部节点
            if (t != null && t.waitStatus != Node.CONDITION) {
                unlinkCancelledWaiters();
                t = lastWaiter;
            }
            //创建以当前线程为基础的节点，并将节点模式设置成CONDITION
            Node node = new Node(Thread.currentThread(), Node.CONDITION);
            //如果尾节点不存在，说明队列为空，将头节点设置成当前节点
            if (t == null)
                firstWaiter = node;
            else
                //如果尾节点存在，将此节点设置成尾节点的下个节点
                t.nextWaiter = node;
            //将尾节点设置成当前节点
            lastWaiter = node;
            return node;
        }

        /**
         * Removes and transfers nodes until hit non-cancelled one or
         * null. Split out from signal in part to encourage compilers
         * to inline the case of no waiters.
         * @param first (non-null) the first node on condition queue
         */
        private void doSignal(Node first) {
            do {
                if ( (firstWaiter = first.nextWaiter) == null)
                    lastWaiter = null;
                first.nextWaiter = null;
            } while (!transferForSignal(first) &&
                     (first = firstWaiter) != null);
        }

        /**
         * Removes and transfers all nodes.
         * @param first (non-null) the first node on condition queue
         */
        private void doSignalAll(Node first) {
            lastWaiter = firstWaiter = null;
            do {
                Node next = first.nextWaiter;
                first.nextWaiter = null;
                transferForSignal(first);
                first = next;
            } while (first != null);
        }

        /**
         * Unlinks cancelled waiter nodes from condition queue.
         * Called only while holding lock. This is called when
         * cancellation occurred during condition wait, and upon
         * insertion of a new waiter when lastWaiter is seen to have
         * been cancelled. This method is needed to avoid garbage
         * retention in the absence of signals. So even though it may
         * require a full traversal, it comes into play only when
         * timeouts or cancellations occur in the absence of
         * signals. It traverses all nodes rather than stopping at a
         * particular target to unlink all pointers to garbage nodes
         * without requiring many re-traversals during cancellation
         * storms.
         */
        private void unlinkCancelledWaiters() {
            Node t = firstWaiter;
            Node trail = null;
            while (t != null) {
                Node next = t.nextWaiter;
                if (t.waitStatus != Node.CONDITION) {
                    t.nextWaiter = null;
                    if (trail == null)
                        firstWaiter = next;
                    else
                        trail.nextWaiter = next;
                    if (next == null)
                        lastWaiter = trail;
                }
                else
                    trail = t;
                t = next;
            }
        }

        // public methods

        /**
         * 唤醒基于当前条件等待的一个线程，从第一个开始，加入到同步队列中，等待获取锁资源
         *
         * Moves the longest-waiting thread, if one exists, from the
         * wait queue for this condition to the wait queue for the
         * owning lock.
         *
         * @throws IllegalMonitorStateException if {@link #isHeldExclusively}
         *         returns {@code false}
         */
        public final void signal() {
            if (!isHeldExclusively())
                throw new IllegalMonitorStateException();
            Node first = firstWaiter;
            if (first != null)
                doSignal(first);
        }

        /**
         * 唤醒所有条件等待线程，加入到同步队列中
         *
         * Moves all threads from the wait queue for this condition to
         * the wait queue for the owning lock.
         *
         * @throws IllegalMonitorStateException if {@link #isHeldExclusively}
         *         returns {@code false}
         */
        public final void signalAll() {
            if (!isHeldExclusively())
                throw new IllegalMonitorStateException();
            Node first = firstWaiter;
            if (first != null)
                doSignalAll(first);
        }

        /**
         * Implements uninterruptible condition wait.
         * <ol>
         * <li> Save lock state returned by {@link #getState}.
         * <li> Invoke {@link #release} with saved state as argument,
         *      throwing IllegalMonitorStateException if it fails.
         * <li> Block until signalled.
         * <li> Reacquire by invoking specialized version of
         *      {@link #acquire} with saved state as argument.
         * </ol>
         */
        public final void awaitUninterruptibly() {
            Node node = addConditionWaiter();
            int savedState = fullyRelease(node);
            boolean interrupted = false;
            while (!isOnSyncQueue(node)) {
                LockSupport.park(this);
                if (Thread.interrupted())
                    interrupted = true;
            }
            if (acquireQueued(node, savedState) || interrupted)
                selfInterrupt();
        }

        /*
         * For interruptible waits, we need to track whether to throw
         * InterruptedException, if interrupted while blocked on
         * condition, versus reinterrupt current thread, if
         * interrupted while blocked waiting to re-acquire.
         */

        /** Mode meaning to reinterrupt on exit from wait */
        private static final int REINTERRUPT =  1;

        /** Mode meaning to throw InterruptedException on exit from wait */
        private static final int THROW_IE    = -1;

        /**
         * 在线程被激活后被调用，主要功能就是判断被激活的线程是否被中断 返回2种中断状态
         *
         * Checks for interrupt, returning THROW_IE if interrupted
         * before signalled, REINTERRUPT if after signalled, or
         * 0 if not interrupted.
         */
        private int checkInterruptWhileWaiting(Node node) {
            // THROW_IE是调用signal()前被中断返回
            // REINTERRUPT在调用signal()后被中断返回
            // 调用transferAfterCancelledWait(node)取判断是那种中断状态
            return Thread.interrupted() ?
                (transferAfterCancelledWait(node) ? THROW_IE : REINTERRUPT) :
                0;
        }

        /**
         * Throws InterruptedException, reinterrupts current thread, or
         * does nothing, depending on mode.
         */
        private void reportInterruptAfterWait(int interruptMode)
            throws InterruptedException {
            if (interruptMode == THROW_IE)
                throw new InterruptedException();
            else if (interruptMode == REINTERRUPT)
                selfInterrupt();
        }

        /**
         * 当前线程等待，进入条件队列
         * await()在被调用后先将当前线程加入到等待队列中，然后释放锁，最后阻塞当前线程
         *
         * Implements interruptible condition wait.
         * <ol>
         * <li> If current thread is interrupted, throw InterruptedException.
         * <li> Save lock state returned by {@link #getState}.
         * <li> Invoke {@link #release} with saved state as argument,
         *      throwing IllegalMonitorStateException if it fails.
         * <li> Block until signalled or interrupted.
         * <li> Reacquire by invoking specialized version of
         *      {@link #acquire} with saved state as argument.
         * <li> If interrupted while blocked in step 4, throw InterruptedException.
         * </ol>
         */
        public final void await() throws InterruptedException {
            // 1.如果当前线程被中断，则抛出中断异常
            if (Thread.interrupted())
                throw new InterruptedException();
            // 2.将节点加入到Condition队列中去，这里如果lastWaiter是cancel状态，那么会把它踢出Condition队列。
            Node node = addConditionWaiter();
            // 3.调用tryRelease，释放当前线程的锁
            int savedState = fullyRelease(node);
            int interruptMode = 0;
            // 4. 判断是否在同步阻塞队列，如果不在一直循环到被加入
            while (!isOnSyncQueue(node)) {
                //阻塞当前线程
                LockSupport.park(this);
                //判断是否被中断
                if ((interruptMode = checkInterruptWhileWaiting(node)) != 0)
                    break;
            }
            //获取锁，如果获取中被中断则设置中断状态
            if (acquireQueued(node, savedState) && interruptMode != THROW_IE)
                interruptMode = REINTERRUPT;
            //清除等待队列中被"激活"的节点
            if (node.nextWaiter != null) // clean up if cancelled
                unlinkCancelledWaiters();
            //如果当前线程被中断，处理中断逻辑
            if (interruptMode != 0)
                reportInterruptAfterWait(interruptMode);
        }

        /**
         * Implements timed condition wait.
         * <ol>
         * <li> If current thread is interrupted, throw InterruptedException.
         * <li> Save lock state returned by {@link #getState}.
         * <li> Invoke {@link #release} with saved state as argument,
         *      throwing IllegalMonitorStateException if it fails.
         * <li> Block until signalled, interrupted, or timed out.
         * <li> Reacquire by invoking specialized version of
         *      {@link #acquire} with saved state as argument.
         * <li> If interrupted while blocked in step 4, throw InterruptedException.
         * </ol>
         */
        public final long awaitNanos(long nanosTimeout)
                throws InterruptedException {
            if (Thread.interrupted())
                throw new InterruptedException();
            Node node = addConditionWaiter();
            int savedState = fullyRelease(node);
            final long deadline = System.nanoTime() + nanosTimeout;
            int interruptMode = 0;
            while (!isOnSyncQueue(node)) {
                if (nanosTimeout <= 0L) {
                    transferAfterCancelledWait(node);
                    break;
                }
                if (nanosTimeout >= spinForTimeoutThreshold)
                    LockSupport.parkNanos(this, nanosTimeout);
                if ((interruptMode = checkInterruptWhileWaiting(node)) != 0)
                    break;
                nanosTimeout = deadline - System.nanoTime();
            }
            if (acquireQueued(node, savedState) && interruptMode != THROW_IE)
                interruptMode = REINTERRUPT;
            if (node.nextWaiter != null)
                unlinkCancelledWaiters();
            if (interruptMode != 0)
                reportInterruptAfterWait(interruptMode);
            return deadline - System.nanoTime();
        }

        /**
         * Implements absolute timed condition wait.
         * <ol>
         * <li> If current thread is interrupted, throw InterruptedException.
         * <li> Save lock state returned by {@link #getState}.
         * <li> Invoke {@link #release} with saved state as argument,
         *      throwing IllegalMonitorStateException if it fails.
         * <li> Block until signalled, interrupted, or timed out.
         * <li> Reacquire by invoking specialized version of
         *      {@link #acquire} with saved state as argument.
         * <li> If interrupted while blocked in step 4, throw InterruptedException.
         * <li> If timed out while blocked in step 4, return false, else true.
         * </ol>
         */
        public final boolean awaitUntil(Date deadline)
                throws InterruptedException {
            long abstime = deadline.getTime();
            if (Thread.interrupted())
                throw new InterruptedException();
            Node node = addConditionWaiter();
            int savedState = fullyRelease(node);
            boolean timedout = false;
            int interruptMode = 0;
            while (!isOnSyncQueue(node)) {
                if (System.currentTimeMillis() > abstime) {
                    timedout = transferAfterCancelledWait(node);
                    break;
                }
                LockSupport.parkUntil(this, abstime);
                if ((interruptMode = checkInterruptWhileWaiting(node)) != 0)
                    break;
            }
            if (acquireQueued(node, savedState) && interruptMode != THROW_IE)
                interruptMode = REINTERRUPT;
            if (node.nextWaiter != null)
                unlinkCancelledWaiters();
            if (interruptMode != 0)
                reportInterruptAfterWait(interruptMode);
            return !timedout;
        }

        /**
         * Implements timed condition wait.
         * <ol>
         * <li> If current thread is interrupted, throw InterruptedException.
         * <li> Save lock state returned by {@link #getState}.
         * <li> Invoke {@link #release} with saved state as argument,
         *      throwing IllegalMonitorStateException if it fails.
         * <li> Block until signalled, interrupted, or timed out.
         * <li> Reacquire by invoking specialized version of
         *      {@link #acquire} with saved state as argument.
         * <li> If interrupted while blocked in step 4, throw InterruptedException.
         * <li> If timed out while blocked in step 4, return false, else true.
         * </ol>
         */
        public final boolean await(long time, TimeUnit unit)
                throws InterruptedException {
            long nanosTimeout = unit.toNanos(time);
            if (Thread.interrupted())
                throw new InterruptedException();
            Node node = addConditionWaiter();
            int savedState = fullyRelease(node);
            final long deadline = System.nanoTime() + nanosTimeout;
            boolean timedout = false;
            int interruptMode = 0;
            while (!isOnSyncQueue(node)) {
                if (nanosTimeout <= 0L) {
                    timedout = transferAfterCancelledWait(node);
                    break;
                }
                if (nanosTimeout >= spinForTimeoutThreshold)
                    LockSupport.parkNanos(this, nanosTimeout);
                if ((interruptMode = checkInterruptWhileWaiting(node)) != 0)
                    break;
                nanosTimeout = deadline - System.nanoTime();
            }
            if (acquireQueued(node, savedState) && interruptMode != THROW_IE)
                interruptMode = REINTERRUPT;
            if (node.nextWaiter != null)
                unlinkCancelledWaiters();
            if (interruptMode != 0)
                reportInterruptAfterWait(interruptMode);
            return !timedout;
        }

        //  support for instrumentation

        /**
         * Returns true if this condition was created by the given
         * synchronization object.
         *
         * @return {@code true} if owned
         */
        final boolean isOwnedBy(AbstractQueuedSynchronizer sync) {
            return sync == AbstractQueuedSynchronizer.this;
        }

        /**
         * Queries whether any threads are waiting on this condition.
         * Implements {@link AbstractQueuedSynchronizer#hasWaiters(ConditionObject)}.
         *
         * @return {@code true} if there are any waiting threads
         * @throws IllegalMonitorStateException if {@link #isHeldExclusively}
         *         returns {@code false}
         */
        protected final boolean hasWaiters() {
            if (!isHeldExclusively())
                throw new IllegalMonitorStateException();
            for (Node w = firstWaiter; w != null; w = w.nextWaiter) {
                if (w.waitStatus == Node.CONDITION)
                    return true;
            }
            return false;
        }

        /**
         * Returns an estimate of the number of threads waiting on
         * this condition.
         * Implements {@link AbstractQueuedSynchronizer#getWaitQueueLength(ConditionObject)}.
         *
         * @return the estimated number of waiting threads
         * @throws IllegalMonitorStateException if {@link #isHeldExclusively}
         *         returns {@code false}
         */
        protected final int getWaitQueueLength() {
            if (!isHeldExclusively())
                throw new IllegalMonitorStateException();
            int n = 0;
            for (Node w = firstWaiter; w != null; w = w.nextWaiter) {
                if (w.waitStatus == Node.CONDITION)
                    ++n;
            }
            return n;
        }

        /**
         * Returns a collection containing those threads that may be
         * waiting on this Condition.
         * Implements {@link AbstractQueuedSynchronizer#getWaitingThreads(ConditionObject)}.
         *
         * @return the collection of threads
         * @throws IllegalMonitorStateException if {@link #isHeldExclusively}
         *         returns {@code false}
         */
        protected final Collection<Thread> getWaitingThreads() {
            if (!isHeldExclusively())
                throw new IllegalMonitorStateException();
            ArrayList<Thread> list = new ArrayList<Thread>();
            for (Node w = firstWaiter; w != null; w = w.nextWaiter) {
                if (w.waitStatus == Node.CONDITION) {
                    Thread t = w.thread;
                    if (t != null)
                        list.add(t);
                }
            }
            return list;
        }
    }

    /**
     * Setup to support compareAndSet. We need to natively implement
     * this here: For the sake of permitting future enhancements, we
     * cannot explicitly subclass AtomicInteger, which would be
     * efficient and useful otherwise. So, as the lesser of evils, we
     * natively implement using hotspot intrinsics API. And while we
     * are at it, we do the same for other CASable fields (which could
     * otherwise be done with atomic field updaters).
     */
    private static final Unsafe unsafe = Unsafe.getUnsafe();
    /**
     * 记录state在AQS类中的偏移值
     */
    private static final long stateOffset;
    private static final long headOffset;
    private static final long tailOffset;
    private static final long waitStatusOffset;
    private static final long nextOffset;

    static {
        try {
            // 初始化state变量的偏移值
            stateOffset = unsafe.objectFieldOffset
                (AbstractQueuedSynchronizer.class.getDeclaredField("state"));
            headOffset = unsafe.objectFieldOffset
                (AbstractQueuedSynchronizer.class.getDeclaredField("head"));
            tailOffset = unsafe.objectFieldOffset
                (AbstractQueuedSynchronizer.class.getDeclaredField("tail"));
            waitStatusOffset = unsafe.objectFieldOffset
                (Node.class.getDeclaredField("waitStatus"));
            nextOffset = unsafe.objectFieldOffset
                (Node.class.getDeclaredField("next"));

        } catch (Exception ex) { throw new Error(ex); }
    }

    /**
     * 通过CAS设置head节点
     * 只有在enq方法中才会调用该方法...
     *
     * CAS head field. Used only by enq.
     */
    private final boolean compareAndSetHead(Node update) {
        return unsafe.compareAndSwapObject(this, headOffset, null, update);
    }

    /**
     * 通过CAS设置tail节点
     *
     * CAS tail field. Used only by enq.
     */
    private final boolean compareAndSetTail(Node expect, Node update) {
        return unsafe.compareAndSwapObject(this, tailOffset, expect, update);
    }

    /**
     * CAS waitStatus field of a node.
     */
    private static final boolean compareAndSetWaitStatus(Node node,
                                                         int expect,
                                                         int update) {
        return unsafe.compareAndSwapInt(node, waitStatusOffset,
                                        expect, update);
    }

    /**
     * CAS next field of a node.
     */
    private static final boolean compareAndSetNext(Node node,
                                                   Node expect,
                                                   Node update) {
        return unsafe.compareAndSwapObject(node, nextOffset, expect, update);
    }
}
