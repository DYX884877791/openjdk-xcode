package com.hx.test03;

/**
 * CloneCallObjectConstructor
 * https://jerryhe.blog.csdn.net/article/details/105360058
 * https://jerryhe.blog.csdn.net/article/details/106592691
 *
 * @author Jerry.X.He <970655147@qq.com>
 * @version 1.0
 * @date 2019-12-29 14:41
 */
public class Test05CloneCallObjectConstructor implements Cloneable {

  // Test05CloneCallObjectConstructor
  public static void main(String[] args) throws Exception {

    Test05CloneCallObjectConstructor obj = new Test05CloneCallObjectConstructor();
    Test05CloneCallObjectConstructor cloned = (Test05CloneCallObjectConstructor) obj.clone();

    System.out.println(obj == cloned);

  }

}