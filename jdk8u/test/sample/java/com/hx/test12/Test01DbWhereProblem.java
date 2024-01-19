package com.hx.test12;

/**
 * Test22DbWhereProblem
 * https://jerryhe.blog.csdn.net/article/details/119294967
 *
 * @author Jerry.X.He <970655147@qq.com>
 * @version 1.0
 * @date 2021-07-19 10:52
 */
public class Test01DbWhereProblem {

    // Test22DbWhereProblem
//    遇到一个没想明白的问题。
//    一个类的静态字段是当前类的子类，多线程同时实例化该类和子类时，线程卡住。
//    https://www.yuque.com/buildup/java/tzemks
    // ThreadA 组线程, 获取了 DbWhere 的锁, 尝试获取 EmptiableWhere 的锁
    // ThreadB 获取了 EmptiableWhere 的锁, 尝试获取 DbWhere 的锁, 导致死锁
    public static void main(String[] args) {

        System.out.println("DbWhereTest4");
        for (int i = 0; i < 3; i++) {
            new Thread(() -> {
                new DbWhere();
            }, "ThreadA").start();
            new Thread(() -> {
                new EmptiableWhere();
            }, "ThreadB").start();
        }

    }

    private static class DbWhere {
        /** 没有查询条件的空Where **/
        public static final EmptiableWhere NONE = new EmptiableWhere();

        public DbWhere() {
            log(this.getClass(), "DbWhere<init>()");
        }

        static void log(Class<?> clazz, String msg) {
            System.out.println(Thread.currentThread().getName() + ' ' + clazz.getSimpleName() + '-' + msg);
        }
    }

    /** 允许为空的查询条件 **/
    private static class EmptiableWhere extends DbWhere {
        public EmptiableWhere() {
            log(this.getClass(), "EmptiableWhere<init>()");
        }
    }

}