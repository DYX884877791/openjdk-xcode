package com.hx.test04;
 
import sun.misc.Unsafe;
 
import java.io.*;
import java.lang.reflect.Constructor;
import java.lang.reflect.Field;
 
/**
 * DoubleCheckSingleton
 *
 * @author Jerry.X.He <970655147@qq.com>
 * @version 1.0
 * @date 2020-04-01 17:01
 */
public class Test18DoubleCheckSingleton implements Serializable {
 
  // INSTANCE
  private static volatile Test18DoubleCheckSingleton INSTANCE;
 
  // disable constructor
  private Test18DoubleCheckSingleton() {
//    throw new RuntimeException("can't instantiate !");
    System.out.println(" <init> called ");
  }
 
  // Test18DoubleCheckSingleton
  public static void main(String[] args) throws Exception {
 
    Test18DoubleCheckSingleton entity01 = Test18DoubleCheckSingleton.getInstance();
 
    // case 1 constructor
    Class<Test18DoubleCheckSingleton> clazz = Test18DoubleCheckSingleton.class;
    Constructor<Test18DoubleCheckSingleton> constructor = clazz.getDeclaredConstructor();
    Test18DoubleCheckSingleton entity02 = constructor.newInstance();
 
    // case 2 unsafe
    Unsafe unsafe = getUnsafe();
    Test18DoubleCheckSingleton entity03 = (Test18DoubleCheckSingleton) unsafe.allocateInstance(Test18DoubleCheckSingleton.class);
 
    // case 3 deserialize
    ByteArrayOutputStream baos = new ByteArrayOutputStream();
    ObjectOutputStream oos = new ObjectOutputStream(baos);
    oos.writeObject(entity01);
    byte[] serialized = baos.toByteArray();
    ObjectInputStream ois = new ObjectInputStream(new ByteArrayInputStream(serialized));
    Test18DoubleCheckSingleton entity04 = (Test18DoubleCheckSingleton) ois.readObject();
 
    int x = 1;
 
  }
 
  /**
   * getInstance
   *
   * @return com.hx.test04.Test18DoubleCheckSingleton
   * @author Jerry.X.He<970655147@qq.com>
   * @date 2020-04-01 17:02
   */
  public static Test18DoubleCheckSingleton getInstance() {
    if(INSTANCE == null) {
      synchronized (Test18DoubleCheckSingleton.class) {
        if(INSTANCE == null) {
          INSTANCE = new Test18DoubleCheckSingleton();
        }
      }
    }
 
    return INSTANCE;
  }
 
  /**
   * getUnsafe
   *
   * @return sun.misc.Unsafe
   * @author Jerry.X.He<970655147@qq.com>
   * @date 2020-04-01 17:11
   */
  public static Unsafe getUnsafe() {
      try {
        Field unsafeField = Unsafe.class.getDeclaredField("theUnsafe");
        unsafeField.setAccessible(true);
        return (Unsafe) unsafeField.get(null);
      } catch (NoSuchFieldException e) {
        e.printStackTrace();
      } catch (IllegalAccessException e) {
        e.printStackTrace();
      }
      return null;
  }
 
}