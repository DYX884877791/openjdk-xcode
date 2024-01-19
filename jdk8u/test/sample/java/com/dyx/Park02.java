package com.dyx;

import java.util.concurrent.locks.LockSupport;

/**
 * @author dengyouxu
 * @desc TODO
 * @since 2023/12/29 16:30
 */
public class Park02 {

    public static void main(String[] args) {
        System.out.println("begin park");
        //使当前线程先获取到许可证
        LockSupport.unpark(Thread.currentThread());
        //再次调用park方法,先获得了许可,因此该方法不会阻塞
        LockSupport.park();
        System.out.println("end park");
    }

}
