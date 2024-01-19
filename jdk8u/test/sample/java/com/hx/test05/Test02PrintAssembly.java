package com.hx.test05;

/**
 * PrintAssembly
 * https://jerryhe.blog.csdn.net/article/details/105440358
 * https://www.cnblogs.com/davidwang456/p/3464542.html
 *
 * @author Jerry.X.He <970655147@qq.com>
 * @version 1.0
 * @date 2020-04-10 15:32
 */
public class Test02PrintAssembly {

  int a = 1;
  static int b = 2;

  // Test02PrintAssembly
//  -Xcomp -XX:+UnlockDiagnosticVMOptions -XX:+PrintAssembly -XX:CompileCommand=dontinline,*Test02PrintAssembly.foo -XX:CompileCommand=compileonly,*Test02PrintAssembly.foo
  public static void main(String[] args) {

    new Test02PrintAssembly().foo(3);

  }

  // foo
  public int foo(int c) {
    return a + b + c;
  }

}