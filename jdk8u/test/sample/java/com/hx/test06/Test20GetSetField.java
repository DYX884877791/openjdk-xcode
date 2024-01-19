package com.hx.test06;

/**
 * GetSetField
 * https://jerryhe.blog.csdn.net/article/details/106601670
 *
 * @author Jerry.X.He <970655147@qq.com>
 * @version 1.0
 * @date 2020-06-07 09:17
 */
public class Test20GetSetField {

  // identStr
  private static Test20GetSetField staticInstance = new Test20GetSetField();
  private static Test20GetSetField anotherStaticInstance = new Test20GetSetField();
  private String identStr = "identStr";
  int f01;
  int f02;
  int f03;

  // Test20GetSetField
  public static void main(String[] args) {

    staticInstance = anotherStaticInstance;

    staticInstance.f02 = 10;
    int x = staticInstance.f02;

  }

}