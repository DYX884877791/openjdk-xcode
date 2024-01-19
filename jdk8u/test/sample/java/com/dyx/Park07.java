package com.dyx;

import java.util.concurrent.locks.LockSupport;

/**
 * @author dengyouxu
 * @desc https://blog.csdn.net/liujiqing123/article/details/118554694
 * @since 2023/12/29 16:39
 */
public class Park07 {

    public static void main(String[] args) {
        Thread t = Thread.currentThread();
        new Thread(()->{
            try {
                Thread.sleep(1000);
                System.out.println("unpark1");
                LockSupport.unpark(t);
                // 尝试分别注释该行sleep代码，观察结果...
                // Thread.sleep(1);
                LockSupport.unpark(t);
                System.out.println("unpark finish");
            } catch (InterruptedException e) {
                e.printStackTrace();
            }

        }).start();
        System.out.println("park1");
        LockSupport.park();
        System.out.println("park2");
        LockSupport.park();
        System.out.println("park finish");
    }

}
