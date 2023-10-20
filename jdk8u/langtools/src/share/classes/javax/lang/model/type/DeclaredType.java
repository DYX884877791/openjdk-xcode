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

import javax.lang.model.element.Element;
import javax.lang.model.element.TypeElement;
import javax.lang.model.util.Types;


/**
 * Represents a declared type, either a class type or an interface type.
 * This includes parameterized types such as {@code java.util.Set<String>}
 * as well as raw types.
 * 代表声明类型–> 类或者接口类型. 它也可以拥有泛型参数,如 java.util.Set
 *
 * <p> While a {@code TypeElement} represents a class or interface
 * <i>element</i>, a {@code DeclaredType} represents a class
 * or interface <i>type</i>, the latter being a use
 * (or <i>invocation</i>) of the former.
 * See {@link TypeElement} for more on this distinction.
 *
 * <p> The supertypes (both class and interface types) of a declared
 * type may be found using the {@link
 * Types#directSupertypes(TypeMirror)} method.  This returns the
 * supertypes with any type arguments substituted in.
 *
 * @author Joseph D. Darcy
 * @author Scott Seligman
 * @author Peter von der Ah&eacute;
 * @see TypeElement
 * @since 1.6
 */
public interface DeclaredType extends ReferenceType {

    /**
     * Returns the element corresponding to this type.
     * 返回该类型对应的element
     * @return the element corresponding to this type
     */
    Element asElement();

    /**
     * Returns the type of the innermost enclosing instance or a
     * {@code NoType} of kind {@code NONE} if there is no enclosing
     * instance.  Only types corresponding to inner classes have an
     * enclosing instance.
     * 返回最内层包围实例的类型,如果没有包围实例则返回NoType和NONE对应的TypeMirror.
     * 只有对应于内部类的类型才有包围实例
     *
     * @return a type mirror for the enclosing type
     * @jls 8.1.3 Inner Classes and Enclosing Instances
     * @jls 15.9.2 Determining Enclosing Instances
     */
    TypeMirror getEnclosingType();

    /**
     * Returns the actual type arguments of this type.
     * For a type nested within a parameterized type
     * (such as {@code Outer<String>.Inner<Number>}), only the type
     * arguments of the innermost type are included.
     * 返回该类型的泛型参数
     *
     * @return the actual type arguments of this type, or an empty list
     *           if none
     */
    List<? extends TypeMirror> getTypeArguments();
}
