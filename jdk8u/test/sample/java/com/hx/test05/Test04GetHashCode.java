package com.hx.test05;

/**
 * GetHashCode
 * https://jerryhe.blog.csdn.net/article/details/105454989
 *
 * @author Jerry.X.He <970655147@qq.com>
 * @version 1.0
 * @date 2020-04-11 15:34
 */
public class Test04GetHashCode {

  // identStr
  private String identStr = "xyz";
  int f01;
  int f02;
  int f03;
  int f04;
  int f05;

  // Test04GetHashCode
  // -XX:hashCode=2 : 总是返回常量1作为所有对象的identity hash code（跟地址无关）
  // -XX:hashCode=4 : 使用对象地址的“当前”地址来作为它的identity hash code（就是当前地址）
  // refer : https://hllvm-group.iteye.com/group/topic/39183
  public static void main(String[] args) {

    Test04GetHashCode obj = new Test04GetHashCode();
    int hashCode = obj.hashCode();
    System.out.println(hashCode);
    int x = 0;

  }

}