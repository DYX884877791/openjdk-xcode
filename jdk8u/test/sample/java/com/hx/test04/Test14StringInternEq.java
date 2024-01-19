package com.hx.test04;

/**
 * StringInternEq
 * https://jerryhe.blog.csdn.net/article/details/105013019
 * https://jerryhe.blog.csdn.net/article/details/122548993
 *
 * @author Jerry.X.He <970655147@qq.com>
 * @version 1.0
 * @date 2020-03-21 16:18
 */
public class Test14StringInternEq implements Cloneable {

  int f01;
  int f02;
  int f03;
  int f04;
  int f05;
  int f06;

  // String.intern
  // -Xint -Xmx10M -XX:+UseSerialGC  -XX:+PrintGCDetails
  public static void main(String[] args) throws Exception {

    String str01 = "2372826".intern();
    str01 = null;
    createFullGc();
    String str02 = "2372826".intern();

    System.out.println(str01 == str02);

  }

  // createFullGc
  public static void createFullGc() {
    for(int i=0; i<2; i++) {
      byte[] bytes = new byte[4 * 1024 * 1024];
    }
  }

}