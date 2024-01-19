package com.dyx;

import java.util.concurrent.locks.LockSupport;

/**
 * @author dengyouxu
 * @desc park blocker测试
 *
 * 使用park，不能看到blocker信息
 * 使用park(blocker)，可以看到blocker信息，因此推荐使用该方法阻塞线程
 *
 * @since 2023/12/29 16:33
 */
public class Park06 {
    public static void main(String[] args) {
        //分别尝试注释这两行代码,运行程序,运行cmd,使用jps  命令,找到该进程对应的pid,然后使用jstack pid   命令,就可以看到线程信息.
        //LockSupport.park();
        LockSupport.park(new Park06());
    }
}
