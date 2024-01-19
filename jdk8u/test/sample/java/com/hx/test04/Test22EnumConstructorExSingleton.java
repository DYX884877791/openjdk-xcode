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
public enum Test22EnumConstructorExSingleton implements Serializable {
 
  INSTANCE;
 
  // disable constructor
  private Test22EnumConstructorExSingleton() {
    synchronized (BeanCreateFlagHolder.class) {
      if(BeanCreateFlagHolder.INSTANCE_CREATED) {
        throw new RuntimeException("can't instantiate !");
      }
 
      BeanCreateFlagHolder.INSTANCE_CREATED = true;
    }
  }
 
  // Test18DoubleCheckSingleton
  public static void main(String[] args) throws Exception {
 
    Test22EnumConstructorExSingleton entity01 = Test22EnumConstructorExSingleton.getInstance();
 
    // case1 constructor
    // Cannot reflectively create enum objects
//    Class<Test22EnumConstructorExSingleton> clazz = Test22EnumConstructorExSingleton.class;
//    Constructor<Test22EnumConstructorExSingleton> constructor = clazz.getDeclaredConstructor(String.class, int.class);
//
//    Method acquireConstructorAccessorMethod = Constructor.class.getDeclaredMethod("acquireConstructorAccessor");
//    acquireConstructorAccessorMethod.setAccessible(true);
//    acquireConstructorAccessorMethod.invoke(constructor);
//
//    Field constructorAccessorField = Constructor.class.getDeclaredField("constructorAccessor");
//    constructorAccessorField.setAccessible(true);
//    ConstructorAccessor constructorAccessor = (ConstructorAccessor) constructorAccessorField.get(constructor);
//    Test22EnumConstructorExSingleton entity02 = (Test22EnumConstructorExSingleton) constructorAccessor.newInstance(new Object[]{"xyz", 2});
//    Test20EnumSingleton entity02 = constructor.newInstance("xyz", 2);
 
    // case2 unsafe
    Unsafe unsafe = Test18DoubleCheckSingleton.getUnsafe();
    Test22EnumConstructorExSingleton entity03 = (Test22EnumConstructorExSingleton) unsafe.allocateInstance(Test22EnumConstructorExSingleton.class);
 
    // case 3 deserialize
    ByteArrayOutputStream baos = new ByteArrayOutputStream();
    ObjectOutputStream oos = new ObjectOutputStream(baos);
    oos.writeObject(entity01);
    byte[] serialized = baos.toByteArray();
    ObjectInputStream ois = new ObjectInputStream(new ByteArrayInputStream(serialized));
    Test22EnumConstructorExSingleton entity04 = (Test22EnumConstructorExSingleton) ois.readObject();
 
    int x = 1;
 
  }
 
  /**
   * getInstance
   *
   * @return com.hx.test04.Test18DoubleCheckSingleton
   * @author Jerry.X.He<970655147@qq.com>
   * @date 2020-04-01 17:02
   */
  public static Test22EnumConstructorExSingleton getInstance() {
    return INSTANCE;
  }
 
  /**
   * BeanCareatedHodler
   *
   * @author Jerry.X.He <970655147@qq.com>
   * @version 1.0
   * @date 2020-04-01 18:38
   */
  static class BeanCreateFlagHolder {
    // INSTANCE_CREATED
    public static boolean INSTANCE_CREATED = false;
  }
 
}