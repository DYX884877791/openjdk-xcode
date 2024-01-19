package com.hx.test07;

//import org.openjdk.jol.vm.VM;
//import org.openjdk.jol.vm.VirtualMachine;

import java.lang.management.ManagementFactory;
import java.lang.management.MemoryPoolMXBean;
import java.util.List;

/**
 * PromitionFailed
 * https://jerryhe.blog.csdn.net/article/details/107898390
 *
 * @author Jerry.X.He <970655147@qq.com>
 * @version 1.0
 * @date 2020-08-09 14:40
 */
public class Test30PromotionFailed {

  // Test30PromotionFailed
  // -Xmx32M -XX:+UseSerialGC -XX:NewRatio=1 -XX:MaxTenuringThreshold=2 -XX:+PrintGCDetails -cp .:/Users/jerry/.m2/repository/org/openjdk/jol/jol-core/0.8/jol-core-0.8.jar com.hx.test07.Test30PromotionFailed
  // young : 13 + 1.5 + 1.5, old : 16
  public static void main(String[] args) throws Exception {

//    VirtualMachine vm = VM.current();
//    int _1M = 1 * 1024 * 1204;
//
//    byte[] alreadyInOld = new byte[7 * _1M];
//    // young
//    System.out.println(vm.addressOf(alreadyInOld));
//    touchMinorGc();
//    touchMinorGc();
//    // old
//    System.out.println(vm.addressOf(alreadyInOld));
//    System.out.println(" ------------ alreadyInOld ------------ ");
//
//    byte[] promotionToOld01 = new byte[3 * _1M];
//    byte[] promotionToOld02 = new byte[3 * _1M];
//    // young
//    System.out.println(vm.addressOf(promotionToOld01));
//    // young
//    System.out.println(vm.addressOf(promotionToOld02));
//    touchMinorGc();
//    touchMinorGc();
//    // old
//    System.out.println(vm.addressOf(promotionToOld01));
//    // young
//    System.out.println(vm.addressOf(promotionToOld02));
//    System.out.println(" ------------ promotionToOldXX ------------ ");
//
//    byte[] allocatedInYoung = new byte[7 * _1M];
//    byte[] allocatedInOld = new byte[(int) (1.6 * _1M)];
//
//    // young
//    System.out.println(vm.addressOf(allocatedInYoung));
//    // old
//    System.out.println(vm.addressOf(allocatedInOld));
    System.out.println(" ------------ allocatedInXXX ------------ ");

  }

  /**
   * touchMinorGc
   *
   * @return void
   * @author Jerry.X.He<9 7 0 6 5 5 1 4 7 @ qq.com>
   * @date 2020-05-02 21:20
   */
  public static void touchMinorGc() throws Exception {
    int _1M = 1 * 1024 * 1024;
    MemoryPoolMXBean eden = getMemoryInfo("Eden Space");
    long used = eden.getUsage().getUsed();
    long max = eden.getUsage().getMax();

    int loopByM = (int) ((max - used) / _1M) + 1;
    System.out.println(String.format("max : %sM, used : %sM, touchMinorGc created %s byte[1M] ", (max / _1M), (used / _1M), loopByM));
    for (int i = 0; i < loopByM; i++) {
      byte[] bytes = new byte[_1M];
    }
  }

  /**
   * 获取 给定的模块的内存信息
   *
   * @param name name
   * @return java.lang.management.MemoryPoolMXBean
   * @author Jerry.X.He
   * @date 2020-08-16 10:15
   */
  public static MemoryPoolMXBean getMemoryInfo(String name) {
    List<MemoryPoolMXBean> mxbs = ManagementFactory.getMemoryPoolMXBeans();
    for (MemoryPoolMXBean mxb : mxbs) {
      if (mxb.getName().equals(name)) {
        return mxb;
      }
    }
    return null;
  }

}