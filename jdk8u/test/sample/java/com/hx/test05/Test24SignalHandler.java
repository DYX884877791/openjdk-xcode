package com.hx.test05;

import sun.misc.Signal;
import sun.misc.SignalHandler;

/**
 * SignalHandler
 * https://jerryhe.blog.csdn.net/article/details/105930146
 *
 * @author Jerry.X.He <970655147@qq.com>
 * @version 1.0
 * @date 2020-05-05 10:53
 */
public class Test24SignalHandler {

  // Test24SignalHandler
  public static void main(String[] args) throws Exception {

    Signal.handle(new Signal("ALRM"), new SignalHandler() {
      @Override
      public void handle(Signal signal) {
        System.out.println(signal);
      }
    });

    // 发送信号
//    Signal.raise(new Signal("ALRM"));
    System.in.read();

  }

}