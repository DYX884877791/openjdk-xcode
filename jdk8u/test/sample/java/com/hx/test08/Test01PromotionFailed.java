package com.hx.test08;

//import org.openjdk.jol.vm.VM;
//import org.openjdk.jol.vm.VirtualMachine;

import static com.hx.test07.Test30PromotionFailed.touchMinorGc;

/**
 * Test01PromotionFailed
 * https://jerryhe.blog.csdn.net/article/details/108165244
 *
 * @author Jerry.X.He <970655147@qq.com>
 * @version 1.0
 * @date 2020-08-16 15:43
 */
public class Test01PromotionFailed {

  // identStr
  private String identStr = "identStr";

  public Test01PromotionFailed(String identStr) {
    this.identStr = identStr;
  }

  // Test01PromotionFailed
  // -Xmx32M -XX:+UseSerialGC -XX:NewRatio=1 -XX:MaxTenuringThreshold=1 -XX:+PrintGCDetails -cp .:/Users/jerry/.m2/repository/org/openjdk/jol/jol-core/0.8/jol-core-0.8.jar
  // 需要运行时 调整 DefNewGeneration 的相关代码, 这个示例 还需要在稍微调整一下, 能够体现出关系的
  public static void main(String[] args) throws Exception {

//    VirtualMachine vm = VM.current();
//    int _1M = 1 * 1024 * 800;
//
//    Test01PromotionFailed[] alreadyInOld = new Test01PromotionFailed[3 * _1M];
//    alreadyInOld[0] = new Test01PromotionFailed("alreadyInOld[0]");
//    System.out.println("alreadyInOld : " + vm.addressOf(alreadyInOld));
//    System.out.println("alreadyInOld[0] : " + vm.addressOf(alreadyInOld[0]));
//    touchMinorGc();
//    System.out.println("alreadyInOld : " + vm.addressOf(alreadyInOld));
//    System.out.println("alreadyInOld[0] : " + vm.addressOf(alreadyInOld[0]));
//
//    System.out.println(" ------------ alreadyInOld ------------ ");
//
//    Test01PromotionFailed[] promotionFailed = new Test01PromotionFailed[3 * _1M];
//    promotionFailed[0] = new Test01PromotionFailed("promotionFailed[0]");
//    promotionFailed[1] = alreadyInOld[0];
//    System.out.println("promotionFailed : " + vm.addressOf(promotionFailed));
//    System.out.println("promotionFailed[0] : " + vm.addressOf(promotionFailed[0]));
//    touchMinorGc();
//    System.out.println("promotionFailed : " + vm.addressOf(promotionFailed));
//    System.out.println("promotionFailed[0] : " + vm.addressOf(promotionFailed[0]));
//    System.out.println("alreadyInOld : " + vm.addressOf(alreadyInOld));
//    System.out.println("alreadyInOld[0] : " + vm.addressOf(alreadyInOld[0]));

    System.out.println(" ------------ promotionFailed ------------ ");

  }

}