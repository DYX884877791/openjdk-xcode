package com.hx.test06;

import java.util.ArrayList;
import java.util.List;

/**
 * Test12LoadDiffDriver
 * https://jerryhe.blog.csdn.net/article/details/106312158
 *
 * @author Jerry.X.He <970655147@qq.com>
 * @version 1.0
 * @date 2020-05-23 16:31
 */
public class Test12LoadDiffDriver {

  // Test12LoadDiffDriver
  // -Xmx32M -XX:+HeapDumpOnOutOfMemoryError -XX:HeapDumpPath=/tmp/java/heapdump_Test12LoadDiffDriver.hprof
  public static void main(String[] args) throws Exception {

    InnerMysqlThread it1 = new InnerMysqlThread();
    InnerPostgresqlThread it2 = new InnerPostgresqlThread();
    it1.start();
    it2.start();

    for(int i=10; i>0; i--) {
      Thread.sleep(1000);
      System.out.println("count down : " + i);
    }
    createOOM();

  }

  /**
   * createOOM
   *
   * @return
   * @date 2020-05-23 17:34
   */
  public static void createOOM() {
    List<Object> list = new ArrayList<>();
    while(true) {
      list.add(new byte[1000_000]);
    }
  }

  // InnerMysqlThread
  static class InnerMysqlThread extends Thread {
    public InnerMysqlThread() {
      this.setName("InnerMysqlThread");
    }

    @Override
    public void run() {
      try {
        Class.forName("com.mysql.jdbc.Driver", true, this.getClass().getClassLoader());
        System.out.println("com.mysql.jdbc.Driver");
      } catch (ClassNotFoundException e) {
        e.printStackTrace();
      }
    }

  }

  // InnerPostgresqlThread
  static class InnerPostgresqlThread extends Thread {
    public InnerPostgresqlThread() {
      this.setName("InnerPostgresqlThread");
    }

    @Override
    public void run() {
      try {
        Class.forName("org.postgresql.Driver", true, this.getClass().getClassLoader());
        System.out.println("org.postgresql.Driver");

      } catch (ClassNotFoundException e) {
        e.printStackTrace();
      }
    }
  }
}