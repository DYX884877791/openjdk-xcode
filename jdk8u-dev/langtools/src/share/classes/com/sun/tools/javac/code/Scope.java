/*
 * Copyright (c) 1999, 2015, Oracle and/or its affiliates. All rights reserved.
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

package com.sun.tools.javac.code;

import java.util.Iterator;

import com.sun.tools.javac.util.*;

/** A scope represents an area of visibility in a Java program. The
 *  Scope class is a container for symbols which provides
 *  efficient access to symbols given their names. Scopes are implemented
 *  as hash tables with "open addressing" and "double hashing".
 *  Scopes can be nested; the next field of a scope points
 *  to its next outer scope. Nested scopes can share their hash tables.
 *
 *  <p><b>This is NOT part of any supported API.
 *  If you write code that depends on this, you do so at your own risk.
 *  This code and its internal interfaces are subject to change or
 *  deletion without notice.</b>
 * 关于作业域的实现-Scope.该类表示Java程序中的可见性区域。Scope类是符号的容器，它提供了对给定名称的符号的有效访问。
 * 以哈希表的形式实现，具有“open addressing”和“double hashing”。作用域可以嵌套；作用域的下一个字段指向它的下一个外部范围。嵌套作用域可以共享它们的哈希表.
 */
public class Scope {

    /** The number of scopes that share this scope's hash table.
     * 分享当前Scope的hash table的数量
     */
    private int shared;

    /** Next enclosing scope (with whom this scope may share a hashtable)
     * 下一个包围着的Scope（与此范围可以共享一个哈希表）
     */
    public Scope next;

    /** The scope's owner.
     * 当前scope所对应的Symbol
     */
    public Symbol owner;

    /** A hash table for the scope's entries.
     * hash 表
     */
    Entry[] table;

    /** Mask for hash codes, always equal to (table.length - 1).
     * hash code 的掩码,该值总是等于table.length - 1
     */
    int hashMask;

    /** A linear list that also contains all entries in
     *  reverse order of appearance (i.e later entries are pushed on top).
     * 一个线性列表，它还包含所有出现在相反顺序的条目（即以后的条目被推到顶部）。
     */
    public Entry elems;

    /** The number of elements in this scope.
     * This includes deleted elements, whose value is the sentinel.
     * 在当前作用域中的元素的数量,包括已经被删除的element--> 哨兵
     */
    int nelems = 0;

    /** A list of scopes to be notified if items are to be removed from this scope.
     * 当保存在该scope中的ScopeListener,当该scope中有结构变化时会进行唤醒的Listener。
     */
    List<ScopeListener> listeners = List.nil();

    /** Use as a "not-found" result for lookup.
     * Also used to mark deleted entries in the table.
     * 用来标识没有找到在当前作用域,同时用来表示删除的元素
     */
    private static final Entry sentinel = new Entry(null, null, null, null);

    /** The hash table's initial size.
     * 表初始的大小,初始大小为16
     */
    private static final int INITIAL_SIZE = 0x10;

    /** A value for the empty scope.
     *  代表一个空的scope
     */
    public static final Scope emptyScope = new Scope(null, null, new Entry[]{});

    /** Construct a new scope, within scope next, with given owner, using
     *  given table. The table's length must be an exponent of 2.
     * table的长度必须是2的指数倍
     */
    private Scope(Scope next, Symbol owner, Entry[] table) {
        this.next = next;
        Assert.check(emptyScope == null || owner != null);
        this.owner = owner;
        this.table = table;
        this.hashMask = table.length - 1;
    }

    /** Convenience constructor used for dup and dupUnshared.
     * 用于dup(复制)和dupUnshared(不共享的复制)的构造器
     */
    private Scope(Scope next, Symbol owner, Entry[] table, int nelems) {
        this(next, owner, table);
        this.nelems = nelems;
    }

    /** Construct a new scope, within scope next, with given owner,
     *  using a fresh table of length INITIAL_SIZE.
     * 创建一个scope，指定的owner,作为新创建的scope的下一个,同时有一个新的table,其长度为16
     */
    public Scope(Symbol owner) {
        this(null, owner, new Entry[INITIAL_SIZE]);
    }

    /** Construct a fresh scope within this scope, with same owner,
     *  which shares its table with the outer scope. Used in connection with
     *  method leave if scope access is stack-like in order to avoid allocation
     *  of fresh tables.
     * dup 操作 ：创建一个新的scope,其拥有同样的owner,新创建的scope和当前使用同一个table。
     */
    public Scope dup() {
        return dup(this.owner);
    }

    /** Construct a fresh scope within this scope, with new owner,
     *  which shares its table with the outer scope. Used in connection with
     *  method leave if scope access is stack-like in order to avoid allocation
     *  of fresh tables.
     * dup 操作 ：在这个范围内构建一个新的范围，拥有新的拥有者，它与外部范围共享它的表。如果范围访问是堆栈式的，以避免分配新的表.
     */
    public Scope dup(Symbol newOwner) {
        Scope result = new Scope(this, newOwner, this.table, this.nelems);
        shared++;
        // System.out.println("====> duping scope " + this.hashCode() + " owned by " + newOwner + " to " + result.hashCode());
        // new Error().printStackTrace(System.out);
        return result;
    }

    /** Construct a fresh scope within this scope, with same owner,
     *  with a new hash table, whose contents initially are those of
     *  the table of its outer scope.
     * dup 操作 ：但是不共享table.
     */
    public Scope dupUnshared() {
        return new Scope(this, this.owner, this.table.clone(), this.nelems);
    }

    /** Remove all entries of this scope from its table, if shared
     *  with next.
     * 删除当前scope中的所有元素.
     */
    public Scope leave() {
        Assert.check(shared == 0);
        if (table != next.table) return next; // 如果当前的table和下一个table不是共享的,则直接返回下一个scope
        // 如果是共享的,则在table中删除本scope中的东西
        while (elems != null) {
            int hash = getIndex(elems.sym.name);
            Entry e = table[hash];
            Assert.check(e == elems, elems.sym);
            table[hash] = elems.shadowed;
            elems = elems.sibling;
        }
        Assert.check(next.shared > 0);
        next.shared--;
        next.nelems = nelems;
        // System.out.println("====> leaving scope " + this.hashCode() + " owned by " + this.owner + " to " + next.hashCode());
        // new Error().printStackTrace(System.out);
        return next;
    }

    /** Double size of hash table.
     * 扩容操作.
     */
    private void dble() {
        Assert.check(shared == 0);
        Entry[] oldtable = table;
        Entry[] newtable = new Entry[oldtable.length * 2];
        for (Scope s = this; s != null; s = s.next) {
            if (s.table == oldtable) {
                Assert.check(s == this || s.shared != 0);
                s.table = newtable;
                s.hashMask = newtable.length - 1;
            }
        }
        // 将原先scope中的元素加入到new table中
        int n = 0;
        for (int i = oldtable.length; --i >= 0; ) {
            Entry e = oldtable[i];
            if (e != null && e != sentinel) {
                table[getIndex(e.sym.name)] = e;
                n++;
            }
        }
        // We don't need to update nelems for shared inherited scopes,
        // since that gets handled by leave().
        nelems = n;
    }

    /** Enter symbol sym in this scope.
     * 插入操作,将指定的Symbol插入到当前scope中.
     */
    public void enter(Symbol sym) {
        Assert.check(shared == 0);
        enter(sym, this);
    }

    /**
     * 插入操作,将指定的Symbol插入到指定的scope中.
     */
    public void enter(Symbol sym, Scope s) {
        enter(sym, s, s, false);
    }

    /**
     * Enter symbol sym in this scope, but mark that it comes from
     * given scope `s' accessed through `origin'.  The last two
     * arguments are only used in import scopes.
     * 插入操作。将指定的symbol插入到当前的scope.但是标识它是来自Scope s的,通过origin获取的.origin,staticallyImported 这两个参数是用在 import scope的
     * 那么此时有2种情况:
     * 1. symbol在当前scope中不存在,则此时需要创建一个Entry,其中Entry的shadowed指向哨兵, sibling指向当前的elems.创建完之后,在将elems 赋值为新创建的elems.
     *      新创建的Entry在链表中处于第一个,而sentinel则是在最后一个,这也就是为什么说elems是一个线性列表，它还包含所有出现在相反顺序的条目（即以后的条目被推到顶部）
     * 2. symbol所对应的hash值在当前的scope中已经有了
     */
    public void enter(Symbol sym, Scope s, Scope origin, boolean staticallyImported) {
        Assert.check(shared == 0);
        // 如果nelems * 3 >= hashMask * 2 则进行扩容一倍的处理
        if (nelems * 3 >= hashMask * 2)
            dble();
        // 获取Symbol在当前scope中的操作
        int hash = getIndex(sym.name);
        Entry old = table[hash];
        if (old == null) { // 如果Symbol在当前scope中不存在,则其为sentinel,并增加nelems
            old = sentinel;
            nelems++;
        }
        // 创建Entry 添加到table中
        Entry e = makeEntry(sym, old, elems, s, origin, staticallyImported);
        table[hash] = e;
        elems = e;

        //notify listeners
        // 唤醒ScopeListener
        for (List<ScopeListener> l = listeners; l.nonEmpty(); l = l.tail) {
            l.head.symbolAdded(sym, this);
        }
    }

    Entry makeEntry(Symbol sym, Entry shadowed, Entry sibling, Scope scope, Scope origin, boolean staticallyImported) {
        return new Entry(sym, shadowed, sibling, scope);
    }


    public interface ScopeListener {
        public void symbolAdded(Symbol sym, Scope s);
        public void symbolRemoved(Symbol sym, Scope s);
    }

    public void addScopeListener(ScopeListener sl) {
        listeners = listeners.prepend(sl);
    }

    /** Remove symbol from this scope.
     * 删除操作.
     *  Entry 维护着shadowed和sibling,因此在删除时需要分别做处理
     */
    public void remove(final Symbol sym) {
        Assert.check(shared == 0);
        // 在当前scope中查找
        Entry e = lookup(sym.name, new Filter<Symbol>() {
            @Override
            public boolean accepts(Symbol candidate) {
                return candidate == sym;
            }
        });
        // 如果e为sentinel,则不进行后续处理
        if (e.scope == null) return;

        // remove e from table and shadowed list;
        //  从表中和shadowed list 中删除e
        int i = getIndex(sym.name);
        Entry te = table[i];
        if (te == e)
            table[i] = e.shadowed;
        else while (true) {
            if (te.shadowed == e) {
                te.shadowed = e.shadowed;
                break;
            }
            te = te.shadowed;
        }

        // remove e from elems and sibling list
        // 从elems中和sibling list中删除e
        te = elems;
        if (te == e)
            elems = e.sibling;
        else while (true) {
            if (te.sibling == e) {
                te.sibling = e.sibling;
                break;
            }
            te = te.sibling;
        }

        //notify listeners
        // 唤醒ScopeListener
        for (List<ScopeListener> l = listeners; l.nonEmpty(); l = l.tail) {
            l.head.symbolRemoved(sym, this);
        }
    }

    /** Enter symbol sym in this scope if not already there.
     * 插入操作,如果不存在,才插入.
     */
    public void enterIfAbsent(Symbol sym) {
        Assert.check(shared == 0);
        Entry e = lookup(sym.name);
        while (e.scope == this && e.sym.kind != sym.kind) e = e.next();
        if (e.scope != this) enter(sym);
    }

    /** Given a class, is there already a class with same fully
     *  qualified name in this (import) scope?
     * 判断指定的symbol是否在当前scope中存在.
     */
    public boolean includes(Symbol c) {
        for (Scope.Entry e = lookup(c.name);
             e.scope == this;
             e = e.next()) {
            if (e.sym == c) return true;
        }
        return false;
    }

    static final Filter<Symbol> noFilter = new Filter<Symbol>() {
        public boolean accepts(Symbol s) {
            return true;
        }
    };

    /** Return the entry associated with given name, starting in
     *  this scope and proceeding outwards. If no entry was found,
     *  return the sentinel, which is characterized by having a null in
     *  both its scope and sym fields, whereas both fields are non-null
     *  for regular entries.
     * 过滤操作.
     */
    public Entry lookup(Name name) {
        return lookup(name, noFilter);
    }

    public Entry lookup(Name name, Filter<Symbol> sf) {
        Entry e = table[getIndex(name)];
        // 如果Name在当前scope中不存在的话,或者e是sentinel,则返回sentinel
        if (e == null || e == sentinel)
            return sentinel;
        while (e.scope != null && (e.sym.name != name || !sf.accepts(e.sym)))
            e = e.shadowed;
        return e;
    }

    /*void dump (java.io.PrintStream out) {
        out.println(this);
        for (int l=0; l < table.length; l++) {
            Entry le = table[l];
            out.print("#"+l+": ");
            if (le==sentinel) out.println("sentinel");
            else if(le == null) out.println("null");
            else out.println(""+le+" s:"+le.sym);
        }
    }*/

    /** Look for slot in the table.
     *  We use open addressing with double hashing.
     * 查找下标操作:
     */
    int getIndex (Name name) {
        int h = name.hashCode();
        int i = h & hashMask;
        // The expression below is always odd, so it is guaranteed
        // to be mutually prime with table.length, a power of 2. 下面的表达式总是奇数，因为保证它与表的长度(2的幂)相互素数。
        // 双重散列法(Double Hashing) 必须使 h1(key) 的值和 m 互素，才能使发生冲突的同义词地址均匀地分布在整个表中，否则可能造成同义词地址的循环计算。
        int x = hashMask - ((h + (h >> 16)) << 1);
        int d = -1; // Index of a deleted item. 已删除项目的索引
        for (;;) {
            Entry e = table[i];
            if (e == null)
                return d >= 0 ? d : i;
            if (e == sentinel) {
                // We have to keep searching even if we see a deleted item.
                // However, remember the index in case we fail to find the name. 我们必须继续搜索，即使我们看到一个被删除的项目。但是，如果我们找不到名称，请记住索引
                if (d < 0)
                    d = i;
            } else if (e.sym.name == name)
                return i;
            i = (i + x) & hashMask;
        }
    }

    public boolean anyMatch(Filter<Symbol> sf) {
        return getElements(sf).iterator().hasNext();
    }

    public Iterable<Symbol> getElements() {
        return getElements(noFilter);
    }

    public Iterable<Symbol> getElements(final Filter<Symbol> sf) {
        return new Iterable<Symbol>() {
            public Iterator<Symbol> iterator() {
                return new Iterator<Symbol>() {
                    private Scope currScope = Scope.this;
                    private Scope.Entry currEntry = elems;
                    {
                        update();
                    }

                    public boolean hasNext() {
                        return currEntry != null;
                    }

                    public Symbol next() {
                        Symbol sym = (currEntry == null ? null : currEntry.sym);
                        if (currEntry != null) {
                            currEntry = currEntry.sibling;
                        }
                        update();
                        return sym;
                    }

                    public void remove() {
                        throw new UnsupportedOperationException();
                    }

                    private void update() {
                        skipToNextMatchingEntry();
                        while (currEntry == null && currScope.next != null) {
                            currScope = currScope.next;
                            currEntry = currScope.elems;
                            skipToNextMatchingEntry();
                        }
                    }

                    void skipToNextMatchingEntry() {
                        while (currEntry != null && !sf.accepts(currEntry.sym)) {
                            currEntry = currEntry.sibling;
                        }
                    }
                };
            }
        };
    }

    public Iterable<Symbol> getElementsByName(Name name) {
        return getElementsByName(name, noFilter);
    }

    public Iterable<Symbol> getElementsByName(final Name name, final Filter<Symbol> sf) {
        return new Iterable<Symbol>() {
            public Iterator<Symbol> iterator() {
                 return new Iterator<Symbol>() {
                    Scope.Entry currentEntry = lookup(name, sf);

                    public boolean hasNext() {
                        return currentEntry.scope != null;
                    }
                    public Symbol next() {
                        Scope.Entry prevEntry = currentEntry;
                        currentEntry = currentEntry.next(sf);
                        return prevEntry.sym;
                    }
                    public void remove() {
                        throw new UnsupportedOperationException();
                    }
                };
            }
        };
    }

    public String toString() {
        StringBuilder result = new StringBuilder();
        result.append("Scope[");
        for (Scope s = this; s != null ; s = s.next) {
            if (s != this) result.append(" | ");
            for (Entry e = s.elems; e != null; e = e.sibling) {
                if (e != s.elems) result.append(", ");
                result.append(e.sym);
            }
        }
        result.append("]");
        return result.toString();
    }

    /** A class for scope entries.
     */
    public static class Entry {

        /** The referenced symbol.
         *  sym == null   iff   this == sentinel
         * 引用的symbol,当且仅当 this == sentinel 时 sym == null
         */
        public Symbol sym;

        /** An entry with the same hash code, or sentinel.
         * 拥有同样hashcode的Entry,或者是哨兵
         */
        private Entry shadowed;

        /** Next entry in same scope.
         * 在同一个scope中的下一个Entry
         */
        public Entry sibling;

        /** The entry's scope.
         *  scope == null   iff   this == sentinel
         *  for an entry in an import scope, this is the scope
         *  where the entry came from (i.e. was imported from).
         *  当且仅当 this == sentinel 时 scope == null.
         *  当这个entry是在import scope中,那么这个scope就是这个entry所对应的scope(被导入的scope)
         */
        public Scope scope;

        public Entry(Symbol sym, Entry shadowed, Entry sibling, Scope scope) {
            this.sym = sym;
            this.shadowed = shadowed;
            this.sibling = sibling;
            this.scope = scope;
        }

        /** Return next entry with the same name as this entry, proceeding
         *  outwards if not found in this scope.
         * 返回与该条目名称相同的下一个条目，如果在该范围内未找到，则向外(即父scope)继续查找
         */
        public Entry next() {
            return shadowed;
        }

        /**
         * 获得符合条件的下一个Entry.
         */
        public Entry next(Filter<Symbol> sf) {
            if (shadowed.sym == null || sf.accepts(shadowed.sym)) return shadowed;
            else return shadowed.next(sf);
        }

        /**
         * 判断该entry是否是静态导入的
         */
        public boolean isStaticallyImported() {
            return false;
        }

        /**
         * origin仅用于import scope中。对于其他的scope entry来说,enclosing type 是可以用的.
         */
        public Scope getOrigin() {
            // The origin is only recorded for import scopes.  For all
            // other scope entries, the "enclosing" type is available
            // from other sources.  See Attr.visitSelect and
            // Attr.visitIdent.  Rather than throwing an assertion
            // error, we return scope which will be the same as origin
            // in many cases.
            return scope;
        }
    }

    /**
     * 代表import 语句导入的scope
     */
    public static class ImportScope extends Scope {

        public ImportScope(Symbol owner) {
            super(owner);
        }

        @Override
        Entry makeEntry(Symbol sym, Entry shadowed, Entry sibling, Scope scope,
                final Scope origin, final boolean staticallyImported) {
            return new Entry(sym, shadowed, sibling, scope) {
                @Override
                public Scope getOrigin() {
                    return origin;
                }

                @Override
                public boolean isStaticallyImported() {
                    return staticallyImported;
                }
            };
        }
    }

    /**
     * 代表static import 语句导入的scope
     */
    public static class StarImportScope extends ImportScope implements ScopeListener {

        public StarImportScope(Symbol owner) {
            super(owner);
        }

        public void importAll (Scope fromScope) {
            for (Scope.Entry e = fromScope.elems; e != null; e = e.sibling) {
                if (e.sym.kind == Kinds.TYP && !includes(e.sym))
                    enter(e.sym, fromScope);
            }
            // Register to be notified when imported items are removed
            fromScope.addScopeListener(this);
        }

        public void symbolRemoved(Symbol sym, Scope s) {
            remove(sym);
        }
        public void symbolAdded(Symbol sym, Scope s) { }
    }

    /** An empty scope, into which you can't place anything.  Used for
     *  the scope for a variable initializer.
     * 一个空的范围，不能放置任何东西。用于变量初始化器的作用域
     */
    public static class DelegatedScope extends Scope {
        Scope delegatee;
        public static final Entry[] emptyTable = new Entry[0];

        public DelegatedScope(Scope outer) {
            super(outer, outer.owner, emptyTable);
            delegatee = outer;
        }
        public Scope dup() {
            return new DelegatedScope(next);
        }
        public Scope dupUnshared() {
            return new DelegatedScope(next);
        }
        public Scope leave() {
            return next;
        }
        public void enter(Symbol sym) {
            // only anonymous classes could be put here
        }
        public void enter(Symbol sym, Scope s) {
            // only anonymous classes could be put here
        }
        public void remove(Symbol sym) {
            throw new AssertionError(sym);
        }
        public Entry lookup(Name name) {
            return delegatee.lookup(name);
        }
    }

    /** A class scope adds capabilities to keep track of changes in related
     *  class scopes - this allows client to realize whether a class scope
     *  has changed, either directly (because a new member has been added/removed
     *  to this scope) or indirectly (i.e. because a new member has been
     *  added/removed into a supertype scope)
     * 代表一个类作用域添加了跟踪相关类作用域变化的能力.这允许客户端直接（因为新成员已经添加/移除到此作用域）或间接（即，因为新成员已经添加/移除到父类的 scope中）了解类作用域是否已经改变。
     */
    public static class CompoundScope extends Scope implements ScopeListener {

        public static final Entry[] emptyTable = new Entry[0];

        private List<Scope> subScopes = List.nil();
        private int mark = 0;

        public CompoundScope(Symbol owner) {
            super(null, owner, emptyTable);
        }

        public void addSubScope(Scope that) {
           if (that != null) {
                subScopes = subScopes.prepend(that);
                that.addScopeListener(this);
                mark++;
                for (ScopeListener sl : listeners) {
                    sl.symbolAdded(null, this); //propagate upwards in case of nested CompoundScopes
                }
           }
         }

        public void symbolAdded(Symbol sym, Scope s) {
            mark++;
            for (ScopeListener sl : listeners) {
                sl.symbolAdded(sym, s);
            }
        }

        public void symbolRemoved(Symbol sym, Scope s) {
            mark++;
            for (ScopeListener sl : listeners) {
                sl.symbolRemoved(sym, s);
            }
        }

        public int getMark() {
            return mark;
        }

        @Override
        public String toString() {
            StringBuilder buf = new StringBuilder();
            buf.append("CompoundScope{");
            String sep = "";
            for (Scope s : subScopes) {
                buf.append(sep);
                buf.append(s);
                sep = ",";
            }
            buf.append("}");
            return buf.toString();
        }

        @Override
        public Iterable<Symbol> getElements(final Filter<Symbol> sf) {
            return new Iterable<Symbol>() {
                public Iterator<Symbol> iterator() {
                    return new CompoundScopeIterator(subScopes) {
                        Iterator<Symbol> nextIterator(Scope s) {
                            return s.getElements(sf).iterator();
                        }
                    };
                }
            };
        }

        @Override
        public Iterable<Symbol> getElementsByName(final Name name, final Filter<Symbol> sf) {
            return new Iterable<Symbol>() {
                public Iterator<Symbol> iterator() {
                    return new CompoundScopeIterator(subScopes) {
                        Iterator<Symbol> nextIterator(Scope s) {
                            return s.getElementsByName(name, sf).iterator();
                        }
                    };
                }
            };
        }

        abstract class CompoundScopeIterator implements Iterator<Symbol> {

            private Iterator<Symbol> currentIterator;
            private List<Scope> scopesToScan;

            public CompoundScopeIterator(List<Scope> scopesToScan) {
                this.scopesToScan = scopesToScan;
                update();
            }

            abstract Iterator<Symbol> nextIterator(Scope s);

            public boolean hasNext() {
                return currentIterator != null;
            }

            public Symbol next() {
                Symbol sym = currentIterator.next();
                if (!currentIterator.hasNext()) {
                    update();
                }
                return sym;
            }

            public void remove() {
                throw new UnsupportedOperationException();
            }

            private void update() {
                while (scopesToScan.nonEmpty()) {
                    currentIterator = nextIterator(scopesToScan.head);
                    scopesToScan = scopesToScan.tail;
                    if (currentIterator.hasNext()) return;
                }
                currentIterator = null;
            }
        }

        @Override
        public Entry lookup(Name name, Filter<Symbol> sf) {
            throw new UnsupportedOperationException();
        }

        @Override
        public Scope dup(Symbol newOwner) {
            throw new UnsupportedOperationException();
        }

        @Override
        public void enter(Symbol sym, Scope s, Scope origin, boolean staticallyImported) {
            throw new UnsupportedOperationException();
        }

        @Override
        public void remove(Symbol sym) {
            throw new UnsupportedOperationException();
        }
    }

    /** An error scope, for which the owner should be an error symbol.
     * 代表一个错误的scope
     */
    public static class ErrorScope extends Scope {
        ErrorScope(Scope next, Symbol errSymbol, Entry[] table) {
            super(next, /*owner=*/errSymbol, table);
        }
        public ErrorScope(Symbol errSymbol) {
            super(errSymbol);
        }
        public Scope dup() {
            return new ErrorScope(this, owner, table);
        }
        public Scope dupUnshared() {
            return new ErrorScope(this, owner, table.clone());
        }
        public Entry lookup(Name name) {
            Entry e = super.lookup(name);
            if (e.scope == null)
                return new Entry(owner, null, null, null);
            else
                return e;
        }
    }
}
