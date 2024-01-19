package com.hx.test04;

/**
 * AllocationTest
 * https://jerryhe.blog.csdn.net/article/details/104701950
 *
 * @author Jerry.X.He <970655147@qq.com>
 * @version 1.0
 * @date 2020-03-06 15:24
 */
public class Test03AllocationTest {

  /**
   * constants
   */
  private static final int _1KB = 1024;
  private static final int _1MB = _1KB * 1024;

  // Test03AllocationTest
  // refer : https://hllvm-group.iteye.com/group/topic/38293
  /**
   * -Xint -Xmx100M -XX:+UseParallelGC -XX:-UseTLAB -XX:+PrintGCDetails -XX:MaxNewSize=40M -XX:NewSize=28M
   */
  public static void main(String[] args) {

    testAllocation();

  }

  // testAllocation
  public static void testAllocation() {

    byte[] byte1 = new byte[_1MB*5];
    byte[] byte2 = new byte[_1MB*10];
    byte1 = null;
    byte2 = null;

    byte[] byte3 = new byte[_1MB*5];
    byte[] byte4 = new byte[_1MB*10];
    byte3 = null;
    byte4 = null;

    byte[] byte5 = new byte[_1MB*15];

  }

}