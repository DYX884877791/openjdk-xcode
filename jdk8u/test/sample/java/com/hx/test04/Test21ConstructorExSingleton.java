package com.hx.test04;
 
import sun.misc.Unsafe;
 
import java.io.*;
 
/**
 * DoubleCheckSingleton
 *
 * @author Jerry.X.He <970655147@qq.com>
 * @version 1.0
 * @date 2020-04-01 17:01
 */
public class Test21ConstructorExSingleton extends Test21ConstructorExSingletonParent implements Serializable {
 
  // INSTANCE
  private static volatile Test21ConstructorExSingleton INSTANCE;
 
  // disable constructor
  private Test21ConstructorExSingleton() {
 
  }
 
  // Test18DoubleCheckSingleton
  public static void main(String[] args) throws Exception {
 
    Test21ConstructorExSingleton entity01 = Test21ConstructorExSingleton.getInstance();
 
    // case1 constructor
//    Class<Test21ConstructorExSingleton> clazz = Test21ConstructorExSingleton.class;
//    Constructor<Test21ConstructorExSingleton> constructor = clazz.getDeclaredConstructor();
//    Test21ConstructorExSingleton entity02 = constructor.newInstance();
 
    // case2 unsafe
    Unsafe unsafe = Test18DoubleCheckSingleton.getUnsafe();
    Test21ConstructorExSingleton entity03 = (Test21ConstructorExSingleton) unsafe.allocateInstance(Test21ConstructorExSingleton.class);
 
    // case 3 deserialize
//    ByteArrayOutputStream baos = new ByteArrayOutputStream();
//    ObjectOutputStream oos = new ObjectOutputStream(baos);
//    oos.writeObject(entity01);
//    byte[] serialized = baos.toByteArray();
//    ObjectInputStream ois = new ObjectInputStream(new ByteArrayInputStream(serialized));
//    Test21ConstructorExSingleton entity04 = (Test21ConstructorExSingleton) ois.readObject();
 
    int x = 1;
 
  }
 
  /**
   * getInstance
   *
   * @return com.hx.test04.Test18DoubleCheckSingleton
   * @author Jerry.X.He<970655147@qq.com>
   * @date 2020-04-01 17:02
   */
  public static Test21ConstructorExSingleton getInstance() {
    if(INSTANCE == null) {
      synchronized (Test21ConstructorExSingleton.class) {
        if(INSTANCE == null) {
          INSTANCE = new Test21ConstructorExSingleton();
        }
      }
    }
 
    return INSTANCE;
  }
 
}
 
/**
 * Test21ConstructorExSingletonParent
 *
 * @author Jerry.X.He <970655147@qq.com>
 * @version 1.0
 * @date 2020-04-01 19:18
 */
class Test21ConstructorExSingletonParent {
 
  // INSTANCE_CREATED
  private static volatile boolean INSTANCE_CREATED = false;
 
  public Test21ConstructorExSingletonParent() {
    synchronized (Test21ConstructorExSingletonParent.class) {
      if (INSTANCE_CREATED) {
        throw new RuntimeException("can't instantiate !");
      }
 
      INSTANCE_CREATED = true;
    }
  }
 
}