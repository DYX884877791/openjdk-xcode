/*
 * Copyright (c) 2002, 2011, Oracle and/or its affiliates. All rights reserved.
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

/*
 */

package java.io;

import java.util.Iterator;
import java.util.Map;
import java.util.LinkedHashMap;
import java.util.Set;

/**
 * ExpiringCache是io包下包内可见的一个基于LinkedHashMap实现的支持自动过期删除的缓存实现
 */
class ExpiringCache {
    //元素的过期时间
    private long millisUntilExpiration;
    //保存元素的Map
    private Map<String,Entry> map;
    // Clear out old entries every few queries
    //queryCount表示读写计数，get和put时都会加1，如果超过queryOverflow则会清除掉所有的过期Entry
    private int queryCount;
    private int queryOverflow = 300;
    //最大元素个数
    private int MAX_ENTRIES = 200;

    static class Entry {
        private long   timestamp;
        private String val;

        Entry(long timestamp, String val) {
            this.timestamp = timestamp;
            this.val = val;
        }

        long   timestamp()                  { return timestamp;           }
        void   setTimestamp(long timestamp) { this.timestamp = timestamp; }

        String val()                        { return val;                 }
        void   setVal(String val)           { this.val = val;             }
    }

    ExpiringCache() {
        this(30000);
    }

    @SuppressWarnings("serial")
    ExpiringCache(long millisUntilExpiration) {
        this.millisUntilExpiration = millisUntilExpiration;
        map = new LinkedHashMap<String,Entry>() {
            /**
             * 重写了removeEldestEntry方法，该方法是在新插入一个元素时调用的，如果返回true，则会将LinkHashMap中维护的双向链表的链表头节点对应的key从Map中移除。
             * 因为是采用默认的构造函数，即双向链表中维护的是元素插入顺序而非访问顺序，所以当元素个数超过200时会移除第一个插入的元素
             */
            protected boolean removeEldestEntry(Map.Entry<String,Entry> eldest) {
              return size() > MAX_ENTRIES;
            }
          };
    }

    /**
     * 其核心就是插入键值对的put方法和根据key值获取value的get方法
     */
    synchronized String get(String key) {
        if (++queryCount >= queryOverflow) {
            //queryCount加1后，如果超过queryOverflow，则清理掉所有所有过期Entry
            cleanup();
        }
        //判断是否存在未过期的相同key的Entry
        Entry entry = entryFor(key);
        if (entry != null) {
            return entry.val();
        }
        return null;
    }

    synchronized void put(String key, String val) {
        if (++queryCount >= queryOverflow) {
            //queryCount加1后，如果超过queryOverflow，则清理掉所有所有过期Entry
            cleanup();
        }
        //查找未过期的Entry
        Entry entry = entryFor(key);
        if (entry != null) {
            //如果存在则更新修改时间
            entry.setTimestamp(System.currentTimeMillis());
            entry.setVal(val);
        } else {
            //不存在则插入一个新的
            map.put(key, new Entry(System.currentTimeMillis(), val));
        }
    }

    synchronized void clear() {
        map.clear();
    }

    private Entry entryFor(String key) {
        Entry entry = map.get(key);
        if (entry != null) {
            long delta = System.currentTimeMillis() - entry.timestamp();
            if (delta < 0 || delta >= millisUntilExpiration) {
                map.remove(key);
                entry = null;
            }
        }
        return entry;
    }

    private void cleanup() {
        Set<String> keySet = map.keySet();
        // Avoid ConcurrentModificationExceptions
        String[] keys = new String[keySet.size()];
        int i = 0;
        for (String key: keySet) {
            keys[i++] = key;
        }
        for (int j = 0; j < keys.length; j++) {
            entryFor(keys[j]);
        }
        queryCount = 0;
    }
}
