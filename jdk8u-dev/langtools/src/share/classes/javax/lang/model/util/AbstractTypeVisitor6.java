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

import javax.lang.model.type.*;

/**
 * A skeletal visitor of types with default behavior appropriate for
 * the {@link javax.lang.model.SourceVersion#RELEASE_6 RELEASE_6}
 * source version.
 *
 * <p> <b>WARNING:</b> The {@code TypeVisitor} interface implemented
 * by this class may have methods added to it in the future to
 * accommodate new, currently unknown, language structures added to
 * future versions of the Java&trade; programming language.
 * Therefore, methods whose names begin with {@code "visit"} may be
 * added to this class in the future; to avoid incompatibilities,
 * classes which extend this class should not declare any instance
 * methods with names beginning with {@code "visit"}.
 *
 * <p>When such a new visit method is added, the default
 * implementation in this class will be to call the {@link
 * #visitUnknown visitUnknown} method.  A new abstract type visitor
 * class will also be introduced to correspond to the new language
 * level; this visitor will have different default behavior for the
 * visit method in question.  When the new visitor is introduced, all
 * or portions of this visitor may be deprecated.
 *
 * <p>Note that adding a default implementation of a new visit method
 * in a visitor class will occur instead of adding a <em>default
 * method</em> directly in the visitor interface since a Java SE 8
 * language feature cannot be used to this version of the API since
 * this version is required to be runnable on Java SE 7
 * implementations.  Future versions of the API that are only required
 * to run on Java SE 8 and later may take advantage of default methods
 * in this situation.
 * 具有默认行为的类型的访问者，适用于jdk 1.6.
 * 警告：该类实现的Type EvestIdor接口可能会在将来添加一些方法，以适应添加到Java编程语言未来版本的新的、目前未知的语言结构。因此，名称以“visit”开头的方法将来可能会被添加到这个类中；为了避免不兼容，扩展这个类的类不应声明任何以“visit”开头的实例方法。
 *
 * 当添加了这种新的访问方法时，此类中的默认实现将调用visitUnknown方法。还将引入一个新的抽象类型访问者类来对应新版本的语言；该访问者中的访问方法具有不同的默认行为。当新访问者被引入时，该类中的一部分或者全部都将被废弃.
 *
 * 注意，在访问者界面中添加一个新的访问方法的默认实现，而不是直接在访问者界面中添加一个默认方法，因为Java SE 8语言特征不能用于这个版本的API，因为这个版本需要在Java SE 7上运行。仅在Java SE 8和以后运行的API的未来版本可能会在这种情况下利用默认方法。
 *
 * 泛型参数 R –> 该visit方法的返回值类型
 * 泛型参数 P –> visit方法额外添加的参数
 *
 * @param <R> the return type of this visitor's methods.  Use {@link
 *            Void} for visitors that do not need to return results.
 * @param <P> the type of the additional parameter to this visitor's
 *            methods.  Use {@code Void} for visitors that do not need an
 *            additional parameter.
 *
 * @author Joseph D. Darcy
 * @author Scott Seligman
 * @author Peter von der Ah&eacute;
 *
 * @see AbstractTypeVisitor7
 * @see AbstractTypeVisitor8
 * @since 1.6
 */
public abstract class AbstractTypeVisitor6<R, P> implements TypeVisitor<R, P> {
    /**
     * Constructor for concrete subclasses to call.
     */
    protected AbstractTypeVisitor6() {}

    /**
     * Visits any type mirror as if by passing itself to that type
     * mirror's {@link TypeMirror#accept accept} method.  The
     * invocation {@code v.visit(t, p)} is equivalent to {@code
     * t.accept(v, p)}.
     *
     * @param t  the type to visit
     * @param p  a visitor-specified parameter
     * @return a visitor-specified result
     */
    public final R visit(TypeMirror t, P p) {
        return t.accept(this, p);
    }

    /**
     * Visits any type mirror as if by passing itself to that type
     * mirror's {@link TypeMirror#accept accept} method and passing
     * {@code null} for the additional parameter.  The invocation
     * {@code v.visit(t)} is equivalent to {@code t.accept(v, null)}.
     *
     * @param t  the type to visit
     * @return a visitor-specified result
     */
    public final R visit(TypeMirror t) {
        return t.accept(this, null);
    }

    /**
     * Visits a {@code UnionType} element by calling {@code
     * visitUnknown}.

     * @param t  {@inheritDoc}
     * @param p  {@inheritDoc}
     * @return the result of {@code visitUnknown}
     *
     * @since 1.7
     */
    public R visitUnion(UnionType t, P p) {
        return visitUnknown(t, p);
    }

    /**
     * Visits an {@code IntersectionType} element by calling {@code
     * visitUnknown}.

     * @param t  {@inheritDoc}
     * @param p  {@inheritDoc}
     * @return the result of {@code visitUnknown}
     *
     * @since 1.8
     */
    public R visitIntersection(IntersectionType t, P p) {
        return visitUnknown(t, p);
    }

    /**
     * {@inheritDoc}
     *
     * <p> The default implementation of this method in {@code
     * AbstractTypeVisitor6} will always throw {@code
     * UnknownTypeException}.  This behavior is not required of a
     * subclass.
     *
     * @param t  the type to visit
     * @return a visitor-specified result
     * @throws UnknownTypeException
     *  a visitor implementation may optionally throw this exception
     */
    public R visitUnknown(TypeMirror t, P p) {
        throw new UnknownTypeException(t, p);
    }
}
