package com.hx.test03;

/**
 * HSDBInspectClass
 * https://jerryhe.blog.csdn.net/article/details/106590392
 *
 * @author Jerry.X.He <970655147@qq.com>
 * @version 1.0
 * @date 2020-02-01 16:30
 */
public class Test16HSDBInspectClass {

  // static instanceClass & instanceClass
  private static InstanceClass staticInstance = new InstanceClass("staticInstance");
  private InstanceClass instance = new InstanceClass("instance");

//  三个 Test16HSDBInspectClass$InstanceClass 实例
//  0x000000079591fd38	Oop for com/hx/test03/Test16HSDBInspectClass$InstanceClass	InstanceKlass for com/hx/test03/Test16HSDBInspectClass$InstanceClass
//  0x000000079591fdb0	Oop for com/hx/test03/Test16HSDBInspectClass$InstanceClass	InstanceKlass for com/hx/test03/Test16HSDBInspectClass$InstanceClass
//  0x000000079591fdf8	Oop for com/hx/test03/Test16HSDBInspectClass$InstanceClass	InstanceKlass for com/hx/test03/Test16HSDBInspectClass$InstanceClass

  // Test16HSDBInspectClass
//  hsdb> revptrs 0x000000079591fd38
//  Computing reverse pointers...
//  Done.
//null
//  Oop for java/lang/Class @ 0x00000007959170d0
//  hsdb> revptrs 0x000000079591fdb0
//  Oop for com/hx/test03/Test16HSDBInspectClass @ 0x000000079591fda0
//  hsdb> revptrs 0x000000079591fdf8
//          null
//          null
//  hsdb>
  // 可以找到 静态变量 和 局部变量, revptrs 找不到 栈帧上面这一个
  // 但是可以从 main 方法的栈帧信息里面看到对应的 引用
//  hsdb> mem 0x000070000225f908
//          0x000070000225f908: 0x000000079591fdf8
  // 三个实例的三个引用, 一个在 instanceKlass 的 java_mirror 里面, 另外一个在新建的实例 thisInstance 里面, 一个在 栈帧上面
  public static void main(String[] args) {

    foo();

  }

  // local instanceClass
  public static void foo() {
    Test16HSDBInspectClass thisInstance = new Test16HSDBInspectClass();
    InstanceClass localInstance = new InstanceClass("localInstance");
    System.out.println(" break point ");
  }

  /**
   * InstanceClass
   *
   * @author Jerry.X.He <970655147@qq.com>
   * @version 1.0
   * @date 2020-02-01 16:33
   */
  private static class InstanceClass {
    private String str;
    public InstanceClass(String str) {
      this.str = str;
    }
  }

}