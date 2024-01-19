package com.hx.test12;

import java.net.URL;
import java.net.URLClassLoader;
import java.util.function.Consumer;

/**
 * https://blog.csdn.net/weixin_44904816/article/details/108250776?ops_request_misc=&request_id=&biz_id=102&utm_term=ChildFirstClassLoader&utm_medium=distribute.pc_search_result.none-task-blog-2~blog~sobaiduweb~default-2-108250776.nonecase&spm=1018.2226.3001.4450
 */
public abstract class FlinkUserCodeClassLoader extends URLClassLoader {
    public static final Consumer<Throwable> NOOP_EXCEPTION_HANDLER = classLoadingException -> {};
 
 
    private final Consumer<Throwable> classLoadingExceptionHandler;
 
 
    protected FlinkUserCodeClassLoader(URL[] urls, ClassLoader parent) {
        this(urls, parent, NOOP_EXCEPTION_HANDLER);
    }
 
 
    protected FlinkUserCodeClassLoader(
            URL[] urls,
            ClassLoader parent,
            Consumer<Throwable> classLoadingExceptionHandler) {
        super(urls, parent);
        this.classLoadingExceptionHandler = classLoadingExceptionHandler;
    }
 
 
    @Override
    protected final Class<?> loadClass(String name, boolean resolve) throws ClassNotFoundException {
        try {
            return loadClassWithoutExceptionHandling(name, resolve);
        } catch (Throwable classLoadingException) {
            classLoadingExceptionHandler.accept(classLoadingException);
            throw classLoadingException;
        }
    }
 
 
    protected Class<?> loadClassWithoutExceptionHandling(String name, boolean resolve) throws ClassNotFoundException {
        return super.loadClass(name, resolve);
    }
}