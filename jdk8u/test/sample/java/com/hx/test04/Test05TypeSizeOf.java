package com.hx.test04;

import java.util.ArrayList;
import java.util.Date;
import java.util.List;

/**
 * https://jerryhe.blog.csdn.net/article/details/104736093
 * https://hllvm-group.iteye.com/group/topic/38670
 * TypeSizeOf
 *
 * @author Jerry.X.He <970655147@qq.com>
 * @version 1.0
 * @date 2020-03-07 16:04
 */
public class Test05TypeSizeOf {

  // fields
  private long id = 1;
  private Test05TypeSizeOf test2 = null;
  private List<Test05TypeSizeOf> list = new ArrayList<>();
  private Date date = new Date();
  private byte status = 2;
  private byte count = 3;

  // refer : https://hllvm-group.iteye.com/group/topic/38670
  // -ea -javaagent:/Users/jerry/Tmp/agent/HelloWorld-1.0-SNAPSHOT_agent.jar
  // vm 调试相关参数为 -da -dsa -Xint -Xmx100M -XX:+UseSerialGC -javaagent:/Users/jerry/Tmp/agent/HelloWorld-1.0-SNAPSHOT_agent.jar com.hx.test04.Test05TypeSizeOf
  public static void main(String[] args) {

//    long sizeOfTest05TypeOfSizeOf = Test01PremainAgentClazz.inst.getObjectSize(new Test05TypeSizeOf());
//    System.out.println(sizeOfTest05TypeOfSizeOf);

  }

}