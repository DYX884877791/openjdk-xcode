package com.hx.test04;

/**
 * LoadRefEachOther
 * https://jerryhe.blog.csdn.net/article/details/104733313
 *
 * @author Jerry.X.He <970655147@qq.com>
 * @version 1.0
 * @date 2020-03-08 10:58
 */
public class Test06LoanRefEachOther {

  // Test06LoanRefEachOther
  // refer : https://hllvm-group.iteye.com/group/topic/38847
  public static void main(String[] args) {

    System.out.println(Clazz1.x1);
//    System.out.println(Clazz2.x1);
    System.out.println(Clazz2.x2);

  }

  /**
   * Clazz1
   *
   * @author Jerry.X.He <970655147@qq.com>
   * @version 1.0
   * @date 2020-03-08 10:59
   */
  private static class Clazz1 {

    static int x1 = 1;
    int f01;
    int f02;
    int f03;
    int f04;
    int f05;

    static {
      x1 = Clazz2.x2;
    }

  }

  /**
   * Clazz2
   *
   * @author Jerry.X.He <970655147@qq.com>
   * @version 1.0
   * @date 2020-03-08 11:00
   */
  private static class Clazz2 extends Clazz1 {

    static int x2 = 2;
    int f01;
    int f02;
    int f03;
    int f04;
    int f05;

    static {
      x2 = Clazz1.x1;
    }

  }

}