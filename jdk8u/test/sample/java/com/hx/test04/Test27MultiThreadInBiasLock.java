package com.hx.test04;

/**
 * Test27MultiThreadInBiasLock
 * https://jerryhe.blog.csdn.net/article/details/105381926
 *
 * @author Jerry.X.He <970655147@qq.com>
 * @version 1.0
 * @date 2020-04-03 15:14
 */
public class Test27MultiThreadInBiasLock implements Cloneable {

  // identStr
  private String identStr = "xyz";
  int f01;
  int f02;
  int f03;
  int f04;
  int f05;

  // Test25SynchronizeObject
  public static void main(String[] args) throws Exception {

    Test27MultiThreadInBiasLock lockObj = new Test27MultiThreadInBiasLock();

    doClone(lockObj);
    synchronized (lockObj) {

    }

    new Thread() {
      @Override
      public void run() {
        doClone(lockObj);
        synchronized (lockObj) {

        }
      }
    }.start();

    Test26SynchronizeObject.sleep(2000);
  }

  // doClone
  private static void doClone(Test27MultiThreadInBiasLock obj) {
    try {
      obj.clone();
    } catch (Exception e) {
      e.printStackTrace();
    }
  }

}