package com.hx.test05;

import java.util.ArrayList;

/**
 * DefNewGc
 * https://jerryhe.blog.csdn.net/article/details/105900027
 *
 * @author Jerry.X.He <970655147@qq.com>
 * @version 1.0
 * @date 2020-05-02 18:15
 */
public class Test20DefNewGc implements Cloneable {

  // static vars
  private static Test20DefNewGc objInOldGen;
  private static Test20DefNewGc objInYoungGen;
  private static Test20DefNewGc objWillGc;
  private static ArrayList list;
  // instance vars
  private String identStr;
  private Object refObj;

  public Test20DefNewGc(String identStr){
    this.identStr = identStr;
  }

  // Test20DefNewGc
  // refer : https://hllvm-group.iteye.com/group/topic/35798
  // vmOpts : -Xint -server -Xmx600m -Xms600m -XX:PermSize=128M -XX:MaxPermSize=128M -XX:NewSize=128m -XX:MaxNewSize=128m -XX:SurvivorRatio=8 -XX:-UseTLAB -XX:+UseSerialGC -XX:CMSInitiatingOccupancyFraction=50 -XX:+PrintGCDetails
  // vm调试 : -Xint -server -Xmx600m -Xms600m -XX:PermSize=128M -XX:MaxPermSize=128M -XX:NewSize=128m -XX:MaxNewSize=128m -XX:SurvivorRatio=8 -XX:-UseTLAB -XX:+UseSerialGC -XX:CMSInitiatingOccupancyFraction=50 -XX:+PrintGCDetails  com.hx.test05.Test20DefNewGc
  public static void main(String[] args) throws Exception {

    // objInOldGen in old gen
    objInOldGen = new Test20DefNewGc("objInOldGen");
    for(int i=0; i<16; i++) {
      touchMinorGc();
    }

    // objInOldGen.refObj in young gen
    objInOldGen.refObj = new Object();

    // objInYoungGen in young gen
    objInYoungGen = new Test20DefNewGc("objInYoungGen");
    objInYoungGen.refObj = new Object();

    // objWillGc in young gen
    objWillGc = new Test20DefNewGc("objWillGc");
    objWillGc.refObj = new Object();
    objWillGc = null;

    // list in young gen
    list = new ArrayList();
    list.add(new Test20DefNewGc("objInList"));
    list.add(new String("this is second element"));

    // objRefByStackVar in youngGen
    Test20DefNewGc objRefByStackVar = new Test20DefNewGc("objRefByStackVar");
    objRefByStackVar.refObj = new Object();

    // invoke final gc
    doClone(objInOldGen);
    System.out.println(" before final gc ");
    touchMinorGc();

  }

  /**
   * touchMinorGc
   *
   * @return void
   * @author Jerry.X.He<970655147@qq.com>
   * @date 2020-05-02 21:20
   */
  public static void touchMinorGc() {
    int _1K = 1 * 1000;
    int _10M = 10 * 1000 * _1K;
    for(int i=0; i<12; i++) {
      byte[] bytes = new byte[_10M];
    }
  }

  /**
   * doClone
   *
   * @author Jerry.X.He <970655147@qq.com>
   * @version 1.0
   * @date 2020-04-05 11:01
   */
  public static void doClone(Test20DefNewGc obj) {
    try {
      obj.clone();
    } catch (Exception e) {
      e.printStackTrace();
    }
  }

}