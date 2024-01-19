package com.hx.test07;

import com.hx.test04.Test18DoubleCheckSingleton;
import sun.misc.Unsafe;

import java.util.concurrent.locks.LockSupport;

/**
 * JStackLog
 * https://jerryhe.blog.csdn.net/article/details/107136988
 *
 * @author Jerry.X.He <970655147@qq.com>
 * @version 1.0
 * @date 2020-07-05 11:10
 */
public class Test08JStackLog {

  // Test08JStackLog
  public static void main(String[] args) {

    Object lock = new Object();
    Unsafe unsafe = Test18DoubleCheckSingleton.getUnsafe();

    new Thread("sleepThread") {
      @Override
      public void run() {
        synchronized (lock) {
          try {
            Thread.sleep(1000 * 1000);
          } catch (Exception e) {
            e.printStackTrace();
          }
        }
      }
    }.start();

    new Thread("waitThread") {
      @Override
      public void run() {
        synchronized (lock) {
          try {
            lock.wait();
          } catch (Exception e) {
            e.printStackTrace();
          }
        }
      }
    }.start();


    new Thread("parkThread") {
      @Override
      public void run() {
        LockSupport.park(lock);
      }
    }.start();

    new Thread("parkThread02") {
      @Override
      public void run() {
        LockSupport.park();
      }
    }.start();

    new Thread("runnableThread") {
      @Override
      public void run() {
        int i = 0;
        while (true) {
          i++;
        }
      }
    }.start();

  }

}