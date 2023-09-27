/*
 * Copyright (c) 2005, 2013, Oracle and/or its affiliates. All rights reserved.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 * This code is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 only, as
 * published by the Free Software Foundation.  Oracle designates this
 * particular file as subject to the "Classpath" exception as provided
 * by Oracle in the LICENSE file that accompanied this code.
 *
 * This code is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * version 2 for more details (a copy is included in the LICENSE file that
 * accompanied this code).
 *
 * You should have received a copy of the GNU General Public License version
 * 2 along with this work; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA.
 *
 * Please contact Oracle, 500 Oracle Parkway, Redwood Shores, CA 94065 USA
 * or visit www.oracle.com if you need additional information or have any
 * questions.
 */

package javax.lang.model.type;


import java.util.List;

import javax.lang.model.element.ExecutableElement;

/**
 * Represents the type of an executable.  An <i>executable</i>
 * is a method, constructor, or initializer.
 *
 * <p> The executable is
 * represented as when viewed as a method (or constructor or
 * initializer) of some reference type.
 * If that reference type is parameterized, then its actual
 * type arguments are substituted into any types returned by the methods of
 * this interface.
 * 代表可执行的类型,如方法,构造器,初始代码块.可执行文件表示为某种引用类型的方法（或构造函数或初始化器）。如果引用类型是参数化的，那么它的实际类型参数被替换为该接口的方法返回的任何类型.
 *
 * @author Joseph D. Darcy
 * @author Scott Seligman
 * @author Peter von der Ah&eacute;
 * @see ExecutableElement
 * @since 1.6
 */
public interface ExecutableType extends TypeMirror {

    /**
     * Returns the type variables declared by the formal type parameters
     * of this executable.
     * 返回该类型的类型变量.
     * @return the type variables declared by the formal type parameters,
     *          or an empty list if there are none 返回该类型的类型变量,或者空集合,如果该类型不是泛型的
     */
    List<? extends TypeVariable> getTypeVariables();

    /**
     * Returns the return type of this executable.
     * Returns a {@link NoType} with kind {@link TypeKind#VOID VOID}
     * if this executable is not a method, or is a method that does not
     * return a value.
     * 返回该类型的返回类型。当该类型不是方法,或者该方法没有返回值时，则该方法返回的是NoType和TypeKind#VOID构成的TypeMirror
     * @return the return type of this executable
     */
    TypeMirror getReturnType();

    /**
     * Returns the types of this executable's formal parameters.
     * 返回该类型的形式参数,如果没有的话,返回空集合
     * @return the types of this executable's formal parameters,
     *          or an empty list if there are none
     */
    List<? extends TypeMirror> getParameterTypes();

    /**
     * Returns the receiver type of this executable,
     * or {@link javax.lang.model.type.NoType NoType} with
     * kind {@link javax.lang.model.type.TypeKind#NONE NONE}
     * if the executable has no receiver type.
     *  返回该类型的接收器类型,如果该类型没有接收器,则返回NoType,TypeKind#NONE 构成的TypeMirror
     *
     * An executable which is an instance method, or a constructor of an
     * inner class, has a receiver type derived from the {@linkplain
     * ExecutableElement#getEnclosingElement declaring type}.
     * 一个实例方法,或者是内部类的构造器,有接收器类型。可以从xecutableElement#getEnclosingElement 获得
     *
     * An executable which is a static method, or a constructor of a
     * non-inner class, or an initializer (static or instance), has no
     * receiver type.
     * 静态方法,不是内部类的构造器,或者是初始代码块(静态或者实例的),没有接收器类型
     *
     * @return the receiver type of this executable
     * @since 1.8
     */
    TypeMirror getReceiverType();

    /**
     * Returns the exceptions and other throwables listed in this
     * executable's {@code throws} clause.
     * 返回该类型的异常声明列表
     *
     * @return the exceptions and other throwables listed in this
     *          executable's {@code throws} clause,
     *          or an empty list if there are none.
     */
    List<? extends TypeMirror> getThrownTypes();
}
