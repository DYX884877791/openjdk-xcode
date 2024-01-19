package com.hx.test04;
 
import sun.misc.Unsafe;
 
import java.io.*;
import java.lang.reflect.Constructor;
 
/**
 * DoubleCheckSingleton
 *
 * @author Jerry.X.He <970655147@qq.com>
 * @version 1.0
 * @date 2020-04-01 17:01
 */
public class Test19StaticClazzHolderSingleton implements Serializable {
 
  // INSTANCE
  private static volatile Test19StaticClazzHolderSingleton INSTANCE;
 
  // disable constructor
  private Test19StaticClazzHolderSingleton() {
//    throw new RuntimeException("can't instantiate !");
    System.out.println(" <init> called ");
  }
 
  // Test18DoubleCheckSingleton
  public static void main(String[] args) throws Exception {
 
    Test19StaticClazzHolderSingleton entity01 = Test19StaticClazzHolderSingleton.getInstance();
 
    // case1 constructor
    Class<Test19StaticClazzHolderSingleton> clazz = Test19StaticClazzHolderSingleton.class;
    Constructor<Test19StaticClazzHolderSingleton> constructor = clazz.getDeclaredConstructor();
    Test19StaticClazzHolderSingleton entity02 = constructor.newInstance();
 
    // case2 unsafe
    Unsafe unsafe = Test18DoubleCheckSingleton.getUnsafe();
    Test19StaticClazzHolderSingleton entity03 = (Test19StaticClazzHolderSingleton) unsafe.allocateInstance(Test19StaticClazzHolderSingleton.class);
 
    // case 3 deserialize
    ByteArrayOutputStream baos = new ByteArrayOutputStream();
    ObjectOutputStream oos = new ObjectOutputStream(baos);
    oos.writeObject(entity01);
    byte[] serialized = baos.toByteArray();
    ObjectInputStream ois = new ObjectInputStream(new ByteArrayInputStream(serialized));
    Test19StaticClazzHolderSingleton entity04 = (Test19StaticClazzHolderSingleton) ois.readObject();
 
    int x = 1;
 
  }
 
  /**
   * getInstance
   *
   * @return com.hx.test04.Test18DoubleCheckSingleton
   * @author Jerry.X.He<970655147@qq.com>
   * @date 2020-04-01 17:02
   */
  public static Test19StaticClazzHolderSingleton getInstance() {
    return SingletonHolder.INSTANCE;
  }
 
  /**
   * SingletonHolder
   *
   * @author Jerry.X.He <970655147@qq.com>
   * @version 1.0
   * @date 2020-04-01 18:07
   */
  private static class SingletonHolder {
    // instance
    private static Test19StaticClazzHolderSingleton INSTANCE = new Test19StaticClazzHolderSingleton();
 
  }
 
}