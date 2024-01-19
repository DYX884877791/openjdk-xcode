package com.dyx;

import java.util.concurrent.locks.LockSupport;

/**
 * @author dengyouxu
 * @desc TODO
 * @since 2023/12/29 16:31
 */
public class Park03 {

    public static void main(String[] args) throws Exception {
        Thread thread = new Thread(new Runnable() {
            @Override
            public void run() {
                long currentTimeMillis = System.currentTimeMillis();
                System.out.println("begin park");
                LockSupport.park();
                System.out.println("end park");
                System.out.println(System.currentTimeMillis() - currentTimeMillis);
            }
        });
        thread.start();
        //开放或者注释该行代码,观察end park时间
        Thread.sleep(2000);
        //使当子线程获取到许可证
        LockSupport.unpark(thread);

    }
}
