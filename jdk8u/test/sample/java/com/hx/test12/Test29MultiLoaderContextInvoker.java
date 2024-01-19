package com.hx.test12;

import java.io.File;
import java.lang.reflect.Method;
import java.net.URL;
import java.util.function.Consumer;

/**
 * Test29MultiLoaderContextInvoker
 *
 * @author Jerry.X.He <970655147@qq.com>
 * @version 1.0
 * @date 2021-12-19 09:31
 */
public class Test29MultiLoaderContextInvoker {
 
    // Test29MultiLoaderContextInvoker
    // -Xmx10M -XX:+UseSerialGC -XX:+TraceClassLoading
    // -Xmx10M -XX:+UseSerialGC -XX:+TraceClassUnloading
    public static void main(String[] args) throws Exception {
 
        ClassLoader appClassloader = Test29MultiLoaderContextInvoker.class.getClassLoader();
        String[] alwaysParentPatterns = new String[]{};
        URL[] classpathes = new URL[]{
                new File("/Users/jerry/IdeaProjects/HelloWorld/target/classes").toURI().toURL()
        };
        Consumer<Throwable> throwableConsumer = (ex) -> {
            ex.printStackTrace();
        };
 
        int loopCount = 20;
        for (int i = 0; i < loopCount; i++) {
            ChildFirstClassLoader classLoader = new ChildFirstClassLoader(classpathes, appClassloader, alwaysParentPatterns, throwableConsumer);
            Class mainClass = classLoader.loadClass("com.hx.test12.Test29MultiLoaderContextMain");
            Method mainMethod = mainClass.getDeclaredMethod("main", int.class, String[].class);
            mainMethod.invoke(null, i, args);
        }
 
    }
 
}