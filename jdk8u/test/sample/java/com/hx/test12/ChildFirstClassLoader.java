package com.hx.test12;

import java.io.IOException;
import java.net.URL;
import java.util.ArrayList;
import java.util.Enumeration;
import java.util.Iterator;
import java.util.List;
import java.util.function.Consumer;

public final class ChildFirstClassLoader extends FlinkUserCodeClassLoader {
    private final String[] alwaysParentFirstPatterns;
 
 
    public ChildFirstClassLoader(
            URL[] urls,
            ClassLoader parent,
            String[] alwaysParentFirstPatterns,
            Consumer<Throwable> classLoadingExceptionHandler) {
        super(urls, parent, classLoadingExceptionHandler);
        this.alwaysParentFirstPatterns = alwaysParentFirstPatterns;
    }
 
 
    @Override
    protected synchronized Class<?> loadClassWithoutExceptionHandling(
            String name,
            boolean resolve) throws ClassNotFoundException {
        // First, check if the class has already been loaded
        Class<?> c = findLoadedClass(name);
 
 
        if (c == null) {
            // check whether the class should go parent-first
            for (String alwaysParentFirstPattern : alwaysParentFirstPatterns) {
                if (name.startsWith(alwaysParentFirstPattern)) {
                    return super.loadClassWithoutExceptionHandling(name, resolve);
                }
            }
 
 
            try {
                // check the URLs
                c = findClass(name);
            } catch (ClassNotFoundException e) {
                // let URLClassLoader do it, which will eventually call the parent
                c = super.loadClassWithoutExceptionHandling(name, resolve);
            }
        }
 
 
        if (resolve) {
            resolveClass(c);
        }
        return c;
    }
 
 
    @Override
    public URL getResource(String name) {
        // first, try and find it via the URLClassloader
        URL urlClassLoaderResource = findResource(name);
        if (urlClassLoaderResource != null) {
            return urlClassLoaderResource;
        }
        // delegate to super
        return super.getResource(name);
    }
 
 
    @Override
    public Enumeration<URL> getResources(String name) throws IOException {
        // first get resources from URLClassloader
        Enumeration<URL> urlClassLoaderResources = findResources(name);
        final List<URL> result = new ArrayList<>();
 
 
        while (urlClassLoaderResources.hasMoreElements()) {
            result.add(urlClassLoaderResources.nextElement());
        }
 
 
        // get parent urls
        Enumeration<URL> parentResources = getParent().getResources(name);
        while (parentResources.hasMoreElements()) {
            result.add(parentResources.nextElement());
        }
 
 
        return new Enumeration<URL>() {
            Iterator<URL> iter = result.iterator();
 
 
            public boolean hasMoreElements() {
                return iter.hasNext();
            }
 
 
            public URL nextElement() {
                return iter.next();
            }
        };
    }
}