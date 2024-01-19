package com.hx.test05;

/**
 * SinglePrintAssembly
 * https://jerryhe.blog.csdn.net/article/details/105449147
 *
 * @author Jerry.X.He <970655147@qq.com>
 * @version 1.0
 * @date 2020-04-10 15:52
 */
public class Test03SingletonPrintAssembly {

  // INSTANCE
  private static volatile Test03SingletonPrintAssembly INSTANCE;

  // Test03SingletonPrintAssembly
//  -Xcomp -XX:+UnlockDiagnosticVMOptions -XX:+PrintAssembly -XX:CompileCommand=dontinline,*Test03SingletonPrintAssembly.getInstance -XX:CompileCommand=compileonly,*Test03SingletonPrintAssembly.getInstance
  public static void main(String[] args) {

    Test03SingletonPrintAssembly.getInstance();

  }

  // getInstance
  public static Test03SingletonPrintAssembly getInstance() {
    if(INSTANCE == null) {
      synchronized (Test03SingletonPrintAssembly.class) {
        if(INSTANCE == null) {
          INSTANCE = new Test03SingletonPrintAssembly();
        }
      }
    }

    return INSTANCE;
  }

}