package com.dyx;

import java.util.concurrent.locks.LockSupport;

/**
 * @author dengyouxu
 * @desc TODO
 * @since 2023/12/29 16:30
 */
public class Park01 {

    public static void main(String[] args) {
        System.out.println("begin park");
        //调用park方法
        LockSupport.park();
        //使当前线程获取到许可证,明显执行不到这一步来,因为在上一步就已经阻塞了
        LockSupport.unpark(Thread.currentThread());
        System.out.println("end park");

    }

}
