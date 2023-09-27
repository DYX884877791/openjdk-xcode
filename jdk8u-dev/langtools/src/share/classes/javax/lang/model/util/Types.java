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

package javax.lang.model.util;

import java.lang.annotation.Annotation;
import java.lang.annotation.AnnotationTypeMismatchException;
import java.lang.annotation.IncompleteAnnotationException;
import java.util.List;
import javax.lang.model.element.*;
import javax.lang.model.type.*;

/**
 * Utility methods for operating on types.
 * 操作类型的工具方法的接口
 * <p><b>Compatibility Note:</b> Methods may be added to this interface
 * in future releases of the platform.
 *
 * @author Joseph D. Darcy
 * @author Scott Seligman
 * @author Peter von der Ah&eacute;
 * @see javax.annotation.processing.ProcessingEnvironment#getTypeUtils
 * @since 1.6
 */
public interface Types {

    /**
     * Returns the element corresponding to a type.
     * The type may be a {@code DeclaredType} or {@code TypeVariable}.
     * Returns {@code null} if the type is not one with a
     * corresponding element.
     * 返回与类型对应的元素。类型可以是声明类型或类型变量。如果类型不是具有相应元素的类型，则返回null
     * @param t the type to map to an element
     * @return the element corresponding to the given type
     */
    Element asElement(TypeMirror t);

    /**
     * Tests whether two {@code TypeMirror} objects represent the same type.
     *
     * <p>Caveat: if either of the arguments to this method represents a
     * wildcard, this method will return false.  As a consequence, a wildcard
     * is not the same type as itself.  This might be surprising at first,
     * but makes sense once you consider that an example like this must be
     * rejected by the compiler:
     * <pre>
     *   {@code List<?> list = new ArrayList<Object>();}
     *   {@code list.add(list.get(0));}
     * </pre>
     *
     * <p>Since annotations are only meta-data associated with a type,
     * the set of annotations on either argument is <em>not</em> taken
     * into account when computing whether or not two {@code
     * TypeMirror} objects are the same type. In particular, two
     * {@code TypeMirror} objects can have different annotations and
     * still be considered the same.
     * 测试是否2个对象代表的是同一个类型.
     * 警告：如果此方法的任何一个参数表示通配符，则此方法将返回false。因此，通配符不是与自
     * 身相同的类型。这可能首先令人惊讶，但一旦你认为这样的例子必须被编译器拒绝，才有意义：
     *    List<?> list = new ArrayList<Object>();
     *  list.add(list.get(0));
     *  由于注释仅是与类型相关联的元数据，所以在计算两个TypeMirror对象是否为相同类型时，
     *  不考虑两个参数上的注释集。特别地，两个类型的反射对象可以具有不同的注释，并且仍然被
     *  认为是相同的
     *
     * @param t1  the first type
     * @param t2  the second type
     * @return {@code true} if and only if the two types are the same
     */
    boolean isSameType(TypeMirror t1, TypeMirror t2);

    /**
     * Tests whether one type is a subtype of another.
     * Any type is considered to be a subtype of itself.
     * 测试是否第一个TypeMirror是第二个TypeMirror的子类型.任何类型都被认为是它自身的子类型
     * @param t1  the first type
     * @param t2  the second type
     * @return {@code true} if and only if the first type is a subtype
     *          of the second
     * @throws IllegalArgumentException if given an executable or package type
     * @jls 4.10 Subtyping
     */
    boolean isSubtype(TypeMirror t1, TypeMirror t2);

    /**
     * Tests whether one type is assignable to another.
     * 测试一种类型是否可分配给另一种类型。
     * 这部分的内容可以参考 JLS 5.2 Assignment Conversion
     * @param t1  the first type
     * @param t2  the second type
     * @return {@code true} if and only if the first type is assignable
     *          to the second
     * @throws IllegalArgumentException if given an executable or package type
     * @jls 5.2 Assignment Conversion
     */
    boolean isAssignable(TypeMirror t1, TypeMirror t2);

    /**
     * Tests whether one type argument <i>contains</i> another.
     * 如果第一个参数包含第二个参数则返回true
     * @param t1  the first type
     * @param t2  the second type
     * @return {@code true} if and only if the first type contains the second
     * @throws IllegalArgumentException if given an executable or package type
     * @jls 4.5.1.1 Type Argument Containment and Equivalence
     */
    boolean contains(TypeMirror t1, TypeMirror t2);

    /**
     * Tests whether the signature of one method is a <i>subsignature</i>
     * of another.
     *  测试是否第一个参数是第二个参数的子签名
     * @param m1  the first method
     * @param m2  the second method
     * @return {@code true} if and only if the first signature is a
     *          subsignature of the second
     * @jls 8.4.2 Method Signature
     */
    boolean isSubsignature(ExecutableType m1, ExecutableType m2);

    /**
     * Returns the direct supertypes of a type.  The interface types, if any,
     * will appear last in the list.
     * 返回类型的直接超类型。接口类型（如果有的话）将出现在列表中的最后一个。
     * @param t  the type being examined
     * @return the direct supertypes, or an empty list if none
     * @throws IllegalArgumentException if given an executable or package type
     */
    List<? extends TypeMirror> directSupertypes(TypeMirror t);

    /**
     * Returns the erasure of a type.
     * 返回指定类型擦除后的TypeMirror.
     * @param t  the type to be erased
     * @return the erasure of the given type
     * @throws IllegalArgumentException if given a package type
     * @jls 4.6 Type Erasure
     */
    TypeMirror erasure(TypeMirror t);

    /**
     * Returns the class of a boxed value of a given primitive type.
     * That is, <i>boxing conversion</i> is applied.
     * 返回指定类型的包装类型
     * @param p  the primitive type to be converted
     * @return the class of a boxed value of type {@code p}
     * @jls 5.1.7 Boxing Conversion
     */
    TypeElement boxedClass(PrimitiveType p);

    /**
     * Returns the type (a primitive type) of unboxed values of a given type.
     * That is, <i>unboxing conversion</i> is applied.
     * 返回指定包装类型所对应的原始类型
     * @param t  the type to be unboxed
     * @return the type of an unboxed value of type {@code t}
     * @throws IllegalArgumentException if the given type has no
     *          unboxing conversion
     * @jls 5.1.8 Unboxing Conversion
     */
    PrimitiveType unboxedType(TypeMirror t);

    /**
     * Applies capture conversion to a type.
     * 对指定类型进行捕获转换
     * @param t  the type to be converted
     * @return the result of applying capture conversion
     * @throws IllegalArgumentException if given an executable or package type
     * @jls 5.1.10 Capture Conversion
     */
    TypeMirror capture(TypeMirror t);

    /**
     * Returns a primitive type.
     * 返回指定TypeKind所对应的PrimitiveType
     * @param kind  the kind of primitive type to return
     * @return a primitive type
     * @throws IllegalArgumentException if {@code kind} is not a primitive kind
     */
    PrimitiveType getPrimitiveType(TypeKind kind);

    /**
     * Returns the null type.  This is the type of {@code null}.
     * 返回NullType-->代表null的类型
     * @return the null type
     */
    NullType getNullType();

    /**
     * Returns a pseudo-type used where no actual type is appropriate.
     * The kind of type to return may be either
     * {@link TypeKind#VOID VOID} or {@link TypeKind#NONE NONE}.
     * For packages, use
     * {@link Elements#getPackageElement(CharSequence)}{@code .asType()}
     * instead.
     * 返回没有实际类型时使用的伪类型。返回的类型可以是TypeKind#VOID，也可以是TypeKind#NONE。对于包，使用 Elements.getPackageElement(CharSequence).asType()。
     * @param kind  the kind of type to return
     * @return a pseudo-type of kind {@code VOID} or {@code NONE}
     * @throws IllegalArgumentException if {@code kind} is not valid
     */
    NoType getNoType(TypeKind kind);

    /**
     * Returns an array type with the specified component type.
     * 返回具有指定组件类型的数组类型.
     * @param componentType  the component type
     * @return an array type with the specified component type.
     * @throws IllegalArgumentException if the component type is not valid for
     *          an array
     */
    ArrayType getArrayType(TypeMirror componentType);

    /**
     * Returns a new wildcard type argument.  Either of the wildcard's
     * bounds may be specified, or neither, but not both.
     * 返回一个新的通配符类型参数。通配符的边界可以指定，或者两者都不，但不是两者都有。
     * @param extendsBound  the extends (upper) bound, or {@code null} if none
     * @param superBound    the super (lower) bound, or {@code null} if none
     * @return a new wildcard
     * @throws IllegalArgumentException if bounds are not valid
     */
    WildcardType getWildcardType(TypeMirror extendsBound,
                                 TypeMirror superBound);

    /**
     * Returns the type corresponding to a type element and
     * actual type arguments.
     * Given the type element for {@code Set} and the type mirror
     * for {@code String},
     * for example, this method may be used to get the
     * parameterized type {@code Set<String>}.
     *
     * <p> The number of type arguments must either equal the
     * number of the type element's formal type parameters, or must be
     * zero.  If zero, and if the type element is generic,
     * then the type element's raw type is returned.
     *
     * <p> If a parameterized type is being returned, its type element
     * must not be contained within a generic outer class.
     * The parameterized type {@code Outer<String>.Inner<Number>},
     * for example, may be constructed by first using this
     * method to get the type {@code Outer<String>}, and then invoking
     * {@link #getDeclaredType(DeclaredType, TypeElement, TypeMirror...)}.
     * 返回与类型元素和实际类型参数相对应的类型。例如，给定Set的类型元素和String的TypeMirror
     * ，此方法可用于获得参数化类型Set<String>。
     * 类型参数的数目必须等于TypeElement的形式类型参数的数目，或者必须为零。如果为零，如
     * 果类型元素是泛型，则返回类型元素的原始类型
     * 如果正在返回参数化类型，则其类型元素不能包含在泛型外部类中。例如，参数化类型
     * Outer<String>.Inner<Number>，可以通过首先使用此方法获得类型Outer<String>，然
     * 后调用getDeclaredType(DeclaredType、TypeElement、TypeMirror)来构造。
     *
     * @param typeElem  the type element
     * @param typeArgs  the actual type arguments
     * @return the type corresponding to the type element and
     *          actual type arguments
     * @throws IllegalArgumentException if too many or too few
     *          type arguments are given, or if an inappropriate type
     *          argument or type element is provided
     */
    DeclaredType getDeclaredType(TypeElement typeElem, TypeMirror... typeArgs);

    /**
     * Returns the type corresponding to a type element
     * and actual type arguments, given a
     * {@linkplain DeclaredType#getEnclosingType() containing type}
     * of which it is a member.
     * The parameterized type {@code Outer<String>.Inner<Number>},
     * for example, may be constructed by first using
     * {@link #getDeclaredType(TypeElement, TypeMirror...)}
     * to get the type {@code Outer<String>}, and then invoking
     * this method.
     *
     * <p> If the containing type is a parameterized type,
     * the number of type arguments must equal the
     * number of {@code typeElem}'s formal type parameters.
     * If it is not parameterized or if it is {@code null}, this method is
     * equivalent to {@code getDeclaredType(typeElem, typeArgs)}.
     * 返回与类型元素和实际类型参数相对应的类型，给定包含成员类型的成员类型。例如，可以通过
     * 首先使用getDeclaredType(TypeElement，TypeMirror)获得类型Outer<String>，然
     * 后调用此方法来构造参数化类型Outer<String>.Inner<Number>。
     * 如果包含的类型是参数化类型，则类型参数的数量必须等于typeElem的形式类型参数的数量。
     * 如果它不是参数化的，或者它是空的，那么这个方法就等同于getDeclaredType(typeElem, typeArgs)
     * @param containing  the containing type, or {@code null} if none
     * @param typeElem    the type element
     * @param typeArgs    the actual type arguments
     * @return the type corresponding to the type element and
     *          actual type arguments, contained within the given type
     * @throws IllegalArgumentException if too many or too few
     *          type arguments are given, or if an inappropriate type
     *          argument, type element, or containing type is provided
     */
    DeclaredType getDeclaredType(DeclaredType containing,
                                 TypeElement typeElem, TypeMirror... typeArgs);

    /**
     * Returns the type of an element when that element is viewed as
     * a member of, or otherwise directly contained by, a given type.
     * For example,
     * when viewed as a member of the parameterized type {@code Set<String>},
     * the {@code Set.add} method is an {@code ExecutableType}
     * whose parameter is of type {@code String}.
     * 当元素被视为给定类型的成员或直接包含时，返回元素的类型。
     * 当第一个参数是Set<String>,第二个参数是Set.add方法时,则返回一个参数为String的ExecutableType
     * @param containing  the containing type
     * @param element     the element
     * @return the type of the element as viewed from the containing type
     * @throws IllegalArgumentException if the element is not a valid one
     *          for the given type
     */
    TypeMirror asMemberOf(DeclaredType containing, Element element);
}
