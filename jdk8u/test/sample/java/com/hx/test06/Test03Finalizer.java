package com.hx.test06;

import static com.hx.test05.Test20DefNewGc.touchMinorGc;

/**
 * Test03Finalizer
 * https://jerryhe.blog.csdn.net/article/details/106170432
 *
 * @author Jerry.X.He <970655147@qq.com>
 * @version 1.0
 * @date 2020-05-17 09:31
 */
public class Test03Finalizer {

  // identStr
  private String identStr = "identStr";
  int f01;
  int f02;
  int f03;
  int f04;
  int f05;

  // Test03Finalizer
  // vmOpts : -Xint -server -Xmx600m -Xms600m -XX:PermSize=128M -XX:MaxPermSize=128M -XX:NewSize=128m -XX:MaxNewSize=128m -XX:SurvivorRatio=8 -XX:+UseSerialGC -XX:+PrintGCDetails
  public static void main(String[] args) {

    Test03Finalizer obj = new Test03Finalizer();
    obj = null;

    touchMinorGc();

    int x = 0;

  }

  @Override
  protected void finalize() throws Throwable {
    System.out.println(" finialize ");
  }

}