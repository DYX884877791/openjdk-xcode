package com.hx.test12;


import java.net.URL;
import java.util.function.Consumer;

public class ParentFirstClassLoader extends FlinkUserCodeClassLoader {
    ParentFirstClassLoader(URL[] urls, ClassLoader parent, Consumer<Throwable> classLoadingExceptionHandler) {
        super(urls, parent, classLoadingExceptionHandler);
    }
}