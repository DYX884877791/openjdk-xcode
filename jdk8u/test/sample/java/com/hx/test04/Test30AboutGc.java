package com.hx.test04;

import java.util.ArrayList;
import java.util.List;

/**
 * AboutGc
 * https://jerryhe.blog.csdn.net/article/details/105414127
 *
 * @author Jerry.X.He <970655147@qq.com>
 * @version 1.0
 * @date 2020-04-08 19:15
 */
public class Test30AboutGc {

  // Test30AboutGc
  // -Xms20M -Xmx20M -Xmn10M -XX:SurvivorRatio=6 -verbose:gc -XX:+PrintGCDetails -XX:+UseParallelGC
  // refer : https://hllvm-group.iteye.com/group/topic/39065
  public static void main(String[] args) throws Exception {

    //Thread.sleep(15000);
    List list = new ArrayList();
    for (int i = 0; i < 10; i++) {
      list.add(new MemoryObject(1024 * 1024));
      Thread.sleep(1000);
      System.out.println(i);
    }

//    System.gc();
//    System.gc();

    Thread.sleep(2000);
    list.clear();
    for (int i = 0; i < 10 ; i++) {
      list.add(new MemoryObject(1024 * 1024));
      if(i % 3 == 0){
        list.remove(0);
      }

      Thread.sleep(1000);
      System.out.println(i);
    }

    System.out.println("list size is " + list.size());
    Thread.sleep(2000);

  }

  // MemoryObject
  static class MemoryObject {
    byte [] b ;
    public MemoryObject(int b){
      this.b = new byte[b];
    }
  }

}