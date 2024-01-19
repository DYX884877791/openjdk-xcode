package com.hx.test06;

import java.lang.ref.*;
import java.lang.reflect.Field;

import static com.hx.test05.Test20DefNewGc.touchMinorGc;

/**
 * ReferenceProcessor
 * https://jerryhe.blog.csdn.net/article/details/106161478
 *
 * @author Jerry.X.He <970655147@qq.com>
 * @version 1.0
 * @date 2020-05-16 11:39
 */
public class Test02ReferenceProcessor01 {

  // identStr
  private String identStr = "identStr";
  int f01;
  int f02;
  int f03;
  int f04;
  int f05;

  // Test02ReferenceProcessor01
  // vmOpts : -Xint -server -Xmx600m -Xms600m -XX:PermSize=128M -XX:MaxPermSize=128M -XX:NewSize=128m -XX:MaxNewSize=128m -XX:SurvivorRatio=8 -XX:+UseSerialGC -XX:+PrintGCDetails
  public static void main(String[] args) throws Exception {

    ReferenceQueue<Test02ReferenceProcessor01> refQueue = new ReferenceQueue<>();
    Test02ReferenceProcessor01 obj = new Test02ReferenceProcessor01();

    SoftReference<Test02ReferenceProcessor01> softRef = new SoftReference<>(obj, refQueue);
    WeakReference<Test02ReferenceProcessor01> weakRef = new WeakReference<>(obj, refQueue);
    PhantomReference<Test02ReferenceProcessor01> phantomReference = new PhantomReference<>(obj, refQueue);

    obj = null;
    touchMinorGc();

    Thread.sleep(1000);
    Field lengthField = ReferenceQueue.class.getDeclaredField("queueLength");
    lengthField.setAccessible(true);
    System.out.println(lengthField.get(refQueue));
    Reference<? extends Test02ReferenceProcessor01> reference = refQueue.poll();
    int x = 0;

  }

  @Override
  protected void finalize() throws Throwable {
    System.out.println(" finialize ");
  }

}