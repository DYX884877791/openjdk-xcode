package com.hx.test11;

import java.io.File;
import java.lang.reflect.Method;
import java.net.URL;
import java.net.URLClassLoader;
import java.util.List;
import java.util.Vector;

/**
 * Test30BigIntArray
 * https://jerryhe.blog.csdn.net/article/details/108023658
 *
 * @author Jerry.X.He <970655147@qq.com>
 * @version 1.0
 * @date 2020/8/12 21:58
 */
public class Test30BigIntArray {

    // identStr
    private String identStr = "identStr";
    // mCache
    final static List<Method> mCache = new Vector<Method>();

    public static void main(String[] args) throws Throwable {
        final File f = new File(".\\target\\classes");
        int i = 0;
        while (true) {
            new Thread(new Runnable() {
                @Override
                public void run() {
                    try {
                        ClassLoader1 classLoader1 = new ClassLoader1(new URL[]{f.toURI().toURL()});
                        ClassLoader2 classLoader2 = new ClassLoader2(new URL[]{f.toURI().toURL()});
                        ClassLoader3 classLoader3 = new ClassLoader3(new URL[]{f.toURI().toURL()});
                        final Class<?> model1Clz = classLoader1.loadClass("com.hx.test11.Test30BigIntArray$TbmkModel1");
                        final Class<?> model2Clz = classLoader2.loadClass("com.hx.test11.Test30BigIntArray$TbmkModel2");
                        final Class<?> model3Clz = classLoader3.loadClass("com.hx.test11.Test30BigIntArray$TbmkModel3");
                        final TbmkModel1 model1 = new TbmkModel1();
                        final TbmkModel2 model2 = new TbmkModel2();
                        final TbmkModel3 model3 = new TbmkModel3();

                        for (int i = 0; i < 1000; ++i) {
                            int methodIdx = i % 10;
                            Method m = model1Clz.getMethod("method" + methodIdx);
                            m.invoke(model1);
                            mCache.add(m);
                            m = model2Clz.getMethod("method" + methodIdx);
                            m.invoke(model2);
                            mCache.add(m);
                            m = model3Clz.getMethod("method" + methodIdx);
                            m.invoke(model3);
                            mCache.add(m);
                        }
                        System.out.println("mCache size: " + mCache.size());
                        Thread.sleep(100);
                    } catch (Exception e) {
                        e.printStackTrace();
                    }
                }
            }
            ).start();

            Thread.sleep(10);
            i++;
            System.out.println(" i = " + i);
            if (i == 3) {
                Thread.sleep(2000_000);
            }
        }
    }

    /**
     * ClassLoader1
     *
     * @author Jerry.X.He <970655147@qq.com>
     * @version 1.0
     * @date 2020/8/12 22:00
     */
    static class ClassLoader1 extends URLClassLoader {
        public ClassLoader1(URL[] urls) {
            super(urls);
        }
    }

    static class ClassLoader2 extends URLClassLoader {
        public ClassLoader2(URL[] urls) {
            super(urls);
        }
    }

    static class ClassLoader3 extends URLClassLoader {
        public ClassLoader3(URL[] urls) {
            super(urls);
        }
    }

    /**
     * TbmkModel1
     *
     * @author Jerry.X.He <970655147@qq.com>
     * @version 1.0
     * @date 2020/8/12 22:02
     */
    static class TbmkModel1 {
        public void method0() {

        }

        public void method1() {

        }

        public void method2() {

        }

        public void method3() {

        }

        public void method4() {

        }

        public void method5() {

        }

        public void method6() {

        }

        public void method7() {

        }

        public void method8() {

        }

        public void method9() {

        }
    }

    static class TbmkModel2 {
        public void method0() {

        }

        public void method1() {

        }

        public void method2() {

        }

        public void method3() {

        }

        public void method4() {

        }

        public void method5() {

        }

        public void method6() {

        }

        public void method7() {

        }

        public void method8() {

        }

        public void method9() {

        }
    }

    static class TbmkModel3 {
        public void method0() {

        }

        public void method1() {

        }

        public void method2() {

        }

        public void method3() {

        }

        public void method4() {

        }

        public void method5() {

        }

        public void method6() {

        }

        public void method7() {

        }

        public void method8() {

        }

        public void method9() {

        }
    }


}