package com.hx.test04;

/**
 * Test26SynchronizeReentrantObject
 * https://jerryhe.blog.csdn.net/article/details/105331662
 * https://jerryhe.blog.csdn.net/article/details/105340003
 *
 * @author Jerry.X.He <970655147@qq.com>
 * @version 1.0
 * @date 2020-04-03 15:14
 */
public class Test26SynchronizeReentrantObject implements Cloneable {

  // identStr
  private String identStr = "xyz";
  int f01;
  int f02;
  int f03;
  int f04;
  int f05;

  // Test26SynchronizeReentrantObject
  public static void main(String[] args) throws Exception {

    Test26SynchronizeReentrantObject lockObj = new Test26SynchronizeReentrantObject();

    synchronized (lockObj) {
      synchronized (lockObj) {

//        Test26SynchronizeReentrantObject cloned = (Test26SynchronizeReentrantObject) lockObj.clone();
//        System.out.println(lockObj.identStr);

      }
    }

  }

}