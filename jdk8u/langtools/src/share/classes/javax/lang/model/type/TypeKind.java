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


/**
 * The kind of a type mirror.
 *
 * <p>Note that it is possible additional type kinds will be added to
 * accommodate new, currently unknown, language structures added to
 * future versions of the Java&trade; programming language.
 *
 * 该类是一个媒介,定义了java中各种类型所对应的枚举.
 * @author Joseph D. Darcy
 * @author Scott Seligman
 * @author Peter von der Ah&eacute;
 * @see TypeMirror
 * @since 1.6
 */
public enum TypeKind {
    /**
     * The primitive type {@code boolean}.
     * boolean的原始类型
     */
    BOOLEAN,

    /**
     * The primitive type {@code byte}.
     * byte的原始类型
     */
    BYTE,

    /**
     * The primitive type {@code short}.
     * short的原始类型
     */
    SHORT,

    /**
     * The primitive type {@code int}.
     * int的原始类型
     */
    INT,

    /**
     * The primitive type {@code long}.
     * long的原始类型
     */
    LONG,

    /**
     * The primitive type {@code char}.
     * char的原始类型
     */
    CHAR,

    /**
     * The primitive type {@code float}.
     * float的原始类型
     */
    FLOAT,

    /**
     * The primitive type {@code double}.
     * double的原始类型
     */
    DOUBLE,

    /**
     * The pseudo-type corresponding to the keyword {@code void}.
     * @see NoType
     * 代表void的伪类型
     */
    VOID,

    /**
     * A pseudo-type used where no actual type is appropriate.
     * @see NoType
     * 代表没有合适的类型与之对应的伪类型
     */
    NONE,

    /**
     * The null type.
     * 代表null的类型
     */
    NULL,

    /**
     * An array type.
     * 数组类型
     */
    ARRAY,

    /**
     * A class or interface type.
     * 代表类或者接口的类型
     */
    DECLARED,

    /**
     * A class or interface type that could not be resolved.
     * 代表无法进行解析的类或接口类型
     */
    ERROR,

    /**
     * A type variable.
     * 类型变量
     * 如一个类声明如下:    C<T,S> ,那么T,S就是类型变量
     */
    TYPEVAR,

    /**
     * A wildcard type argument.
     * 通配符.如:
     * ?,? extends Number,? super T
     */
    WILDCARD,

    /**
     * A pseudo-type corresponding to a package element.
     * @see NoType
     * 代表包的伪类型
     */
    PACKAGE,

    /**
     * A method, constructor, or initializer.
     * 代表方法,构造器,初始代码块
     */
    EXECUTABLE,

    /**
     * An implementation-reserved type.
     * This is not the type you are looking for.
     * 保留类型
     */
    OTHER,

    /**
      * A union type.
      *
      * 联合类型
      * jdk1.7中的 try multi-catch 中异常的参数就是联合类型
      * @since 1.7
      */
    UNION,

    /**
      * An intersection type.
      * 交集类型
      * 如一个类有泛型参数,如<T extends Number & Runnable>,那么T extends Number & Runnable 就是交集类型
      * @since 1.8
      */
    INTERSECTION;

    /**
     * Returns {@code true} if this kind corresponds to a primitive
     * type and {@code false} otherwise.
     * @return {@code true} if this kind corresponds to a primitive type
     * 判断是否是原生类型
     */
    public boolean isPrimitive() {
        switch(this) {
        case BOOLEAN:
        case BYTE:
        case SHORT:
        case INT:
        case LONG:
        case CHAR:
        case FLOAT:
        case DOUBLE:
            return true;

        default:
            return false;
        }
    }
}
