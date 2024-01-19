---
source: https://gee.cs.oswego.edu/dl/concurrency-interest/
---
Maintained by [Doug Lea](http://gee.cs.oswego.edu/dl/)

To join a mailing list discussing this JSR, go to: [http://altair.cs.oswego.edu/mailman/listinfo/concurrency-interest](http://altair.cs.oswego.edu/mailman/listinfo/concurrency-interest). (Archived postings may also be found at [MarkMail's searchable archives](http://concurrency.markmail.org/).)

While JSR166 has completed and is a now final approved JCP spec, the expert group remains involved in incremental improvements and changes to the java.util.concurrent package and related classes and packages.

___

## JSR166 software repository

Classes that are part of JSR166, as well as related classes, tests, benchmarks and demo programs are available in a CVS repository. This repository can be accessed via anonymous CVS using a CVSROOT of :pserver:anonymous@gee.cs.oswego.edu/home/jsr166/jsr166:

```
cvs -d ':pserver:anonymous@gee.cs.oswego.edu/home/jsr166/jsr166' checkout jsr166

```

You can also get a [tar.gz snapshot of all sources](http://gee.cs.oswego.edu/cgi-bin/viewcvs.cgi/jsr166/jsr166.tar.gz?view=tar) via viewcvs.

Pre-built binary artifacts are provided (see below), but you can also build your own by following instructions provided upon running ant in the repository root directory.

___

## JSR166 maintenance updates

The most recent versions of JSR166 classes, targeting current or soon-upcoming openjdk releases:

-   API specs: [http://gee.cs.oswego.edu/dl/jsr166/dist/docs/](http://gee.cs.oswego.edu/dl/jsr166/dist/docs/)
-   jar file: [http://gee.cs.oswego.edu/dl/jsr166/dist/jsr166.jar](http://gee.cs.oswego.edu/dl/jsr166/dist/jsr166.jar) (compiled using Java17 javac).
-   Browsable CVS sources: [http://gee.cs.oswego.edu/cgi-bin/viewcvs.cgi/jsr166/jsr166/src/main/java/util/](http://gee.cs.oswego.edu/cgi-bin/viewcvs.cgi/jsr166/jsr166/src/main/java/util/)
-   Browsable CVS TCK test sources: [http://gee.cs.oswego.edu/cgi-bin/viewcvs.cgi/jsr166/jsr166/src/tests/tck/](http://gee.cs.oswego.edu/cgi-bin/viewcvs.cgi/jsr166/jsr166/src/test/tck/)

You may be able to use these versions now, without waiting for JDK releases, by obtaining [jsr166 jar](http://gee.cs.oswego.edu/dl/concurrent/dist/jsr166.jar), then running java using the option java --patch-module java.base="$DIR/jsr166.jar", where DIR is the full file prefix.

___

## Repository jsr166-4jdk8

Updated versions of classes originally incorporated in JDK8. These may include updated algorithms within existing classes, as well as minor extensions that ultimately ended up in JDK9 versions, but do not rely on any other JDK9 features.

-   Browsable CVS sources: [http://gee.cs.oswego.edu/cgi-bin/viewcvs.cgi/jsr166/jsr166/src/jdk8/java/util/](http://gee.cs.oswego.edu/cgi-bin/viewcvs.cgi/jsr166/jsr166/src/jdk8/java/util/)

You may be able to use these by running java8 with the option -Xbootclasspath/p:jsr166-4jdk8.jar (You may need to precede "jsr166-4jdk8.jar" with its full file path.)

___

## Package jsr166e

New classes, located in java package jsr166e, that may appear in JDK8, but are usable in JDK6 - they require only Java6 to compile and run. The subpackage jsr166e.extra includes classes relying on jsr166e that are not currently targeted for package java.util.concurrent in JDK8 but are released separately.

-   Browsable CVS sources: [http://gee.cs.oswego.edu/cgi-bin/viewcvs.cgi/jsr166/jsr166/src/jsr166e/](http://gee.cs.oswego.edu/cgi-bin/viewcvs.cgi/jsr166/jsr166/src/jsr166e/)

___

## Repository jsr166-4jdk7

Updated versions of classes originally incorporated in JDK7. These may include updated algorithms within existing classes, as well as minor extensions that ultimately ended up in JDK8 versions, but do not rely on any other JDK8 features.

-   Browsable CVS sources: [http://gee.cs.oswego.edu/cgi-bin/viewcvs.cgi/jsr166/jsr166/src/jdk7/java/util/](http://gee.cs.oswego.edu/cgi-bin/viewcvs.cgi/jsr166/jsr166/src/jdk7/java/util/)

___

## Package jsr166y

Classes, located in java package jsr166y, targeted to appear in JDK7 but usable in JDK6. These require Java6 to compile and run.

-   Browsable CVS sources: [http://gee.cs.oswego.edu/cgi-bin/viewcvs.cgi/jsr166/jsr166/src/jsr166y/](http://gee.cs.oswego.edu/cgi-bin/viewcvs.cgi/jsr166/jsr166/src/jsr166y/)

___

## Package extra166y

Preliminary versions of classes that build upon those targeted for JDK7, but are not planned for inclusion. These require Java6 to compile and run.

-   Browsable CVS sources: [http://gee.cs.oswego.edu/cgi-bin/viewcvs.cgi/jsr166/jsr166/src/extra166y/](http://gee.cs.oswego.edu/cgi-bin/viewcvs.cgi/jsr166/jsr166/src/extra166y/)
-   Browsable CVS test file sources: [http://gee.cs.oswego.edu/cgi-bin/viewcvs.cgi/jsr166/jsr166/src/test/extra166y/](http://gee.cs.oswego.edu/cgi-bin/viewcvs.cgi/jsr166/jsr166/src/test/extra166y/)

___

## Package jsr166x

Preliminary versions of Deques (double-ended queues) and Navigable collections (concurrent sorted maps and sets) that appear in JDK6 are separately available as package jsr166x, which can be used with any Java5 JVM:

-   Browsable CVS sources: [http://gee.cs.oswego.edu/cgi-bin/viewcvs.cgi/jsr166/jsr166/src/jsr166x/](http://gee.cs.oswego.edu/cgi-bin/viewcvs.cgi/jsr166/jsr166/src/jsr166x/)

___

## Related information

For the official JSR166 proposal see [the JCP web site](http://www.jcp.org/jsr/detail/166.jsp).

The initial contents of JSR166 were released as part of [JDK 5](http://www.jcp.org/en/jsr/detail?id=176), mostly in new package java.util.concurrent.

Sources for all classes originated by the JSR166 group are released to the public domain, as described at [http://creativecommons.org/licenses/publicdomain](http://creativecommons.org/licenses/publicdomain). This includes all code in java.util.concurrent and its subpackages (except CopyOnWriteArrayList), as well as java.util classes Deque, NavigableMap, NavigableSet, Queue, AbstractQueue, and ArrayDeque. Additionally, the JSR166 effort included modifications of [openjdk](http://openjdk.java.net/) versions of a few other java.util classes including TreeMap, AbstractMap, LinkedList, and Collections, which carry [GPL+Classpath exception](http://openjdk.java.net/legal/gplv2+ce.html) licenses.

Many of our internal functionality and performance tests are also in the CVS repository, at [http://gee.cs.oswego.edu/cgi-bin/viewcvs.cgi/jsr166/jsr166/src/test/loops/](http://gee.cs.oswego.edu/cgi-bin/viewcvs.cgi/jsr166/jsr166/src/test/loops/). Other (overlapping) sets of tests designed for use in openjdk testing may be found in [http://gee.cs.oswego.edu/cgi-bin/viewcvs.cgi/jsr166/jsr166/src/test/tck/](http://gee.cs.oswego.edu/cgi-bin/viewcvs.cgi/jsr166/jsr166/src/test/tck/) and [http://gee.cs.oswego.edu/cgi-bin/viewcvs.cgi/jsr166/jsr166/src/test/jtreg/util/](http://gee.cs.oswego.edu/cgi-bin/viewcvs.cgi/jsr166/jsr166/src/test/jtreg/util/). Some are poorly documented, and some target only specific functionality or performance issues so may not be useful indicators. (A unix "runscript" is available to run most in "loops"). Still they may be useful for tracking changes, comparing VMs and VM options, or exploring alternative implementations.

The book [Java Concurrency in Practice](http://www.awprofessional.com/title/0321349601) by Brian Goetz, with Tim Peierls, Joshua Bloch, Joseph Bowbeer, David Holmes, Doug Lea. Addison-Wesley, 2006 describes use of java.util.concurrent.

Slides from the JavaOne Collections Connection BoF ([PDF](https://gee.cs.oswego.edu/dl/concurrency-interest/Collections-BOF-2006.pdf)) ([PPT](https://gee.cs.oswego.edu/dl/concurrency-interest/Collections-BOF-2006.ppt)) describe JDK 5 and JDK 6 collections features, including concurrent collections.

There is a [case study report about the JCP process used in JSR166.](http://www.jcp.org/en/resources/guide/166-casestudy)

There is a [technical paper on the JSR166 synchronizer framework](http://gee.cs.oswego.edu/dl/papers/aqs.pdf) appearing in the [2004 PODC CSJP Workshop](http://www.podc.org/html/podc2004/).

Expert group member Brian Goetz has written several on-line articles about java.util.concurrent features, as listed at his [publications page](http://www.briangoetz.com/pubs.html).

There are some out-of-date but mostly still accurate [slides](https://gee.cs.oswego.edu/dl/concurrency-interest/jsr166-slides.pdf) on highlights of JSR-166.

The initial contributing members of the JSR166 expert group are Josh Bloch, Joe Bowbeer, Brian Goetz, David Holmes, Doug Lea (spec lead), and Tim Peierls. Martin Buchholz and Bill Scherer joined as members in maintenance and extension efforts. Others contributing notable guidance and expertise include Dave Dice (Sun), Cliff Click (Azul), Steve Dever (Sun), and Bill Pugh (UMd). Thanks also to the many other people contributing ideas, reviewing APIs and code, and testing out pre-releases.

___

[Doug Lea](http://gee.cs.oswego.edu/dl/)
