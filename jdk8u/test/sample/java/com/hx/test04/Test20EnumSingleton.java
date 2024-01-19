package com.hx.test04;
 
import com.sun.org.apache.bcel.internal.classfile.ConstantClass;
//import mockit.Mock;
//import mockit.MockUp;
import sun.misc.Unsafe;
import sun.reflect.ConstructorAccessor;
import sun.reflect.Reflection;
 
import java.io.*;
import java.lang.reflect.*;
 
/**
 * DoubleCheckSingleton
 *
 * @author Jerry.X.He <970655147@qq.com>
 * @version 1.0
 * @date 2020-04-01 17:01
 */
public enum Test20EnumSingleton implements Serializable {
 
  INSTANCE;
 
  // disable constructor
  private Test20EnumSingleton() {
//    throw new RuntimeException("can't instantiate !");
    System.out.println(" <init> called ");
  }
 
  // Test18DoubleCheckSingleton
  public static void main(String[] args) throws Exception {
 
    Test20EnumSingleton entity01 = Test20EnumSingleton.getInstance();
 
    // case1 constructor
    // Cannot reflectively create enum objects
    Class<Test20EnumSingleton> clazz = Test20EnumSingleton.class;
    Constructor<Test20EnumSingleton> constructor = clazz.getDeclaredConstructor(String.class, int.class);
 
    Method acquireConstructorAccessorMethod = Constructor.class.getDeclaredMethod("acquireConstructorAccessor");
    acquireConstructorAccessorMethod.setAccessible(true);
    acquireConstructorAccessorMethod.invoke(constructor);
 
    Field constructorAccessorField = Constructor.class.getDeclaredField("constructorAccessor");
    constructorAccessorField.setAccessible(true);
    ConstructorAccessor constructorAccessor = (ConstructorAccessor) constructorAccessorField.get(constructor);
    Test20EnumSingleton entity02 = (Test20EnumSingleton) constructorAccessor.newInstance(new Object[]{"xyz", 2});
//    Test20EnumSingleton entity02 = constructor.newInstance("xyz", 2);
 
    // case2 unsafe
    Unsafe unsafe = Test18DoubleCheckSingleton.getUnsafe();
    Test20EnumSingleton entity03 = (Test20EnumSingleton) unsafe.allocateInstance(Test20EnumSingleton.class);
 
    // case 3 deserialize
    ByteArrayOutputStream baos = new ByteArrayOutputStream();
    ObjectOutputStream oos = new ObjectOutputStream(baos);
    oos.writeObject(entity01);
    byte[] serialized = baos.toByteArray();
    ObjectInputStream ois = new ObjectInputStream(new ByteArrayInputStream(serialized));
    Test20EnumSingleton entity04 = (Test20EnumSingleton) ois.readObject();
 
    int x = 1;
 
  }
 
  /**
   * getInstance
   *
   * @return com.hx.test04.Test18DoubleCheckSingleton
   * @author Jerry.X.He<970655147@qq.com>
   * @date 2020-04-01 17:02
   */
  public static Test20EnumSingleton getInstance() {
    return INSTANCE;
  }
 
}