package com.hx.test05;

import com.hx.test04.Test26MethodOrder;

import java.lang.reflect.Method;

/**
 * GenerateMethodAccessor
 * https://jerryhe.blog.csdn.net/article/details/105882661
 *
 * @author Jerry.X.He <970655147@qq.com>
 * @version 1.0
 * @date 2020-04-25 19:17
 */
public class Test16GenerateMethodAccessor {

  // Test16GenerateMethodAccessor
  public static void main(String[] args) throws Exception {

    Method method = Test26MethodOrder.class.getDeclaredMethod("func008");
    method.setAccessible(true);
    for(int i=0; i<=16; i++) {
      method.invoke(null);
    }

  }

}