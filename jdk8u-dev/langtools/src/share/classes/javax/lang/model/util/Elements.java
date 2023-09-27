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


import java.util.List;
import java.util.Map;

import javax.lang.model.element.*;
import javax.lang.model.type.*;


/**
 * Utility methods for operating on program elements.
 * 操作java源程序元素的工具方法.
 * <p><b>Compatibility Note:</b> Methods may be added to this interface
 * in future releases of the platform.
 *
 * @author Joseph D. Darcy
 * @author Scott Seligman
 * @author Peter von der Ah&eacute;
 * @see javax.annotation.processing.ProcessingEnvironment#getElementUtils
 * @since 1.6
 */
public interface Elements {

    /**
     * Returns a package given its fully qualified name.
     * 返回指定全限定名所对应的PackageElement.如果指定的是"",则返回未命名包.如果没有找到对应的,则返回null.
     * @param name  fully qualified package name, or "" for an unnamed package
     * @return the named package, or {@code null} if it cannot be found
     */
    PackageElement getPackageElement(CharSequence name);

    /**
     * Returns a type element given its canonical name.
     * 返回指定规范名称对应的TypeElement,如果没有对应的,则返回null.
     * @param name  the canonical name
     * @return the named type element, or {@code null} if it cannot be found
     */
    TypeElement getTypeElement(CharSequence name);

    /**
     * Returns the values of an annotation's elements, including defaults.
     * 返回该注解的元素所对应的值,包括默认值
     * @see AnnotationMirror#getElementValues()
     * @param a  annotation to examine
     * @return the values of the annotation's elements, including defaults
     */
    Map<? extends ExecutableElement, ? extends AnnotationValue>
            getElementValuesWithDefaults(AnnotationMirror a);

    /**
     * Returns the text of the documentation (&quot;Javadoc&quot;)
     * comment of an element.
     * 返回该元素所对应的Javadoc.
     * <pre>
     * 该元素对应的文档注释是有{@code \/**}开头的,以{@code *\/}结尾的,忽略空格。因此，文档注释包含至少三
     * 个{@code *}字符。文档注释返回的文本是在源代码中出现的注释的处理形式.开头的{@code \/**}和结尾
     * 的{@code *\/}被删除.对于在初始{@code \/**}之后开始的注释行，前导空白字符将被丢弃，而在空白之后
     * 或开始该行时出现的任何连续的“*”字符也被丢弃。然后将处理后的行连接在一起（包括行终止符）并返回。
     * </pre>
     * <p> A documentation comment of an element is a comment that
     * begins with "{@code /**}" , ends with a separate
     * "<code>*&#47;</code>", and immediately precedes the element,
     * ignoring white space.  Therefore, a documentation comment
     * contains at least three"{@code *}" characters.  The text
     * returned for the documentation comment is a processed form of
     * the comment as it appears in source code.  The leading "{@code
     * /**}" and trailing "<code>*&#47;</code>" are removed.  For lines
     * of the comment starting after the initial "{@code /**}",
     * leading white space characters are discarded as are any
     * consecutive "{@code *}" characters appearing after the white
     * space or starting the line.  The processed lines are then
     * concatenated together (including line terminators) and
     * returned.
     *
     * @param e  the element being examined
     * @return the documentation comment of the element, or {@code null}
     *          if there is none
     * @jls 3.6 White Space
     */
    String getDocComment(Element e);

    /**
     * Returns {@code true} if the element is deprecated, {@code false} otherwise.
     * 如果指定的元素是deprecated,则返回true,否则,返回false.
     * @param e  the element being examined
     * @return {@code true} if the element is deprecated, {@code false} otherwise
     */
    boolean isDeprecated(Element e);

    /**
     * Returns the <i>binary name</i> of a type element.
     * 返回指定元素所对应的二进制名称.
     * 关于这部分内容,可以参阅 JLS 13.1 二进制形式
     * @param type  the type element being examined
     * @return the binary name
     *
     * @see TypeElement#getQualifiedName
     * @jls 13.1 The Form of a Binary
     */
    Name getBinaryName(TypeElement type);


    /**
     * Returns the package of an element.  The package of a package is
     * itself.
     * 返回指定元素所对应的包,如果指定的元素是包,则返回自己
     * @param type the element being examined
     * @return the package of an element
     */
    PackageElement getPackageOf(Element type);

    /**
     * Returns all members of a type element, whether inherited or
     * declared directly.  For a class the result also includes its
     * constructors, but not local or anonymous classes.
     * 返回指定元素的内部成员,包括继承和直接声明的.
     * 对于一个类来说,包含构造器,但是不包含本地或者匿名类
     * 请注意，某些类型的元素可以使用ElementFilter中的方法进行过滤。
     * <p>Note that elements of certain kinds can be isolated using
     * methods in {@link ElementFilter}.
     *
     * @param type  the type being examined
     * @return all members of the type
     * @see Element#getEnclosedElements
     */
    List<? extends Element> getAllMembers(TypeElement type);

    /**
     * Returns all annotations <i>present</i> on an element, whether
     * directly present or present via inheritance.
     * 返回该元素所对应的所有注解,不管是直接声明的还是继承的
     * @param e  the element being examined
     * @return all annotations of the element
     * @see Element#getAnnotationMirrors
     * @see javax.lang.model.AnnotatedConstruct
     */
    List<? extends AnnotationMirror> getAllAnnotationMirrors(Element e);

    /**
     * Tests whether one type, method, or field hides another.
     * 如果第一个Element隐藏了第二个Element,则返回true
     * @param hider   the first element
     * @param hidden  the second element
     * @return {@code true} if and only if the first element hides
     *          the second
     */
    boolean hides(Element hider, Element hidden);

    /**
     * Tests whether one method, as a member of a given type,
     * overrides another method.
     * When a non-abstract method overrides an abstract one, the
     * former is also said to <i>implement</i> the latter.
     *
     * <p> In the simplest and most typical usage, the value of the
     * {@code type} parameter will simply be the class or interface
     * directly enclosing {@code overrider} (the possibly-overriding
     * method).  For example, suppose {@code m1} represents the method
     * {@code String.hashCode} and {@code m2} represents {@code
     * Object.hashCode}.  We can then ask whether {@code m1} overrides
     * {@code m2} within the class {@code String} (it does):
     *
     * <blockquote>
     * {@code assert elements.overrides(m1, m2,
     *          elements.getTypeElement("java.lang.String")); }
     * </blockquote>
     *
     * A more interesting case can be illustrated by the following example
     * in which a method in type {@code A} does not override a
     * like-named method in type {@code B}:
     *
     * <blockquote>
     * {@code class A { public void m() {} } }<br>
     * {@code interface B { void m(); } }<br>
     * ...<br>
     * {@code m1 = ...;  // A.m }<br>
     * {@code m2 = ...;  // B.m }<br>
     * {@code assert ! elements.overrides(m1, m2,
     *          elements.getTypeElement("A")); }
     * </blockquote>
     *
     * When viewed as a member of a third type {@code C}, however,
     * the method in {@code A} does override the one in {@code B}:
     *
     * <blockquote>
     * {@code class C extends A implements B {} }<br>
     * ...<br>
     * {@code assert elements.overrides(m1, m2,
     *          elements.getTypeElement("C")); }
     * </blockquote>
     * 测试第一个方法,作为制定类型的成员,复写了第二个方法
     * 当一个非抽象的方法复写了抽象的方法,这也叫实现.
     * 在最简单和最典型的用法中，类型参数的值只是含有复写方法的类或接口。举例:m1代表的是
     * String.hashCode,m2代表的是Object.hashCode.我们可以通过如下方式来测试是否String
     * 中的m1的方法复写了m2:
     * assert elements.overrides(m1, m2,
     *          elements.getTypeElement("java.lang.String"));
     *  以下示例可说明更有趣的情况，其中类型A中的方法不重写类型B中的同名方法：
     *    class A { public void m() {} } }
     *       interface B { void m(); } }
     *       ...
     *       m1 = ...; // A.m
     *       m2 = ...; // B.m
     *       assert ! elements.overrides(m1, m2, elements.getTypeElement("A"));
     *  当有第三个类型c时,A中的方法就复写了B中的方法:
     *  assert elements.overrides(m1, m2,
     *          elements.getTypeElement("C"));
     *
     * @param overrider  the first method, possible overrider
     * @param overridden  the second method, possibly being overridden
     * @param type   the type of which the first method is a member
     * @return {@code true} if and only if the first method overrides
     *          the second
     * @jls 8.4.8 Inheritance, Overriding, and Hiding
     * @jls 9.4.1 Inheritance and Overriding
     */
    boolean overrides(ExecutableElement overrider, ExecutableElement overridden,
                      TypeElement type);

    /**
     * Returns the text of a <i>constant expression</i> representing a
     * primitive value or a string.
     * The text returned is in a form suitable for representing the value
     * in source code.
     * 返回表示原始类型值或字符串的常量表达式的文本。返回的文本格式适合于表示源代码中的值。
     * 参数value是 原始类型的值或者是String
     * @param value  a primitive value or string
     * @return the text of a constant expression
     * @throws IllegalArgumentException if the argument is not a primitive
     *          value or string
     *
     * @see VariableElement#getConstantValue()
     */
    String getConstantExpression(Object value);

    /**
     * Prints a representation of the elements to the given writer in
     * the specified order.  The main purpose of this method is for
     * diagnostics.  The exact format of the output is <em>not</em>
     * specified and is subject to change.
     *  按指定的顺序将元素的表示打印到给定的Writer。这种方法的主要目的是用于诊断。未指定输出的确切格式，并有可能更改这种输出格式。
     * @param w the writer to print the output to
     * @param elements the elements to print
     */
    void printElements(java.io.Writer w, Element... elements);

    /**
     * Return a name with the same sequence of characters as the
     * argument.
     * 返回指定参数所对应的Name
     * @param cs the character sequence to return as a name
     * @return a name with the same sequence of characters as the argument
     */
    Name getName(CharSequence cs);

    /**
     * Returns {@code true} if the type element is a functional interface, {@code false} otherwise.
     * 如果给定的类是函数接口,则返回true
     * @param type the type element being examined
     * @return {@code true} if the element is a functional interface, {@code false} otherwise
     * @jls 9.8 Functional Interfaces
     * @since 1.8
     */
    boolean isFunctionalInterface(TypeElement type);
}
