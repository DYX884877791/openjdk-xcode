package com.hx.test;

import java.lang.reflect.Method;
import java.net.URL;
import java.net.URLClassLoader;

/**
 * Test28UrlClassLoader
 * https://jerryhe.blog.csdn.net/article/details/117387015
 *
 * @author Jerry.X.He <970655147@qq.com>
 * @version 1.0
 * @date 2021-05-29 10:26
 */
public class Test28UrlClassLoader {

    // Test28UrlClassLoader
    public static void main(String[] args) throws Exception {

        ClassLoader classLoader = new URLClassLoader(new URL[]{
                new URL("file:///Users/jerry/.m2/repository/asm/asm/3.3.1/asm-3.3.1.jar"),
                new URL("file:///Users/jerry/.m2/repository/org/ow2/asm/asm/5.0.4/asm-5.0.4.jar")
        });

        Class classWriterClazz = classLoader.loadClass("org.objectweb.asm.ClassWriter");
        Method method = classWriterClazz.getMethod("visit", int.class, int.class, String.class, String.class,
                                                   String.class, String[].class);
        System.out.println(method.toString());

    }

}