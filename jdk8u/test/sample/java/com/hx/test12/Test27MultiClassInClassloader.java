package com.hx.test12;

import java.io.File;
import java.net.URL;
import java.util.ArrayList;
import java.util.List;

/**
 * Test27MultiClassInClassloader
 * https://jerryhe.blog.csdn.net/article/details/121887423
 * https://jerryhe.blog.csdn.net/article/details/121888858
 *
 * @author Jerry.X.He <970655147@qq.com>
 * @version 1.0
 * @date 2021-12-12 14:27
 */
public class Test27MultiClassInClassloader {

    // Test27MultiClassInClassloader
    public static void main(String[] args) throws Exception {

        List<Object> refs = new ArrayList<>();
        ClassLoader appClassloader = Test27MultiClassInClassloader.class.getClassLoader();
        URL[] classpath = new URL[]{
                new File("/Users/jerry/IdeaProjects/HelloWorld/target/classes").toURI().toURL()
        };
        String[] alwaysParentPattern = new String[]{};
        for (int i = 0; i < 10; i++) {
            ChildFirstClassLoader classLoader = new ChildFirstClassLoader(classpath, appClassloader, alwaysParentPattern, (e) -> e.printStackTrace());
            Class userClazz = classLoader.loadClass("com.hx.test12.Test27MultiClassInClassloaderUser");
            System.out.println(" the " + i + "th classloader " + Integer.toHexString(userClazz.hashCode()));
            Object userInstance = userClazz.newInstance();
            refs.add(userInstance);
        }

        System.in.read();

    }

}