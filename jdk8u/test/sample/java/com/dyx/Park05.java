package com.dyx;

import java.util.concurrent.locks.LockSupport;

/**
 * @author dengyouxu
 * @desc park中断测试
 * @since 2023/12/29 16:32
 */
public class Park05 {

    public static void main(String[] args) throws Exception {
        Thread thread = new Thread(new Runnable() {
            @Override
            public void run() {
                //最开始中断标志位位false
                System.out.println(Thread.currentThread().isInterrupted());
                long currentTimeMillis = System.currentTimeMillis();
                System.out.println("begin park");
                LockSupport.park();
                System.out.println("end park");
                System.out.println(System.currentTimeMillis() - currentTimeMillis);
                //调用interrupt方法之后,中断标志位为true
                System.out.println(Thread.currentThread().isInterrupted());
            }
        });
        thread.start();
        //开放或者注释该行代码,观察end park时间
        Thread.sleep(2000);
        //使用interrupt,也可以中断因为park造成的阻塞,但是该中断不会抛出异常
        thread.interrupt();

    }
}
