/**
 * 用HSDB分析Person类的运行时数据
 */

import java.util.UUID;

public class Person {
    // 静态常量（常量池中不存在恒定值引用）
    private final static String finalStr1= UUID.randomUUID().toString();

    // 静态常量（常量池中存在恒定值引用）
    private final static String finalStr2="final String";

    // String类型静态变量
    private static String staticStr="static String";

    // int类型静态变量
    private static int staticInt=1;

    // String成员变量
    private String a="String";

    // int类型成员变量
    private int b=2;

    /******************get/set方法****************************/
    public static String getFinalStr1() {
        return finalStr1;
    }

    public static String getFinalStr2() {
        return finalStr2;
    }

    public static String getStaticStr() {
        return staticStr;
    }

    public static void setStaticStr(String staticStr) {
        Person.staticStr = staticStr;
    }

    public static int getStaticInt() {
        return staticInt;
    }

    public static void setStaticInt(int staticInt) {
        Person.staticInt = staticInt;
    }

    public String getA() {
        return a;
    }

    public void setA(String a) {
        this.a = a;
    }

    public int getB() {
        return b;
    }

    public void setB(int b) {
        this.b = b;
    }
}
