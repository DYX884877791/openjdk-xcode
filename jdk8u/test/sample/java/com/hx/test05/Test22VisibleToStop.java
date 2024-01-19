package com.hx.test05;

import java.util.HashMap;

/**
 * Test22VisibleToStop
 * https://jerryhe.blog.csdn.net/article/details/105928222
 *
 * @author Jerry.X.He <970655147@qq.com>
 * @version 1.0
 * @date 2020-05-04 17:33
 */
public class Test22VisibleToStop {

  private static boolean stop = false;
  private static volatile boolean v2stop = false;
  private static HashMap<String, Boolean> map2stop = new HashMap<String, Boolean>();

  // Test22VisibleToStop
  // 卧槽 显式加上 -Xint, -Xcomp 都能够停止, 啥都不加 停止不了,, 呵呵 好神奇啊
  // -XX:+UnlockDiagnosticVMOptions -XX:+PrintAssembly -XX:CompileCommand=dontinline,*Test22VisibleToStop$3.run -XX:CompileCommand=compileonly,*Test22VisibleToStop$3.run
  // 呵呵增加了一下 参数之后 直接就退出了
  // refer : https://hllvm-group.iteye.com/group/topic/39675
  public static void main(String[] args) throws Exception {

    testMap();
    testVolatile();
    testBasicType();
    testBasicTypeWithSync();

  }

  //程序正常结束
  public static void testMap() throws InterruptedException{
    map2stop.put("stop", Boolean.FALSE);
    new Thread(){
      @Override
      public void run(){
        int i=0;
        while (!map2stop.get("stop") ){
          i++;
        }
        System.out.println("map loop finish:"+i);
      }
    }.start();

    Thread.sleep(1000);
    map2stop.put("stop", Boolean.TRUE);
    Thread.sleep(1000);
    System.out.println("map main stop");
  }

  //说明了volatile的开销
  public static void testVolatile() throws InterruptedException{
    new Thread(){
      @Override
      public void run(){
        int i=0;
        while(!v2stop){
          i++;
        }
        System.out.println("volatile loop finish:"+i);
      }
    }.start();

    Thread.sleep(1000);
    v2stop=true;
    Thread.sleep(1000);
    System.out.println("volatile main stop");
  }

  //不可见，发生死循环
  public static void testBasicType() throws InterruptedException{
    new Thread(){
      @Override
      public void run(){
        int i=0;
        while(!stop){
          i++;
        }
        System.out.println("basic type loop finish:"+i);
      }
    }.start();

    Thread.sleep(1000);
    stop=true;
    Thread.sleep(1000);
    System.out.println("basic type main stop");
  }

  //不可见，发生死循环
  public static void testBasicTypeWithSync() throws InterruptedException{
    Object lock = new Object();
    new Thread(){
      @Override
      public void run(){
        int i=0;
        while(!stop){
          synchronized (lock) {
            i++;
          }
        }
        System.out.println("basic type with sync loop finish:"+i);
      }
    }.start();

    Thread.sleep(1000);
    synchronized (lock) {
      stop = true;
    }
    Thread.sleep(1000);
    System.out.println("basic type with sync main stop");
  }

}