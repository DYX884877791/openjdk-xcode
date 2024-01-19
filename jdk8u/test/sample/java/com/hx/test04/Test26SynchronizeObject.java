package com.hx.test04;

/**
 * SynchronizedObject
 * https://jerryhe.blog.csdn.net/article/details/105315424
 * https://jerryhe.blog.csdn.net/article/details/105330793
 * https://jerryhe.blog.csdn.net/article/details/105339063
 *
 * @author Jerry.X.He <970655147@qq.com>
 * @version 1.0
 * @date 2020-04-03 15:14
 */
public class Test26SynchronizeObject implements Cloneable {

    // identStr
    private String identStr = "xyz";
    int f01;
    int f02;
    int f03;
    int f04;
    int f05;

    // Test25SynchronizeObject
    public static void main(String[] args) throws Exception {

        Test26SynchronizeObject lockObj = new Test26SynchronizeObject();


        new Thread(new Runnable() {

            @Override
            public void run() {
                synchronized (lockObj) {
                    System.out.println("进入Thread-1运行:" + lockObj.identStr);
                }
            }


        }, "Thread-1").start();


        synchronized (lockObj) {

            Test26SynchronizeObject cloned = (Test26SynchronizeObject) lockObj.clone();
            System.out.println("进入Test26SynchronizeObject的main方法执行:" + lockObj.identStr);

        }
        Thread.sleep(Integer.MAX_VALUE);



    }

    public static void doClone(Test26SynchronizeObject obj) {
        try {
            obj.clone();
        } catch (Exception e) {
            e.printStackTrace();
        }
    }

    public static void sleep(long millis) {

    }


}