# Makefile for program source directory in GNU NLS utilities package.
# Copyright (C) 1995, 1996, 1997 by Ulrich Drepper <drepper@gnu.ai.mit.edu>
# Copyright (C) 2001-2018 Free Software Foundation, Inc.
#
# This file may be copied and used freely without restrictions.  It can
# be used in projects which are not available under the GNU Public License
# but which still want to provide support for the GNU gettext functionality.
# Please note that the actual code is *not* freely available.

PACKAGE = bfd
VERSION = 2.30

SHELL = /bin/sh


srcdir = /Users/dengyouxu/Developer/Software/openjdk8u/jdk8u-xcode10/jdk8u-dev/hotspot/src/share/tools/binutils/binutils-2.30/bfd/po
top_srcdir = /Users/dengyouxu/Developer/Software/openjdk8u/jdk8u-xcode10/jdk8u-dev/hotspot/src/share/tools/binutils/binutils-2.30/bfd
VPATH = /Users/dengyouxu/Developer/Software/openjdk8u/jdk8u-xcode10/jdk8u-dev/hotspot/src/share/tools/binutils/binutils-2.30/bfd/po
top_builddir = ..

prefix = /usr/local
exec_prefix = ${prefix}
datadir = ${datarootdir}
localedir = ${datarootdir}/locale
gnulocaledir = ${prefix}/share/locale
gettextsrcdir = ${prefix}/share/gettext/po
subdir = po

DESTDIR =

INSTALL = /usr/local/bin/ginstall -c
INSTALL_DATA = ${INSTALL} -m 644
MKINSTALLDIRS = /Users/dengyouxu/Developer/Software/openjdk8u/jdk8u-xcode10/jdk8u-dev/hotspot/src/share/tools/binutils/binutils-2.30/bfd/../mkinstalldirs

CC = gcc
GENCAT = 
GMSGFMT = PATH=../src:$$PATH /opt/local/bin/msgfmt
MSGFMT = /opt/local/bin/msgfmt
XGETTEXT = PATH=../src:$$PATH /opt/local/bin/xgettext
MSGMERGE = PATH=../src:$$PATH msgmerge

DEFS = -DHAVE_CONFIG_H
CFLAGS = -fPIC -O
CPPFLAGS = 

INCLUDES = -I.. -I$(top_srcdir)/intl

COMPILE = $(CC) -c $(DEFS) $(INCLUDES) $(CPPFLAGS) $(CFLAGS) $(XCFLAGS)

SOURCES = cat-id-tbl.c
POFILES =  $(srcdir)/da.po $(srcdir)/es.po $(srcdir)/fi.po $(srcdir)/fr.po $(srcdir)/hr.po $(srcdir)/id.po $(srcdir)/ja.po $(srcdir)/ro.po $(srcdir)/ru.po $(srcdir)/rw.po $(srcdir)/sr.po $(srcdir)/sv.po $(srcdir)/tr.po $(srcdir)/uk.po $(srcdir)/vi.po $(srcdir)/zh_CN.po
GMOFILES =  $(srcdir)/da.gmo $(srcdir)/es.gmo $(srcdir)/fi.gmo $(srcdir)/fr.gmo $(srcdir)/hr.gmo $(srcdir)/id.gmo $(srcdir)/ja.gmo $(srcdir)/ro.gmo $(srcdir)/ru.gmo $(srcdir)/rw.gmo $(srcdir)/sr.gmo $(srcdir)/sv.gmo $(srcdir)/tr.gmo $(srcdir)/uk.gmo $(srcdir)/vi.gmo $(srcdir)/zh_CN.gmo
DISTFILES = ChangeLog Makefile.in.in SRC-POTFILES.in BLD-POTFILES.in $(PACKAGE).pot \
stamp-cat-id $(POFILES) $(GMOFILES) $(SOURCES)

# Note - the following line gets processed by bfd/configure and amended
# to contain the full list of source dir POTFILES.
SRC-POTFILES = \
	/Users/dengyouxu/Developer/Software/openjdk8u/jdk8u-xcode10/jdk8u-dev/hotspot/src/share/tools/binutils/binutils-2.30/bfd/aix386-core.c \
	/Users/dengyouxu/Developer/Software/openjdk8u/jdk8u-xcode10/jdk8u-dev/hotspot/src/share/tools/binutils/binutils-2.30/bfd/aix5ppc-core.c \
	/Users/dengyouxu/Developer/Software/openjdk8u/jdk8u-xcode10/jdk8u-dev/hotspot/src/share/tools/binutils/binutils-2.30/bfd/aout-adobe.c \
	/Users/dengyouxu/Developer/Software/openjdk8u/jdk8u-xcode10/jdk8u-dev/hotspot/src/share/tools/binutils/binutils-2.30/bfd/aout-arm.c \
	/Users/dengyouxu/Developer/Software/openjdk8u/jdk8u-xcode10/jdk8u-dev/hotspot/src/share/tools/binutils/binutils-2.30/bfd/aout-cris.c \
	/Users/dengyouxu/Developer/Software/openjdk8u/jdk8u-xcode10/jdk8u-dev/hotspot/src/share/tools/binutils/binutils-2.30/bfd/aout-ns32k.c \
	/Users/dengyouxu/Developer/Software/openjdk8u/jdk8u-xcode10/jdk8u-dev/hotspot/src/share/tools/binutils/binutils-2.30/bfd/aout-sparcle.c \
	/Users/dengyouxu/Developer/Software/openjdk8u/jdk8u-xcode10/jdk8u-dev/hotspot/src/share/tools/binutils/binutils-2.30/bfd/aout-target.h \
	/Users/dengyouxu/Developer/Software/openjdk8u/jdk8u-xcode10/jdk8u-dev/hotspot/src/share/tools/binutils/binutils-2.30/bfd/aout-tic30.c \
	/Users/dengyouxu/Developer/Software/openjdk8u/jdk8u-xcode10/jdk8u-dev/hotspot/src/share/tools/binutils/binutils-2.30/bfd/aout0.c \
	/Users/dengyouxu/Developer/Software/openjdk8u/jdk8u-xcode10/jdk8u-dev/hotspot/src/share/tools/binutils/binutils-2.30/bfd/aout32.c \
	/Users/dengyouxu/Developer/Software/openjdk8u/jdk8u-xcode10/jdk8u-dev/hotspot/src/share/tools/binutils/binutils-2.30/bfd/aout64.c \
	/Users/dengyouxu/Developer/Software/openjdk8u/jdk8u-xcode10/jdk8u-dev/hotspot/src/share/tools/binutils/binutils-2.30/bfd/aoutf1.h \
	/Users/dengyouxu/Developer/Software/openjdk8u/jdk8u-xcode10/jdk8u-dev/hotspot/src/share/tools/binutils/binutils-2.30/bfd/aoutx.h \
	/Users/dengyouxu/Developer/Software/openjdk8u/jdk8u-xcode10/jdk8u-dev/hotspot/src/share/tools/binutils/binutils-2.30/bfd/archive.c \
	/Users/dengyouxu/Developer/Software/openjdk8u/jdk8u-xcode10/jdk8u-dev/hotspot/src/share/tools/binutils/binutils-2.30/bfd/archive64.c \
	/Users/dengyouxu/Developer/Software/openjdk8u/jdk8u-xcode10/jdk8u-dev/hotspot/src/share/tools/binutils/binutils-2.30/bfd/archures.c \
	/Users/dengyouxu/Developer/Software/openjdk8u/jdk8u-xcode10/jdk8u-dev/hotspot/src/share/tools/binutils/binutils-2.30/bfd/armnetbsd.c \
	/Users/dengyouxu/Developer/Software/openjdk8u/jdk8u-xcode10/jdk8u-dev/hotspot/src/share/tools/binutils/binutils-2.30/bfd/bfd.c \
	/Users/dengyouxu/Developer/Software/openjdk8u/jdk8u-xcode10/jdk8u-dev/hotspot/src/share/tools/binutils/binutils-2.30/bfd/bfdio.c \
	/Users/dengyouxu/Developer/Software/openjdk8u/jdk8u-xcode10/jdk8u-dev/hotspot/src/share/tools/binutils/binutils-2.30/bfd/bfdwin.c \
	/Users/dengyouxu/Developer/Software/openjdk8u/jdk8u-xcode10/jdk8u-dev/hotspot/src/share/tools/binutils/binutils-2.30/bfd/binary.c \
	/Users/dengyouxu/Developer/Software/openjdk8u/jdk8u-xcode10/jdk8u-dev/hotspot/src/share/tools/binutils/binutils-2.30/bfd/bout.c \
	/Users/dengyouxu/Developer/Software/openjdk8u/jdk8u-xcode10/jdk8u-dev/hotspot/src/share/tools/binutils/binutils-2.30/bfd/cache.c \
	/Users/dengyouxu/Developer/Software/openjdk8u/jdk8u-xcode10/jdk8u-dev/hotspot/src/share/tools/binutils/binutils-2.30/bfd/cf-i386lynx.c \
	/Users/dengyouxu/Developer/Software/openjdk8u/jdk8u-xcode10/jdk8u-dev/hotspot/src/share/tools/binutils/binutils-2.30/bfd/cf-sparclynx.c \
	/Users/dengyouxu/Developer/Software/openjdk8u/jdk8u-xcode10/jdk8u-dev/hotspot/src/share/tools/binutils/binutils-2.30/bfd/cisco-core.c \
	/Users/dengyouxu/Developer/Software/openjdk8u/jdk8u-xcode10/jdk8u-dev/hotspot/src/share/tools/binutils/binutils-2.30/bfd/coff-alpha.c \
	/Users/dengyouxu/Developer/Software/openjdk8u/jdk8u-xcode10/jdk8u-dev/hotspot/src/share/tools/binutils/binutils-2.30/bfd/coff-apollo.c \
	/Users/dengyouxu/Developer/Software/openjdk8u/jdk8u-xcode10/jdk8u-dev/hotspot/src/share/tools/binutils/binutils-2.30/bfd/coff-arm.c \
	/Users/dengyouxu/Developer/Software/openjdk8u/jdk8u-xcode10/jdk8u-dev/hotspot/src/share/tools/binutils/binutils-2.30/bfd/coff-aux.c \
	/Users/dengyouxu/Developer/Software/openjdk8u/jdk8u-xcode10/jdk8u-dev/hotspot/src/share/tools/binutils/binutils-2.30/bfd/coff-bfd.c \
	/Users/dengyouxu/Developer/Software/openjdk8u/jdk8u-xcode10/jdk8u-dev/hotspot/src/share/tools/binutils/binutils-2.30/bfd/coff-go32.c \
	/Users/dengyouxu/Developer/Software/openjdk8u/jdk8u-xcode10/jdk8u-dev/hotspot/src/share/tools/binutils/binutils-2.30/bfd/coff-h8300.c \
	/Users/dengyouxu/Developer/Software/openjdk8u/jdk8u-xcode10/jdk8u-dev/hotspot/src/share/tools/binutils/binutils-2.30/bfd/coff-h8500.c \
	/Users/dengyouxu/Developer/Software/openjdk8u/jdk8u-xcode10/jdk8u-dev/hotspot/src/share/tools/binutils/binutils-2.30/bfd/coff-i386.c \
	/Users/dengyouxu/Developer/Software/openjdk8u/jdk8u-xcode10/jdk8u-dev/hotspot/src/share/tools/binutils/binutils-2.30/bfd/coff-i860.c \
	/Users/dengyouxu/Developer/Software/openjdk8u/jdk8u-xcode10/jdk8u-dev/hotspot/src/share/tools/binutils/binutils-2.30/bfd/coff-i960.c \
	/Users/dengyouxu/Developer/Software/openjdk8u/jdk8u-xcode10/jdk8u-dev/hotspot/src/share/tools/binutils/binutils-2.30/bfd/coff-m68k.c \
	/Users/dengyouxu/Developer/Software/openjdk8u/jdk8u-xcode10/jdk8u-dev/hotspot/src/share/tools/binutils/binutils-2.30/bfd/coff-m88k.c \
	/Users/dengyouxu/Developer/Software/openjdk8u/jdk8u-xcode10/jdk8u-dev/hotspot/src/share/tools/binutils/binutils-2.30/bfd/coff-mips.c \
	/Users/dengyouxu/Developer/Software/openjdk8u/jdk8u-xcode10/jdk8u-dev/hotspot/src/share/tools/binutils/binutils-2.30/bfd/coff-rs6000.c \
	/Users/dengyouxu/Developer/Software/openjdk8u/jdk8u-xcode10/jdk8u-dev/hotspot/src/share/tools/binutils/binutils-2.30/bfd/coff-sh.c \
	/Users/dengyouxu/Developer/Software/openjdk8u/jdk8u-xcode10/jdk8u-dev/hotspot/src/share/tools/binutils/binutils-2.30/bfd/coff-sparc.c \
	/Users/dengyouxu/Developer/Software/openjdk8u/jdk8u-xcode10/jdk8u-dev/hotspot/src/share/tools/binutils/binutils-2.30/bfd/coff-stgo32.c \
	/Users/dengyouxu/Developer/Software/openjdk8u/jdk8u-xcode10/jdk8u-dev/hotspot/src/share/tools/binutils/binutils-2.30/bfd/coff-svm68k.c \
	/Users/dengyouxu/Developer/Software/openjdk8u/jdk8u-xcode10/jdk8u-dev/hotspot/src/share/tools/binutils/binutils-2.30/bfd/coff-tic30.c \
	/Users/dengyouxu/Developer/Software/openjdk8u/jdk8u-xcode10/jdk8u-dev/hotspot/src/share/tools/binutils/binutils-2.30/bfd/coff-tic4x.c \
	/Users/dengyouxu/Developer/Software/openjdk8u/jdk8u-xcode10/jdk8u-dev/hotspot/src/share/tools/binutils/binutils-2.30/bfd/coff-tic54x.c \
	/Users/dengyouxu/Developer/Software/openjdk8u/jdk8u-xcode10/jdk8u-dev/hotspot/src/share/tools/binutils/binutils-2.30/bfd/coff-tic80.c \
	/Users/dengyouxu/Developer/Software/openjdk8u/jdk8u-xcode10/jdk8u-dev/hotspot/src/share/tools/binutils/binutils-2.30/bfd/coff-u68k.c \
	/Users/dengyouxu/Developer/Software/openjdk8u/jdk8u-xcode10/jdk8u-dev/hotspot/src/share/tools/binutils/binutils-2.30/bfd/coff-w65.c \
	/Users/dengyouxu/Developer/Software/openjdk8u/jdk8u-xcode10/jdk8u-dev/hotspot/src/share/tools/binutils/binutils-2.30/bfd/coff-we32k.c \
	/Users/dengyouxu/Developer/Software/openjdk8u/jdk8u-xcode10/jdk8u-dev/hotspot/src/share/tools/binutils/binutils-2.30/bfd/coff-x86_64.c \
	/Users/dengyouxu/Developer/Software/openjdk8u/jdk8u-xcode10/jdk8u-dev/hotspot/src/share/tools/binutils/binutils-2.30/bfd/coff-z80.c \
	/Users/dengyouxu/Developer/Software/openjdk8u/jdk8u-xcode10/jdk8u-dev/hotspot/src/share/tools/binutils/binutils-2.30/bfd/coff-z8k.c \
	/Users/dengyouxu/Developer/Software/openjdk8u/jdk8u-xcode10/jdk8u-dev/hotspot/src/share/tools/binutils/binutils-2.30/bfd/coff64-rs6000.c \
	/Users/dengyouxu/Developer/Software/openjdk8u/jdk8u-xcode10/jdk8u-dev/hotspot/src/share/tools/binutils/binutils-2.30/bfd/coffcode.h \
	/Users/dengyouxu/Developer/Software/openjdk8u/jdk8u-xcode10/jdk8u-dev/hotspot/src/share/tools/binutils/binutils-2.30/bfd/coffgen.c \
	/Users/dengyouxu/Developer/Software/openjdk8u/jdk8u-xcode10/jdk8u-dev/hotspot/src/share/tools/binutils/binutils-2.30/bfd/cofflink.c \
	/Users/dengyouxu/Developer/Software/openjdk8u/jdk8u-xcode10/jdk8u-dev/hotspot/src/share/tools/binutils/binutils-2.30/bfd/coffswap.h \
	/Users/dengyouxu/Developer/Software/openjdk8u/jdk8u-xcode10/jdk8u-dev/hotspot/src/share/tools/binutils/binutils-2.30/bfd/compress.c \
	/Users/dengyouxu/Developer/Software/openjdk8u/jdk8u-xcode10/jdk8u-dev/hotspot/src/share/tools/binutils/binutils-2.30/bfd/corefile.c \
	/Users/dengyouxu/Developer/Software/openjdk8u/jdk8u-xcode10/jdk8u-dev/hotspot/src/share/tools/binutils/binutils-2.30/bfd/cpu-aarch64.c \
	/Users/dengyouxu/Developer/Software/openjdk8u/jdk8u-xcode10/jdk8u-dev/hotspot/src/share/tools/binutils/binutils-2.30/bfd/cpu-alpha.c \
	/Users/dengyouxu/Developer/Software/openjdk8u/jdk8u-xcode10/jdk8u-dev/hotspot/src/share/tools/binutils/binutils-2.30/bfd/cpu-arc.c \
	/Users/dengyouxu/Developer/Software/openjdk8u/jdk8u-xcode10/jdk8u-dev/hotspot/src/share/tools/binutils/binutils-2.30/bfd/cpu-arm.c \
	/Users/dengyouxu/Developer/Software/openjdk8u/jdk8u-xcode10/jdk8u-dev/hotspot/src/share/tools/binutils/binutils-2.30/bfd/cpu-avr.c \
	/Users/dengyouxu/Developer/Software/openjdk8u/jdk8u-xcode10/jdk8u-dev/hotspot/src/share/tools/binutils/binutils-2.30/bfd/cpu-bfin.c \
	/Users/dengyouxu/Developer/Software/openjdk8u/jdk8u-xcode10/jdk8u-dev/hotspot/src/share/tools/binutils/binutils-2.30/bfd/cpu-cr16.c \
	/Users/dengyouxu/Developer/Software/openjdk8u/jdk8u-xcode10/jdk8u-dev/hotspot/src/share/tools/binutils/binutils-2.30/bfd/cpu-cr16c.c \
	/Users/dengyouxu/Developer/Software/openjdk8u/jdk8u-xcode10/jdk8u-dev/hotspot/src/share/tools/binutils/binutils-2.30/bfd/cpu-cris.c \
	/Users/dengyouxu/Developer/Software/openjdk8u/jdk8u-xcode10/jdk8u-dev/hotspot/src/share/tools/binutils/binutils-2.30/bfd/cpu-crx.c \
	/Users/dengyouxu/Developer/Software/openjdk8u/jdk8u-xcode10/jdk8u-dev/hotspot/src/share/tools/binutils/binutils-2.30/bfd/cpu-d10v.c \
	/Users/dengyouxu/Developer/Software/openjdk8u/jdk8u-xcode10/jdk8u-dev/hotspot/src/share/tools/binutils/binutils-2.30/bfd/cpu-d30v.c \
	/Users/dengyouxu/Developer/Software/openjdk8u/jdk8u-xcode10/jdk8u-dev/hotspot/src/share/tools/binutils/binutils-2.30/bfd/cpu-dlx.c \
	/Users/dengyouxu/Developer/Software/openjdk8u/jdk8u-xcode10/jdk8u-dev/hotspot/src/share/tools/binutils/binutils-2.30/bfd/cpu-epiphany.c \
	/Users/dengyouxu/Developer/Software/openjdk8u/jdk8u-xcode10/jdk8u-dev/hotspot/src/share/tools/binutils/binutils-2.30/bfd/cpu-fr30.c \
	/Users/dengyouxu/Developer/Software/openjdk8u/jdk8u-xcode10/jdk8u-dev/hotspot/src/share/tools/binutils/binutils-2.30/bfd/cpu-frv.c \
	/Users/dengyouxu/Developer/Software/openjdk8u/jdk8u-xcode10/jdk8u-dev/hotspot/src/share/tools/binutils/binutils-2.30/bfd/cpu-ft32.c \
	/Users/dengyouxu/Developer/Software/openjdk8u/jdk8u-xcode10/jdk8u-dev/hotspot/src/share/tools/binutils/binutils-2.30/bfd/cpu-h8300.c \
	/Users/dengyouxu/Developer/Software/openjdk8u/jdk8u-xcode10/jdk8u-dev/hotspot/src/share/tools/binutils/binutils-2.30/bfd/cpu-h8500.c \
	/Users/dengyouxu/Developer/Software/openjdk8u/jdk8u-xcode10/jdk8u-dev/hotspot/src/share/tools/binutils/binutils-2.30/bfd/cpu-hppa.c \
	/Users/dengyouxu/Developer/Software/openjdk8u/jdk8u-xcode10/jdk8u-dev/hotspot/src/share/tools/binutils/binutils-2.30/bfd/cpu-i370.c \
	/Users/dengyouxu/Developer/Software/openjdk8u/jdk8u-xcode10/jdk8u-dev/hotspot/src/share/tools/binutils/binutils-2.30/bfd/cpu-i386.c \
	/Users/dengyouxu/Developer/Software/openjdk8u/jdk8u-xcode10/jdk8u-dev/hotspot/src/share/tools/binutils/binutils-2.30/bfd/cpu-i860.c \
	/Users/dengyouxu/Developer/Software/openjdk8u/jdk8u-xcode10/jdk8u-dev/hotspot/src/share/tools/binutils/binutils-2.30/bfd/cpu-i960.c \
	/Users/dengyouxu/Developer/Software/openjdk8u/jdk8u-xcode10/jdk8u-dev/hotspot/src/share/tools/binutils/binutils-2.30/bfd/cpu-ia64.c \
	/Users/dengyouxu/Developer/Software/openjdk8u/jdk8u-xcode10/jdk8u-dev/hotspot/src/share/tools/binutils/binutils-2.30/bfd/cpu-iamcu.c \
	/Users/dengyouxu/Developer/Software/openjdk8u/jdk8u-xcode10/jdk8u-dev/hotspot/src/share/tools/binutils/binutils-2.30/bfd/cpu-ip2k.c \
	/Users/dengyouxu/Developer/Software/openjdk8u/jdk8u-xcode10/jdk8u-dev/hotspot/src/share/tools/binutils/binutils-2.30/bfd/cpu-iq2000.c \
	/Users/dengyouxu/Developer/Software/openjdk8u/jdk8u-xcode10/jdk8u-dev/hotspot/src/share/tools/binutils/binutils-2.30/bfd/cpu-k1om.c \
	/Users/dengyouxu/Developer/Software/openjdk8u/jdk8u-xcode10/jdk8u-dev/hotspot/src/share/tools/binutils/binutils-2.30/bfd/cpu-l1om.c \
	/Users/dengyouxu/Developer/Software/openjdk8u/jdk8u-xcode10/jdk8u-dev/hotspot/src/share/tools/binutils/binutils-2.30/bfd/cpu-lm32.c \
	/Users/dengyouxu/Developer/Software/openjdk8u/jdk8u-xcode10/jdk8u-dev/hotspot/src/share/tools/binutils/binutils-2.30/bfd/cpu-m10200.c \
	/Users/dengyouxu/Developer/Software/openjdk8u/jdk8u-xcode10/jdk8u-dev/hotspot/src/share/tools/binutils/binutils-2.30/bfd/cpu-m10300.c \
	/Users/dengyouxu/Developer/Software/openjdk8u/jdk8u-xcode10/jdk8u-dev/hotspot/src/share/tools/binutils/binutils-2.30/bfd/cpu-m32c.c \
	/Users/dengyouxu/Developer/Software/openjdk8u/jdk8u-xcode10/jdk8u-dev/hotspot/src/share/tools/binutils/binutils-2.30/bfd/cpu-m32r.c \
	/Users/dengyouxu/Developer/Software/openjdk8u/jdk8u-xcode10/jdk8u-dev/hotspot/src/share/tools/binutils/binutils-2.30/bfd/cpu-m68hc11.c \
	/Users/dengyouxu/Developer/Software/openjdk8u/jdk8u-xcode10/jdk8u-dev/hotspot/src/share/tools/binutils/binutils-2.30/bfd/cpu-m68hc12.c \
	/Users/dengyouxu/Developer/Software/openjdk8u/jdk8u-xcode10/jdk8u-dev/hotspot/src/share/tools/binutils/binutils-2.30/bfd/cpu-m68k.c \
	/Users/dengyouxu/Developer/Software/openjdk8u/jdk8u-xcode10/jdk8u-dev/hotspot/src/share/tools/binutils/binutils-2.30/bfd/cpu-m88k.c \
	/Users/dengyouxu/Developer/Software/openjdk8u/jdk8u-xcode10/jdk8u-dev/hotspot/src/share/tools/binutils/binutils-2.30/bfd/cpu-m9s12x.c \
	/Users/dengyouxu/Developer/Software/openjdk8u/jdk8u-xcode10/jdk8u-dev/hotspot/src/share/tools/binutils/binutils-2.30/bfd/cpu-m9s12xg.c \
	/Users/dengyouxu/Developer/Software/openjdk8u/jdk8u-xcode10/jdk8u-dev/hotspot/src/share/tools/binutils/binutils-2.30/bfd/cpu-mcore.c \
	/Users/dengyouxu/Developer/Software/openjdk8u/jdk8u-xcode10/jdk8u-dev/hotspot/src/share/tools/binutils/binutils-2.30/bfd/cpu-mep.c \
	/Users/dengyouxu/Developer/Software/openjdk8u/jdk8u-xcode10/jdk8u-dev/hotspot/src/share/tools/binutils/binutils-2.30/bfd/cpu-metag.c \
	/Users/dengyouxu/Developer/Software/openjdk8u/jdk8u-xcode10/jdk8u-dev/hotspot/src/share/tools/binutils/binutils-2.30/bfd/cpu-microblaze.c \
	/Users/dengyouxu/Developer/Software/openjdk8u/jdk8u-xcode10/jdk8u-dev/hotspot/src/share/tools/binutils/binutils-2.30/bfd/cpu-mips.c \
	/Users/dengyouxu/Developer/Software/openjdk8u/jdk8u-xcode10/jdk8u-dev/hotspot/src/share/tools/binutils/binutils-2.30/bfd/cpu-mmix.c \
	/Users/dengyouxu/Developer/Software/openjdk8u/jdk8u-xcode10/jdk8u-dev/hotspot/src/share/tools/binutils/binutils-2.30/bfd/cpu-moxie.c \
	/Users/dengyouxu/Developer/Software/openjdk8u/jdk8u-xcode10/jdk8u-dev/hotspot/src/share/tools/binutils/binutils-2.30/bfd/cpu-msp430.c \
	/Users/dengyouxu/Developer/Software/openjdk8u/jdk8u-xcode10/jdk8u-dev/hotspot/src/share/tools/binutils/binutils-2.30/bfd/cpu-mt.c \
	/Users/dengyouxu/Developer/Software/openjdk8u/jdk8u-xcode10/jdk8u-dev/hotspot/src/share/tools/binutils/binutils-2.30/bfd/cpu-nds32.c \
	/Users/dengyouxu/Developer/Software/openjdk8u/jdk8u-xcode10/jdk8u-dev/hotspot/src/share/tools/binutils/binutils-2.30/bfd/cpu-nios2.c \
	/Users/dengyouxu/Developer/Software/openjdk8u/jdk8u-xcode10/jdk8u-dev/hotspot/src/share/tools/binutils/binutils-2.30/bfd/cpu-ns32k.c \
	/Users/dengyouxu/Developer/Software/openjdk8u/jdk8u-xcode10/jdk8u-dev/hotspot/src/share/tools/binutils/binutils-2.30/bfd/cpu-or1k.c \
	/Users/dengyouxu/Developer/Software/openjdk8u/jdk8u-xcode10/jdk8u-dev/hotspot/src/share/tools/binutils/binutils-2.30/bfd/cpu-pdp11.c \
	/Users/dengyouxu/Developer/Software/openjdk8u/jdk8u-xcode10/jdk8u-dev/hotspot/src/share/tools/binutils/binutils-2.30/bfd/cpu-pj.c \
	/Users/dengyouxu/Developer/Software/openjdk8u/jdk8u-xcode10/jdk8u-dev/hotspot/src/share/tools/binutils/binutils-2.30/bfd/cpu-plugin.c \
	/Users/dengyouxu/Developer/Software/openjdk8u/jdk8u-xcode10/jdk8u-dev/hotspot/src/share/tools/binutils/binutils-2.30/bfd/cpu-powerpc.c \
	/Users/dengyouxu/Developer/Software/openjdk8u/jdk8u-xcode10/jdk8u-dev/hotspot/src/share/tools/binutils/binutils-2.30/bfd/cpu-pru.c \
	/Users/dengyouxu/Developer/Software/openjdk8u/jdk8u-xcode10/jdk8u-dev/hotspot/src/share/tools/binutils/binutils-2.30/bfd/cpu-riscv.c \
	/Users/dengyouxu/Developer/Software/openjdk8u/jdk8u-xcode10/jdk8u-dev/hotspot/src/share/tools/binutils/binutils-2.30/bfd/cpu-rl78.c \
	/Users/dengyouxu/Developer/Software/openjdk8u/jdk8u-xcode10/jdk8u-dev/hotspot/src/share/tools/binutils/binutils-2.30/bfd/cpu-rs6000.c \
	/Users/dengyouxu/Developer/Software/openjdk8u/jdk8u-xcode10/jdk8u-dev/hotspot/src/share/tools/binutils/binutils-2.30/bfd/cpu-rx.c \
	/Users/dengyouxu/Developer/Software/openjdk8u/jdk8u-xcode10/jdk8u-dev/hotspot/src/share/tools/binutils/binutils-2.30/bfd/cpu-s390.c \
	/Users/dengyouxu/Developer/Software/openjdk8u/jdk8u-xcode10/jdk8u-dev/hotspot/src/share/tools/binutils/binutils-2.30/bfd/cpu-score.c \
	/Users/dengyouxu/Developer/Software/openjdk8u/jdk8u-xcode10/jdk8u-dev/hotspot/src/share/tools/binutils/binutils-2.30/bfd/cpu-sh.c \
	/Users/dengyouxu/Developer/Software/openjdk8u/jdk8u-xcode10/jdk8u-dev/hotspot/src/share/tools/binutils/binutils-2.30/bfd/cpu-sparc.c \
	/Users/dengyouxu/Developer/Software/openjdk8u/jdk8u-xcode10/jdk8u-dev/hotspot/src/share/tools/binutils/binutils-2.30/bfd/cpu-spu.c \
	/Users/dengyouxu/Developer/Software/openjdk8u/jdk8u-xcode10/jdk8u-dev/hotspot/src/share/tools/binutils/binutils-2.30/bfd/cpu-tic30.c \
	/Users/dengyouxu/Developer/Software/openjdk8u/jdk8u-xcode10/jdk8u-dev/hotspot/src/share/tools/binutils/binutils-2.30/bfd/cpu-tic4x.c \
	/Users/dengyouxu/Developer/Software/openjdk8u/jdk8u-xcode10/jdk8u-dev/hotspot/src/share/tools/binutils/binutils-2.30/bfd/cpu-tic54x.c \
	/Users/dengyouxu/Developer/Software/openjdk8u/jdk8u-xcode10/jdk8u-dev/hotspot/src/share/tools/binutils/binutils-2.30/bfd/cpu-tic6x.c \
	/Users/dengyouxu/Developer/Software/openjdk8u/jdk8u-xcode10/jdk8u-dev/hotspot/src/share/tools/binutils/binutils-2.30/bfd/cpu-tic80.c \
	/Users/dengyouxu/Developer/Software/openjdk8u/jdk8u-xcode10/jdk8u-dev/hotspot/src/share/tools/binutils/binutils-2.30/bfd/cpu-tilegx.c \
	/Users/dengyouxu/Developer/Software/openjdk8u/jdk8u-xcode10/jdk8u-dev/hotspot/src/share/tools/binutils/binutils-2.30/bfd/cpu-tilepro.c \
	/Users/dengyouxu/Developer/Software/openjdk8u/jdk8u-xcode10/jdk8u-dev/hotspot/src/share/tools/binutils/binutils-2.30/bfd/cpu-v850.c \
	/Users/dengyouxu/Developer/Software/openjdk8u/jdk8u-xcode10/jdk8u-dev/hotspot/src/share/tools/binutils/binutils-2.30/bfd/cpu-v850_rh850.c \
	/Users/dengyouxu/Developer/Software/openjdk8u/jdk8u-xcode10/jdk8u-dev/hotspot/src/share/tools/binutils/binutils-2.30/bfd/cpu-vax.c \
	/Users/dengyouxu/Developer/Software/openjdk8u/jdk8u-xcode10/jdk8u-dev/hotspot/src/share/tools/binutils/binutils-2.30/bfd/cpu-visium.c \
	/Users/dengyouxu/Developer/Software/openjdk8u/jdk8u-xcode10/jdk8u-dev/hotspot/src/share/tools/binutils/binutils-2.30/bfd/cpu-w65.c \
	/Users/dengyouxu/Developer/Software/openjdk8u/jdk8u-xcode10/jdk8u-dev/hotspot/src/share/tools/binutils/binutils-2.30/bfd/cpu-wasm32.c \
	/Users/dengyouxu/Developer/Software/openjdk8u/jdk8u-xcode10/jdk8u-dev/hotspot/src/share/tools/binutils/binutils-2.30/bfd/cpu-we32k.c \
	/Users/dengyouxu/Developer/Software/openjdk8u/jdk8u-xcode10/jdk8u-dev/hotspot/src/share/tools/binutils/binutils-2.30/bfd/cpu-xc16x.c \
	/Users/dengyouxu/Developer/Software/openjdk8u/jdk8u-xcode10/jdk8u-dev/hotspot/src/share/tools/binutils/binutils-2.30/bfd/cpu-xgate.c \
	/Users/dengyouxu/Developer/Software/openjdk8u/jdk8u-xcode10/jdk8u-dev/hotspot/src/share/tools/binutils/binutils-2.30/bfd/cpu-xstormy16.c \
	/Users/dengyouxu/Developer/Software/openjdk8u/jdk8u-xcode10/jdk8u-dev/hotspot/src/share/tools/binutils/binutils-2.30/bfd/cpu-xtensa.c \
	/Users/dengyouxu/Developer/Software/openjdk8u/jdk8u-xcode10/jdk8u-dev/hotspot/src/share/tools/binutils/binutils-2.30/bfd/cpu-z80.c \
	/Users/dengyouxu/Developer/Software/openjdk8u/jdk8u-xcode10/jdk8u-dev/hotspot/src/share/tools/binutils/binutils-2.30/bfd/cpu-z8k.c \
	/Users/dengyouxu/Developer/Software/openjdk8u/jdk8u-xcode10/jdk8u-dev/hotspot/src/share/tools/binutils/binutils-2.30/bfd/demo64.c \
	/Users/dengyouxu/Developer/Software/openjdk8u/jdk8u-xcode10/jdk8u-dev/hotspot/src/share/tools/binutils/binutils-2.30/bfd/dwarf1.c \
	/Users/dengyouxu/Developer/Software/openjdk8u/jdk8u-xcode10/jdk8u-dev/hotspot/src/share/tools/binutils/binutils-2.30/bfd/dwarf2.c \
	/Users/dengyouxu/Developer/Software/openjdk8u/jdk8u-xcode10/jdk8u-dev/hotspot/src/share/tools/binutils/binutils-2.30/bfd/ecoff.c \
	/Users/dengyouxu/Developer/Software/openjdk8u/jdk8u-xcode10/jdk8u-dev/hotspot/src/share/tools/binutils/binutils-2.30/bfd/ecofflink.c \
	/Users/dengyouxu/Developer/Software/openjdk8u/jdk8u-xcode10/jdk8u-dev/hotspot/src/share/tools/binutils/binutils-2.30/bfd/ecoffswap.h \
	/Users/dengyouxu/Developer/Software/openjdk8u/jdk8u-xcode10/jdk8u-dev/hotspot/src/share/tools/binutils/binutils-2.30/bfd/elf-attrs.c \
	/Users/dengyouxu/Developer/Software/openjdk8u/jdk8u-xcode10/jdk8u-dev/hotspot/src/share/tools/binutils/binutils-2.30/bfd/elf-bfd.h \
	/Users/dengyouxu/Developer/Software/openjdk8u/jdk8u-xcode10/jdk8u-dev/hotspot/src/share/tools/binutils/binutils-2.30/bfd/elf-eh-frame.c \
	/Users/dengyouxu/Developer/Software/openjdk8u/jdk8u-xcode10/jdk8u-dev/hotspot/src/share/tools/binutils/binutils-2.30/bfd/elf-hppa.h \
	/Users/dengyouxu/Developer/Software/openjdk8u/jdk8u-xcode10/jdk8u-dev/hotspot/src/share/tools/binutils/binutils-2.30/bfd/elf-ifunc.c \
	/Users/dengyouxu/Developer/Software/openjdk8u/jdk8u-xcode10/jdk8u-dev/hotspot/src/share/tools/binutils/binutils-2.30/bfd/elf-linux-core.h \
	/Users/dengyouxu/Developer/Software/openjdk8u/jdk8u-xcode10/jdk8u-dev/hotspot/src/share/tools/binutils/binutils-2.30/bfd/elf-m10200.c \
	/Users/dengyouxu/Developer/Software/openjdk8u/jdk8u-xcode10/jdk8u-dev/hotspot/src/share/tools/binutils/binutils-2.30/bfd/elf-m10300.c \
	/Users/dengyouxu/Developer/Software/openjdk8u/jdk8u-xcode10/jdk8u-dev/hotspot/src/share/tools/binutils/binutils-2.30/bfd/elf-nacl.c \
	/Users/dengyouxu/Developer/Software/openjdk8u/jdk8u-xcode10/jdk8u-dev/hotspot/src/share/tools/binutils/binutils-2.30/bfd/elf-properties.c \
	/Users/dengyouxu/Developer/Software/openjdk8u/jdk8u-xcode10/jdk8u-dev/hotspot/src/share/tools/binutils/binutils-2.30/bfd/elf-strtab.c \
	/Users/dengyouxu/Developer/Software/openjdk8u/jdk8u-xcode10/jdk8u-dev/hotspot/src/share/tools/binutils/binutils-2.30/bfd/elf-vxworks.c \
	/Users/dengyouxu/Developer/Software/openjdk8u/jdk8u-xcode10/jdk8u-dev/hotspot/src/share/tools/binutils/binutils-2.30/bfd/elf.c \
	/Users/dengyouxu/Developer/Software/openjdk8u/jdk8u-xcode10/jdk8u-dev/hotspot/src/share/tools/binutils/binutils-2.30/bfd/elf32-am33lin.c \
	/Users/dengyouxu/Developer/Software/openjdk8u/jdk8u-xcode10/jdk8u-dev/hotspot/src/share/tools/binutils/binutils-2.30/bfd/elf32-arc.c \
	/Users/dengyouxu/Developer/Software/openjdk8u/jdk8u-xcode10/jdk8u-dev/hotspot/src/share/tools/binutils/binutils-2.30/bfd/elf32-arm.c \
	/Users/dengyouxu/Developer/Software/openjdk8u/jdk8u-xcode10/jdk8u-dev/hotspot/src/share/tools/binutils/binutils-2.30/bfd/elf32-avr.c \
	/Users/dengyouxu/Developer/Software/openjdk8u/jdk8u-xcode10/jdk8u-dev/hotspot/src/share/tools/binutils/binutils-2.30/bfd/elf32-bfin.c \
	/Users/dengyouxu/Developer/Software/openjdk8u/jdk8u-xcode10/jdk8u-dev/hotspot/src/share/tools/binutils/binutils-2.30/bfd/elf32-cr16.c \
	/Users/dengyouxu/Developer/Software/openjdk8u/jdk8u-xcode10/jdk8u-dev/hotspot/src/share/tools/binutils/binutils-2.30/bfd/elf32-cr16c.c \
	/Users/dengyouxu/Developer/Software/openjdk8u/jdk8u-xcode10/jdk8u-dev/hotspot/src/share/tools/binutils/binutils-2.30/bfd/elf32-cris.c \
	/Users/dengyouxu/Developer/Software/openjdk8u/jdk8u-xcode10/jdk8u-dev/hotspot/src/share/tools/binutils/binutils-2.30/bfd/elf32-crx.c \
	/Users/dengyouxu/Developer/Software/openjdk8u/jdk8u-xcode10/jdk8u-dev/hotspot/src/share/tools/binutils/binutils-2.30/bfd/elf32-d10v.c \
	/Users/dengyouxu/Developer/Software/openjdk8u/jdk8u-xcode10/jdk8u-dev/hotspot/src/share/tools/binutils/binutils-2.30/bfd/elf32-d30v.c \
	/Users/dengyouxu/Developer/Software/openjdk8u/jdk8u-xcode10/jdk8u-dev/hotspot/src/share/tools/binutils/binutils-2.30/bfd/elf32-dlx.c \
	/Users/dengyouxu/Developer/Software/openjdk8u/jdk8u-xcode10/jdk8u-dev/hotspot/src/share/tools/binutils/binutils-2.30/bfd/elf32-epiphany.c \
	/Users/dengyouxu/Developer/Software/openjdk8u/jdk8u-xcode10/jdk8u-dev/hotspot/src/share/tools/binutils/binutils-2.30/bfd/elf32-fr30.c \
	/Users/dengyouxu/Developer/Software/openjdk8u/jdk8u-xcode10/jdk8u-dev/hotspot/src/share/tools/binutils/binutils-2.30/bfd/elf32-frv.c \
	/Users/dengyouxu/Developer/Software/openjdk8u/jdk8u-xcode10/jdk8u-dev/hotspot/src/share/tools/binutils/binutils-2.30/bfd/elf32-ft32.c \
	/Users/dengyouxu/Developer/Software/openjdk8u/jdk8u-xcode10/jdk8u-dev/hotspot/src/share/tools/binutils/binutils-2.30/bfd/elf32-gen.c \
	/Users/dengyouxu/Developer/Software/openjdk8u/jdk8u-xcode10/jdk8u-dev/hotspot/src/share/tools/binutils/binutils-2.30/bfd/elf32-h8300.c \
	/Users/dengyouxu/Developer/Software/openjdk8u/jdk8u-xcode10/jdk8u-dev/hotspot/src/share/tools/binutils/binutils-2.30/bfd/elf32-hppa.c \
	/Users/dengyouxu/Developer/Software/openjdk8u/jdk8u-xcode10/jdk8u-dev/hotspot/src/share/tools/binutils/binutils-2.30/bfd/elf32-hppa.h \
	/Users/dengyouxu/Developer/Software/openjdk8u/jdk8u-xcode10/jdk8u-dev/hotspot/src/share/tools/binutils/binutils-2.30/bfd/elf32-i370.c \
	/Users/dengyouxu/Developer/Software/openjdk8u/jdk8u-xcode10/jdk8u-dev/hotspot/src/share/tools/binutils/binutils-2.30/bfd/elf32-i386.c \
	/Users/dengyouxu/Developer/Software/openjdk8u/jdk8u-xcode10/jdk8u-dev/hotspot/src/share/tools/binutils/binutils-2.30/bfd/elf32-i860.c \
	/Users/dengyouxu/Developer/Software/openjdk8u/jdk8u-xcode10/jdk8u-dev/hotspot/src/share/tools/binutils/binutils-2.30/bfd/elf32-i960.c \
	/Users/dengyouxu/Developer/Software/openjdk8u/jdk8u-xcode10/jdk8u-dev/hotspot/src/share/tools/binutils/binutils-2.30/bfd/elf32-ip2k.c \
	/Users/dengyouxu/Developer/Software/openjdk8u/jdk8u-xcode10/jdk8u-dev/hotspot/src/share/tools/binutils/binutils-2.30/bfd/elf32-iq2000.c \
	/Users/dengyouxu/Developer/Software/openjdk8u/jdk8u-xcode10/jdk8u-dev/hotspot/src/share/tools/binutils/binutils-2.30/bfd/elf32-lm32.c \
	/Users/dengyouxu/Developer/Software/openjdk8u/jdk8u-xcode10/jdk8u-dev/hotspot/src/share/tools/binutils/binutils-2.30/bfd/elf32-m32c.c \
	/Users/dengyouxu/Developer/Software/openjdk8u/jdk8u-xcode10/jdk8u-dev/hotspot/src/share/tools/binutils/binutils-2.30/bfd/elf32-m32r.c \
	/Users/dengyouxu/Developer/Software/openjdk8u/jdk8u-xcode10/jdk8u-dev/hotspot/src/share/tools/binutils/binutils-2.30/bfd/elf32-m68hc11.c \
	/Users/dengyouxu/Developer/Software/openjdk8u/jdk8u-xcode10/jdk8u-dev/hotspot/src/share/tools/binutils/binutils-2.30/bfd/elf32-m68hc12.c \
	/Users/dengyouxu/Developer/Software/openjdk8u/jdk8u-xcode10/jdk8u-dev/hotspot/src/share/tools/binutils/binutils-2.30/bfd/elf32-m68hc1x.c \
	/Users/dengyouxu/Developer/Software/openjdk8u/jdk8u-xcode10/jdk8u-dev/hotspot/src/share/tools/binutils/binutils-2.30/bfd/elf32-m68k.c \
	/Users/dengyouxu/Developer/Software/openjdk8u/jdk8u-xcode10/jdk8u-dev/hotspot/src/share/tools/binutils/binutils-2.30/bfd/elf32-m88k.c \
	/Users/dengyouxu/Developer/Software/openjdk8u/jdk8u-xcode10/jdk8u-dev/hotspot/src/share/tools/binutils/binutils-2.30/bfd/elf32-mcore.c \
	/Users/dengyouxu/Developer/Software/openjdk8u/jdk8u-xcode10/jdk8u-dev/hotspot/src/share/tools/binutils/binutils-2.30/bfd/elf32-mep.c \
	/Users/dengyouxu/Developer/Software/openjdk8u/jdk8u-xcode10/jdk8u-dev/hotspot/src/share/tools/binutils/binutils-2.30/bfd/elf32-metag.c \
	/Users/dengyouxu/Developer/Software/openjdk8u/jdk8u-xcode10/jdk8u-dev/hotspot/src/share/tools/binutils/binutils-2.30/bfd/elf32-microblaze.c \
	/Users/dengyouxu/Developer/Software/openjdk8u/jdk8u-xcode10/jdk8u-dev/hotspot/src/share/tools/binutils/binutils-2.30/bfd/elf32-mips.c \
	/Users/dengyouxu/Developer/Software/openjdk8u/jdk8u-xcode10/jdk8u-dev/hotspot/src/share/tools/binutils/binutils-2.30/bfd/elf32-moxie.c \
	/Users/dengyouxu/Developer/Software/openjdk8u/jdk8u-xcode10/jdk8u-dev/hotspot/src/share/tools/binutils/binutils-2.30/bfd/elf32-msp430.c \
	/Users/dengyouxu/Developer/Software/openjdk8u/jdk8u-xcode10/jdk8u-dev/hotspot/src/share/tools/binutils/binutils-2.30/bfd/elf32-mt.c \
	/Users/dengyouxu/Developer/Software/openjdk8u/jdk8u-xcode10/jdk8u-dev/hotspot/src/share/tools/binutils/binutils-2.30/bfd/elf32-nds32.c \
	/Users/dengyouxu/Developer/Software/openjdk8u/jdk8u-xcode10/jdk8u-dev/hotspot/src/share/tools/binutils/binutils-2.30/bfd/elf32-nios2.c \
	/Users/dengyouxu/Developer/Software/openjdk8u/jdk8u-xcode10/jdk8u-dev/hotspot/src/share/tools/binutils/binutils-2.30/bfd/elf32-or1k.c \
	/Users/dengyouxu/Developer/Software/openjdk8u/jdk8u-xcode10/jdk8u-dev/hotspot/src/share/tools/binutils/binutils-2.30/bfd/elf32-pj.c \
	/Users/dengyouxu/Developer/Software/openjdk8u/jdk8u-xcode10/jdk8u-dev/hotspot/src/share/tools/binutils/binutils-2.30/bfd/elf32-ppc.c \
	/Users/dengyouxu/Developer/Software/openjdk8u/jdk8u-xcode10/jdk8u-dev/hotspot/src/share/tools/binutils/binutils-2.30/bfd/elf32-pru.c \
	/Users/dengyouxu/Developer/Software/openjdk8u/jdk8u-xcode10/jdk8u-dev/hotspot/src/share/tools/binutils/binutils-2.30/bfd/elf32-rl78.c \
	/Users/dengyouxu/Developer/Software/openjdk8u/jdk8u-xcode10/jdk8u-dev/hotspot/src/share/tools/binutils/binutils-2.30/bfd/elf32-rx.c \
	/Users/dengyouxu/Developer/Software/openjdk8u/jdk8u-xcode10/jdk8u-dev/hotspot/src/share/tools/binutils/binutils-2.30/bfd/elf32-s390.c \
	/Users/dengyouxu/Developer/Software/openjdk8u/jdk8u-xcode10/jdk8u-dev/hotspot/src/share/tools/binutils/binutils-2.30/bfd/elf32-score.c \
	/Users/dengyouxu/Developer/Software/openjdk8u/jdk8u-xcode10/jdk8u-dev/hotspot/src/share/tools/binutils/binutils-2.30/bfd/elf32-score7.c \
	/Users/dengyouxu/Developer/Software/openjdk8u/jdk8u-xcode10/jdk8u-dev/hotspot/src/share/tools/binutils/binutils-2.30/bfd/elf32-sh-symbian.c \
	/Users/dengyouxu/Developer/Software/openjdk8u/jdk8u-xcode10/jdk8u-dev/hotspot/src/share/tools/binutils/binutils-2.30/bfd/elf32-sh.c \
	/Users/dengyouxu/Developer/Software/openjdk8u/jdk8u-xcode10/jdk8u-dev/hotspot/src/share/tools/binutils/binutils-2.30/bfd/elf32-sh64-com.c \
	/Users/dengyouxu/Developer/Software/openjdk8u/jdk8u-xcode10/jdk8u-dev/hotspot/src/share/tools/binutils/binutils-2.30/bfd/elf32-sh64.c \
	/Users/dengyouxu/Developer/Software/openjdk8u/jdk8u-xcode10/jdk8u-dev/hotspot/src/share/tools/binutils/binutils-2.30/bfd/elf32-sparc.c \
	/Users/dengyouxu/Developer/Software/openjdk8u/jdk8u-xcode10/jdk8u-dev/hotspot/src/share/tools/binutils/binutils-2.30/bfd/elf32-spu.c \
	/Users/dengyouxu/Developer/Software/openjdk8u/jdk8u-xcode10/jdk8u-dev/hotspot/src/share/tools/binutils/binutils-2.30/bfd/elf32-tic6x.c \
	/Users/dengyouxu/Developer/Software/openjdk8u/jdk8u-xcode10/jdk8u-dev/hotspot/src/share/tools/binutils/binutils-2.30/bfd/elf32-tilegx.c \
	/Users/dengyouxu/Developer/Software/openjdk8u/jdk8u-xcode10/jdk8u-dev/hotspot/src/share/tools/binutils/binutils-2.30/bfd/elf32-tilepro.c \
	/Users/dengyouxu/Developer/Software/openjdk8u/jdk8u-xcode10/jdk8u-dev/hotspot/src/share/tools/binutils/binutils-2.30/bfd/elf32-v850.c \
	/Users/dengyouxu/Developer/Software/openjdk8u/jdk8u-xcode10/jdk8u-dev/hotspot/src/share/tools/binutils/binutils-2.30/bfd/elf32-vax.c \
	/Users/dengyouxu/Developer/Software/openjdk8u/jdk8u-xcode10/jdk8u-dev/hotspot/src/share/tools/binutils/binutils-2.30/bfd/elf32-visium.c \
	/Users/dengyouxu/Developer/Software/openjdk8u/jdk8u-xcode10/jdk8u-dev/hotspot/src/share/tools/binutils/binutils-2.30/bfd/elf32-wasm32.c \
	/Users/dengyouxu/Developer/Software/openjdk8u/jdk8u-xcode10/jdk8u-dev/hotspot/src/share/tools/binutils/binutils-2.30/bfd/elf32-xc16x.c \
	/Users/dengyouxu/Developer/Software/openjdk8u/jdk8u-xcode10/jdk8u-dev/hotspot/src/share/tools/binutils/binutils-2.30/bfd/elf32-xgate.c \
	/Users/dengyouxu/Developer/Software/openjdk8u/jdk8u-xcode10/jdk8u-dev/hotspot/src/share/tools/binutils/binutils-2.30/bfd/elf32-xstormy16.c \
	/Users/dengyouxu/Developer/Software/openjdk8u/jdk8u-xcode10/jdk8u-dev/hotspot/src/share/tools/binutils/binutils-2.30/bfd/elf32-xtensa.c \
	/Users/dengyouxu/Developer/Software/openjdk8u/jdk8u-xcode10/jdk8u-dev/hotspot/src/share/tools/binutils/binutils-2.30/bfd/elf32.c \
	/Users/dengyouxu/Developer/Software/openjdk8u/jdk8u-xcode10/jdk8u-dev/hotspot/src/share/tools/binutils/binutils-2.30/bfd/elf64-alpha.c \
	/Users/dengyouxu/Developer/Software/openjdk8u/jdk8u-xcode10/jdk8u-dev/hotspot/src/share/tools/binutils/binutils-2.30/bfd/elf64-gen.c \
	/Users/dengyouxu/Developer/Software/openjdk8u/jdk8u-xcode10/jdk8u-dev/hotspot/src/share/tools/binutils/binutils-2.30/bfd/elf64-hppa.c \
	/Users/dengyouxu/Developer/Software/openjdk8u/jdk8u-xcode10/jdk8u-dev/hotspot/src/share/tools/binutils/binutils-2.30/bfd/elf64-hppa.h \
	/Users/dengyouxu/Developer/Software/openjdk8u/jdk8u-xcode10/jdk8u-dev/hotspot/src/share/tools/binutils/binutils-2.30/bfd/elf64-ia64-vms.c \
	/Users/dengyouxu/Developer/Software/openjdk8u/jdk8u-xcode10/jdk8u-dev/hotspot/src/share/tools/binutils/binutils-2.30/bfd/elf64-mips.c \
	/Users/dengyouxu/Developer/Software/openjdk8u/jdk8u-xcode10/jdk8u-dev/hotspot/src/share/tools/binutils/binutils-2.30/bfd/elf64-mmix.c \
	/Users/dengyouxu/Developer/Software/openjdk8u/jdk8u-xcode10/jdk8u-dev/hotspot/src/share/tools/binutils/binutils-2.30/bfd/elf64-ppc.c \
	/Users/dengyouxu/Developer/Software/openjdk8u/jdk8u-xcode10/jdk8u-dev/hotspot/src/share/tools/binutils/binutils-2.30/bfd/elf64-s390.c \
	/Users/dengyouxu/Developer/Software/openjdk8u/jdk8u-xcode10/jdk8u-dev/hotspot/src/share/tools/binutils/binutils-2.30/bfd/elf64-sh64.c \
	/Users/dengyouxu/Developer/Software/openjdk8u/jdk8u-xcode10/jdk8u-dev/hotspot/src/share/tools/binutils/binutils-2.30/bfd/elf64-sparc.c \
	/Users/dengyouxu/Developer/Software/openjdk8u/jdk8u-xcode10/jdk8u-dev/hotspot/src/share/tools/binutils/binutils-2.30/bfd/elf64-tilegx.c \
	/Users/dengyouxu/Developer/Software/openjdk8u/jdk8u-xcode10/jdk8u-dev/hotspot/src/share/tools/binutils/binutils-2.30/bfd/elf64-x86-64.c \
	/Users/dengyouxu/Developer/Software/openjdk8u/jdk8u-xcode10/jdk8u-dev/hotspot/src/share/tools/binutils/binutils-2.30/bfd/elf64.c \
	/Users/dengyouxu/Developer/Software/openjdk8u/jdk8u-xcode10/jdk8u-dev/hotspot/src/share/tools/binutils/binutils-2.30/bfd/elfcode.h \
	/Users/dengyouxu/Developer/Software/openjdk8u/jdk8u-xcode10/jdk8u-dev/hotspot/src/share/tools/binutils/binutils-2.30/bfd/elfcore.h \
	/Users/dengyouxu/Developer/Software/openjdk8u/jdk8u-xcode10/jdk8u-dev/hotspot/src/share/tools/binutils/binutils-2.30/bfd/elflink.c \
	/Users/dengyouxu/Developer/Software/openjdk8u/jdk8u-xcode10/jdk8u-dev/hotspot/src/share/tools/binutils/binutils-2.30/bfd/elfn32-mips.c \
	/Users/dengyouxu/Developer/Software/openjdk8u/jdk8u-xcode10/jdk8u-dev/hotspot/src/share/tools/binutils/binutils-2.30/bfd/elfxx-aarch64.c \
	/Users/dengyouxu/Developer/Software/openjdk8u/jdk8u-xcode10/jdk8u-dev/hotspot/src/share/tools/binutils/binutils-2.30/bfd/elfxx-ia64.c \
	/Users/dengyouxu/Developer/Software/openjdk8u/jdk8u-xcode10/jdk8u-dev/hotspot/src/share/tools/binutils/binutils-2.30/bfd/elfxx-mips.c \
	/Users/dengyouxu/Developer/Software/openjdk8u/jdk8u-xcode10/jdk8u-dev/hotspot/src/share/tools/binutils/binutils-2.30/bfd/elfxx-riscv.c \
	/Users/dengyouxu/Developer/Software/openjdk8u/jdk8u-xcode10/jdk8u-dev/hotspot/src/share/tools/binutils/binutils-2.30/bfd/elfxx-sparc.c \
	/Users/dengyouxu/Developer/Software/openjdk8u/jdk8u-xcode10/jdk8u-dev/hotspot/src/share/tools/binutils/binutils-2.30/bfd/elfxx-tilegx.c \
	/Users/dengyouxu/Developer/Software/openjdk8u/jdk8u-xcode10/jdk8u-dev/hotspot/src/share/tools/binutils/binutils-2.30/bfd/elfxx-x86.c \
	/Users/dengyouxu/Developer/Software/openjdk8u/jdk8u-xcode10/jdk8u-dev/hotspot/src/share/tools/binutils/binutils-2.30/bfd/elfxx-x86.c \
	/Users/dengyouxu/Developer/Software/openjdk8u/jdk8u-xcode10/jdk8u-dev/hotspot/src/share/tools/binutils/binutils-2.30/bfd/epoc-pe-arm.c \
	/Users/dengyouxu/Developer/Software/openjdk8u/jdk8u-xcode10/jdk8u-dev/hotspot/src/share/tools/binutils/binutils-2.30/bfd/epoc-pei-arm.c \
	/Users/dengyouxu/Developer/Software/openjdk8u/jdk8u-xcode10/jdk8u-dev/hotspot/src/share/tools/binutils/binutils-2.30/bfd/format.c \
	/Users/dengyouxu/Developer/Software/openjdk8u/jdk8u-xcode10/jdk8u-dev/hotspot/src/share/tools/binutils/binutils-2.30/bfd/freebsd.h \
	/Users/dengyouxu/Developer/Software/openjdk8u/jdk8u-xcode10/jdk8u-dev/hotspot/src/share/tools/binutils/binutils-2.30/bfd/genlink.h \
	/Users/dengyouxu/Developer/Software/openjdk8u/jdk8u-xcode10/jdk8u-dev/hotspot/src/share/tools/binutils/binutils-2.30/bfd/go32stub.h \
	/Users/dengyouxu/Developer/Software/openjdk8u/jdk8u-xcode10/jdk8u-dev/hotspot/src/share/tools/binutils/binutils-2.30/bfd/hash.c \
	/Users/dengyouxu/Developer/Software/openjdk8u/jdk8u-xcode10/jdk8u-dev/hotspot/src/share/tools/binutils/binutils-2.30/bfd/hp300bsd.c \
	/Users/dengyouxu/Developer/Software/openjdk8u/jdk8u-xcode10/jdk8u-dev/hotspot/src/share/tools/binutils/binutils-2.30/bfd/hp300hpux.c \
	/Users/dengyouxu/Developer/Software/openjdk8u/jdk8u-xcode10/jdk8u-dev/hotspot/src/share/tools/binutils/binutils-2.30/bfd/hpux-core.c \
	/Users/dengyouxu/Developer/Software/openjdk8u/jdk8u-xcode10/jdk8u-dev/hotspot/src/share/tools/binutils/binutils-2.30/bfd/i386aout.c \
	/Users/dengyouxu/Developer/Software/openjdk8u/jdk8u-xcode10/jdk8u-dev/hotspot/src/share/tools/binutils/binutils-2.30/bfd/i386bsd.c \
	/Users/dengyouxu/Developer/Software/openjdk8u/jdk8u-xcode10/jdk8u-dev/hotspot/src/share/tools/binutils/binutils-2.30/bfd/i386dynix.c \
	/Users/dengyouxu/Developer/Software/openjdk8u/jdk8u-xcode10/jdk8u-dev/hotspot/src/share/tools/binutils/binutils-2.30/bfd/i386freebsd.c \
	/Users/dengyouxu/Developer/Software/openjdk8u/jdk8u-xcode10/jdk8u-dev/hotspot/src/share/tools/binutils/binutils-2.30/bfd/i386linux.c \
	/Users/dengyouxu/Developer/Software/openjdk8u/jdk8u-xcode10/jdk8u-dev/hotspot/src/share/tools/binutils/binutils-2.30/bfd/i386lynx.c \
	/Users/dengyouxu/Developer/Software/openjdk8u/jdk8u-xcode10/jdk8u-dev/hotspot/src/share/tools/binutils/binutils-2.30/bfd/i386mach3.c \
	/Users/dengyouxu/Developer/Software/openjdk8u/jdk8u-xcode10/jdk8u-dev/hotspot/src/share/tools/binutils/binutils-2.30/bfd/i386msdos.c \
	/Users/dengyouxu/Developer/Software/openjdk8u/jdk8u-xcode10/jdk8u-dev/hotspot/src/share/tools/binutils/binutils-2.30/bfd/i386netbsd.c \
	/Users/dengyouxu/Developer/Software/openjdk8u/jdk8u-xcode10/jdk8u-dev/hotspot/src/share/tools/binutils/binutils-2.30/bfd/i386os9k.c \
	/Users/dengyouxu/Developer/Software/openjdk8u/jdk8u-xcode10/jdk8u-dev/hotspot/src/share/tools/binutils/binutils-2.30/bfd/ieee.c \
	/Users/dengyouxu/Developer/Software/openjdk8u/jdk8u-xcode10/jdk8u-dev/hotspot/src/share/tools/binutils/binutils-2.30/bfd/ihex.c \
	/Users/dengyouxu/Developer/Software/openjdk8u/jdk8u-xcode10/jdk8u-dev/hotspot/src/share/tools/binutils/binutils-2.30/bfd/init.c \
	/Users/dengyouxu/Developer/Software/openjdk8u/jdk8u-xcode10/jdk8u-dev/hotspot/src/share/tools/binutils/binutils-2.30/bfd/irix-core.c \
	/Users/dengyouxu/Developer/Software/openjdk8u/jdk8u-xcode10/jdk8u-dev/hotspot/src/share/tools/binutils/binutils-2.30/bfd/libaout.h \
	/Users/dengyouxu/Developer/Software/openjdk8u/jdk8u-xcode10/jdk8u-dev/hotspot/src/share/tools/binutils/binutils-2.30/bfd/libbfd.c \
	/Users/dengyouxu/Developer/Software/openjdk8u/jdk8u-xcode10/jdk8u-dev/hotspot/src/share/tools/binutils/binutils-2.30/bfd/libbfd.h \
	/Users/dengyouxu/Developer/Software/openjdk8u/jdk8u-xcode10/jdk8u-dev/hotspot/src/share/tools/binutils/binutils-2.30/bfd/libcoff.h \
	/Users/dengyouxu/Developer/Software/openjdk8u/jdk8u-xcode10/jdk8u-dev/hotspot/src/share/tools/binutils/binutils-2.30/bfd/libecoff.h \
	/Users/dengyouxu/Developer/Software/openjdk8u/jdk8u-xcode10/jdk8u-dev/hotspot/src/share/tools/binutils/binutils-2.30/bfd/libhppa.h \
	/Users/dengyouxu/Developer/Software/openjdk8u/jdk8u-xcode10/jdk8u-dev/hotspot/src/share/tools/binutils/binutils-2.30/bfd/libieee.h \
	/Users/dengyouxu/Developer/Software/openjdk8u/jdk8u-xcode10/jdk8u-dev/hotspot/src/share/tools/binutils/binutils-2.30/bfd/libnlm.h \
	/Users/dengyouxu/Developer/Software/openjdk8u/jdk8u-xcode10/jdk8u-dev/hotspot/src/share/tools/binutils/binutils-2.30/bfd/liboasys.h \
	/Users/dengyouxu/Developer/Software/openjdk8u/jdk8u-xcode10/jdk8u-dev/hotspot/src/share/tools/binutils/binutils-2.30/bfd/libpei.h \
	/Users/dengyouxu/Developer/Software/openjdk8u/jdk8u-xcode10/jdk8u-dev/hotspot/src/share/tools/binutils/binutils-2.30/bfd/libxcoff.h \
	/Users/dengyouxu/Developer/Software/openjdk8u/jdk8u-xcode10/jdk8u-dev/hotspot/src/share/tools/binutils/binutils-2.30/bfd/linker.c \
	/Users/dengyouxu/Developer/Software/openjdk8u/jdk8u-xcode10/jdk8u-dev/hotspot/src/share/tools/binutils/binutils-2.30/bfd/lynx-core.c \
	/Users/dengyouxu/Developer/Software/openjdk8u/jdk8u-xcode10/jdk8u-dev/hotspot/src/share/tools/binutils/binutils-2.30/bfd/m68k4knetbsd.c \
	/Users/dengyouxu/Developer/Software/openjdk8u/jdk8u-xcode10/jdk8u-dev/hotspot/src/share/tools/binutils/binutils-2.30/bfd/m68klinux.c \
	/Users/dengyouxu/Developer/Software/openjdk8u/jdk8u-xcode10/jdk8u-dev/hotspot/src/share/tools/binutils/binutils-2.30/bfd/m68knetbsd.c \
	/Users/dengyouxu/Developer/Software/openjdk8u/jdk8u-xcode10/jdk8u-dev/hotspot/src/share/tools/binutils/binutils-2.30/bfd/m88kmach3.c \
	/Users/dengyouxu/Developer/Software/openjdk8u/jdk8u-xcode10/jdk8u-dev/hotspot/src/share/tools/binutils/binutils-2.30/bfd/m88kopenbsd.c \
	/Users/dengyouxu/Developer/Software/openjdk8u/jdk8u-xcode10/jdk8u-dev/hotspot/src/share/tools/binutils/binutils-2.30/bfd/mach-o-aarch64.c \
	/Users/dengyouxu/Developer/Software/openjdk8u/jdk8u-xcode10/jdk8u-dev/hotspot/src/share/tools/binutils/binutils-2.30/bfd/mach-o-arm.c \
	/Users/dengyouxu/Developer/Software/openjdk8u/jdk8u-xcode10/jdk8u-dev/hotspot/src/share/tools/binutils/binutils-2.30/bfd/mach-o-i386.c \
	/Users/dengyouxu/Developer/Software/openjdk8u/jdk8u-xcode10/jdk8u-dev/hotspot/src/share/tools/binutils/binutils-2.30/bfd/mach-o-x86-64.c \
	/Users/dengyouxu/Developer/Software/openjdk8u/jdk8u-xcode10/jdk8u-dev/hotspot/src/share/tools/binutils/binutils-2.30/bfd/mach-o.c \
	/Users/dengyouxu/Developer/Software/openjdk8u/jdk8u-xcode10/jdk8u-dev/hotspot/src/share/tools/binutils/binutils-2.30/bfd/mach-o.h \
	/Users/dengyouxu/Developer/Software/openjdk8u/jdk8u-xcode10/jdk8u-dev/hotspot/src/share/tools/binutils/binutils-2.30/bfd/merge.c \
	/Users/dengyouxu/Developer/Software/openjdk8u/jdk8u-xcode10/jdk8u-dev/hotspot/src/share/tools/binutils/binutils-2.30/bfd/mipsbsd.c \
	/Users/dengyouxu/Developer/Software/openjdk8u/jdk8u-xcode10/jdk8u-dev/hotspot/src/share/tools/binutils/binutils-2.30/bfd/mmo.c \
	/Users/dengyouxu/Developer/Software/openjdk8u/jdk8u-xcode10/jdk8u-dev/hotspot/src/share/tools/binutils/binutils-2.30/bfd/netbsd-core.c \
	/Users/dengyouxu/Developer/Software/openjdk8u/jdk8u-xcode10/jdk8u-dev/hotspot/src/share/tools/binutils/binutils-2.30/bfd/netbsd.h \
	/Users/dengyouxu/Developer/Software/openjdk8u/jdk8u-xcode10/jdk8u-dev/hotspot/src/share/tools/binutils/binutils-2.30/bfd/newsos3.c \
	/Users/dengyouxu/Developer/Software/openjdk8u/jdk8u-xcode10/jdk8u-dev/hotspot/src/share/tools/binutils/binutils-2.30/bfd/nlm-target.h \
	/Users/dengyouxu/Developer/Software/openjdk8u/jdk8u-xcode10/jdk8u-dev/hotspot/src/share/tools/binutils/binutils-2.30/bfd/nlm.c \
	/Users/dengyouxu/Developer/Software/openjdk8u/jdk8u-xcode10/jdk8u-dev/hotspot/src/share/tools/binutils/binutils-2.30/bfd/nlm32-alpha.c \
	/Users/dengyouxu/Developer/Software/openjdk8u/jdk8u-xcode10/jdk8u-dev/hotspot/src/share/tools/binutils/binutils-2.30/bfd/nlm32-i386.c \
	/Users/dengyouxu/Developer/Software/openjdk8u/jdk8u-xcode10/jdk8u-dev/hotspot/src/share/tools/binutils/binutils-2.30/bfd/nlm32-ppc.c \
	/Users/dengyouxu/Developer/Software/openjdk8u/jdk8u-xcode10/jdk8u-dev/hotspot/src/share/tools/binutils/binutils-2.30/bfd/nlm32-sparc.c \
	/Users/dengyouxu/Developer/Software/openjdk8u/jdk8u-xcode10/jdk8u-dev/hotspot/src/share/tools/binutils/binutils-2.30/bfd/nlm32.c \
	/Users/dengyouxu/Developer/Software/openjdk8u/jdk8u-xcode10/jdk8u-dev/hotspot/src/share/tools/binutils/binutils-2.30/bfd/nlm64.c \
	/Users/dengyouxu/Developer/Software/openjdk8u/jdk8u-xcode10/jdk8u-dev/hotspot/src/share/tools/binutils/binutils-2.30/bfd/nlmcode.h \
	/Users/dengyouxu/Developer/Software/openjdk8u/jdk8u-xcode10/jdk8u-dev/hotspot/src/share/tools/binutils/binutils-2.30/bfd/nlmswap.h \
	/Users/dengyouxu/Developer/Software/openjdk8u/jdk8u-xcode10/jdk8u-dev/hotspot/src/share/tools/binutils/binutils-2.30/bfd/ns32k.h \
	/Users/dengyouxu/Developer/Software/openjdk8u/jdk8u-xcode10/jdk8u-dev/hotspot/src/share/tools/binutils/binutils-2.30/bfd/ns32knetbsd.c \
	/Users/dengyouxu/Developer/Software/openjdk8u/jdk8u-xcode10/jdk8u-dev/hotspot/src/share/tools/binutils/binutils-2.30/bfd/oasys.c \
	/Users/dengyouxu/Developer/Software/openjdk8u/jdk8u-xcode10/jdk8u-dev/hotspot/src/share/tools/binutils/binutils-2.30/bfd/opncls.c \
	/Users/dengyouxu/Developer/Software/openjdk8u/jdk8u-xcode10/jdk8u-dev/hotspot/src/share/tools/binutils/binutils-2.30/bfd/osf-core.c \
	/Users/dengyouxu/Developer/Software/openjdk8u/jdk8u-xcode10/jdk8u-dev/hotspot/src/share/tools/binutils/binutils-2.30/bfd/pc532-mach.c \
	/Users/dengyouxu/Developer/Software/openjdk8u/jdk8u-xcode10/jdk8u-dev/hotspot/src/share/tools/binutils/binutils-2.30/bfd/pdp11.c \
	/Users/dengyouxu/Developer/Software/openjdk8u/jdk8u-xcode10/jdk8u-dev/hotspot/src/share/tools/binutils/binutils-2.30/bfd/pe-arm-wince.c \
	/Users/dengyouxu/Developer/Software/openjdk8u/jdk8u-xcode10/jdk8u-dev/hotspot/src/share/tools/binutils/binutils-2.30/bfd/pe-arm.c \
	/Users/dengyouxu/Developer/Software/openjdk8u/jdk8u-xcode10/jdk8u-dev/hotspot/src/share/tools/binutils/binutils-2.30/bfd/pe-i386.c \
	/Users/dengyouxu/Developer/Software/openjdk8u/jdk8u-xcode10/jdk8u-dev/hotspot/src/share/tools/binutils/binutils-2.30/bfd/pe-mcore.c \
	/Users/dengyouxu/Developer/Software/openjdk8u/jdk8u-xcode10/jdk8u-dev/hotspot/src/share/tools/binutils/binutils-2.30/bfd/pe-mips.c \
	/Users/dengyouxu/Developer/Software/openjdk8u/jdk8u-xcode10/jdk8u-dev/hotspot/src/share/tools/binutils/binutils-2.30/bfd/pe-ppc.c \
	/Users/dengyouxu/Developer/Software/openjdk8u/jdk8u-xcode10/jdk8u-dev/hotspot/src/share/tools/binutils/binutils-2.30/bfd/pe-sh.c \
	/Users/dengyouxu/Developer/Software/openjdk8u/jdk8u-xcode10/jdk8u-dev/hotspot/src/share/tools/binutils/binutils-2.30/bfd/pe-x86_64.c \
	/Users/dengyouxu/Developer/Software/openjdk8u/jdk8u-xcode10/jdk8u-dev/hotspot/src/share/tools/binutils/binutils-2.30/bfd/pef-traceback.h \
	/Users/dengyouxu/Developer/Software/openjdk8u/jdk8u-xcode10/jdk8u-dev/hotspot/src/share/tools/binutils/binutils-2.30/bfd/pef.c \
	/Users/dengyouxu/Developer/Software/openjdk8u/jdk8u-xcode10/jdk8u-dev/hotspot/src/share/tools/binutils/binutils-2.30/bfd/pef.h \
	/Users/dengyouxu/Developer/Software/openjdk8u/jdk8u-xcode10/jdk8u-dev/hotspot/src/share/tools/binutils/binutils-2.30/bfd/pei-arm-wince.c \
	/Users/dengyouxu/Developer/Software/openjdk8u/jdk8u-xcode10/jdk8u-dev/hotspot/src/share/tools/binutils/binutils-2.30/bfd/pei-arm.c \
	/Users/dengyouxu/Developer/Software/openjdk8u/jdk8u-xcode10/jdk8u-dev/hotspot/src/share/tools/binutils/binutils-2.30/bfd/pei-i386.c \
	/Users/dengyouxu/Developer/Software/openjdk8u/jdk8u-xcode10/jdk8u-dev/hotspot/src/share/tools/binutils/binutils-2.30/bfd/pei-ia64.c \
	/Users/dengyouxu/Developer/Software/openjdk8u/jdk8u-xcode10/jdk8u-dev/hotspot/src/share/tools/binutils/binutils-2.30/bfd/pei-mcore.c \
	/Users/dengyouxu/Developer/Software/openjdk8u/jdk8u-xcode10/jdk8u-dev/hotspot/src/share/tools/binutils/binutils-2.30/bfd/pei-mips.c \
	/Users/dengyouxu/Developer/Software/openjdk8u/jdk8u-xcode10/jdk8u-dev/hotspot/src/share/tools/binutils/binutils-2.30/bfd/pei-ppc.c \
	/Users/dengyouxu/Developer/Software/openjdk8u/jdk8u-xcode10/jdk8u-dev/hotspot/src/share/tools/binutils/binutils-2.30/bfd/pei-sh.c \
	/Users/dengyouxu/Developer/Software/openjdk8u/jdk8u-xcode10/jdk8u-dev/hotspot/src/share/tools/binutils/binutils-2.30/bfd/pei-x86_64.c \
	/Users/dengyouxu/Developer/Software/openjdk8u/jdk8u-xcode10/jdk8u-dev/hotspot/src/share/tools/binutils/binutils-2.30/bfd/peicode.h \
	/Users/dengyouxu/Developer/Software/openjdk8u/jdk8u-xcode10/jdk8u-dev/hotspot/src/share/tools/binutils/binutils-2.30/bfd/plugin.c \
	/Users/dengyouxu/Developer/Software/openjdk8u/jdk8u-xcode10/jdk8u-dev/hotspot/src/share/tools/binutils/binutils-2.30/bfd/ppcboot.c \
	/Users/dengyouxu/Developer/Software/openjdk8u/jdk8u-xcode10/jdk8u-dev/hotspot/src/share/tools/binutils/binutils-2.30/bfd/reloc.c \
	/Users/dengyouxu/Developer/Software/openjdk8u/jdk8u-xcode10/jdk8u-dev/hotspot/src/share/tools/binutils/binutils-2.30/bfd/reloc16.c \
	/Users/dengyouxu/Developer/Software/openjdk8u/jdk8u-xcode10/jdk8u-dev/hotspot/src/share/tools/binutils/binutils-2.30/bfd/riscix.c \
	/Users/dengyouxu/Developer/Software/openjdk8u/jdk8u-xcode10/jdk8u-dev/hotspot/src/share/tools/binutils/binutils-2.30/bfd/rs6000-core.c \
	/Users/dengyouxu/Developer/Software/openjdk8u/jdk8u-xcode10/jdk8u-dev/hotspot/src/share/tools/binutils/binutils-2.30/bfd/sco5-core.c \
	/Users/dengyouxu/Developer/Software/openjdk8u/jdk8u-xcode10/jdk8u-dev/hotspot/src/share/tools/binutils/binutils-2.30/bfd/section.c \
	/Users/dengyouxu/Developer/Software/openjdk8u/jdk8u-xcode10/jdk8u-dev/hotspot/src/share/tools/binutils/binutils-2.30/bfd/simple.c \
	/Users/dengyouxu/Developer/Software/openjdk8u/jdk8u-xcode10/jdk8u-dev/hotspot/src/share/tools/binutils/binutils-2.30/bfd/som.c \
	/Users/dengyouxu/Developer/Software/openjdk8u/jdk8u-xcode10/jdk8u-dev/hotspot/src/share/tools/binutils/binutils-2.30/bfd/som.h \
	/Users/dengyouxu/Developer/Software/openjdk8u/jdk8u-xcode10/jdk8u-dev/hotspot/src/share/tools/binutils/binutils-2.30/bfd/sparclinux.c \
	/Users/dengyouxu/Developer/Software/openjdk8u/jdk8u-xcode10/jdk8u-dev/hotspot/src/share/tools/binutils/binutils-2.30/bfd/sparclynx.c \
	/Users/dengyouxu/Developer/Software/openjdk8u/jdk8u-xcode10/jdk8u-dev/hotspot/src/share/tools/binutils/binutils-2.30/bfd/sparcnetbsd.c \
	/Users/dengyouxu/Developer/Software/openjdk8u/jdk8u-xcode10/jdk8u-dev/hotspot/src/share/tools/binutils/binutils-2.30/bfd/srec.c \
	/Users/dengyouxu/Developer/Software/openjdk8u/jdk8u-xcode10/jdk8u-dev/hotspot/src/share/tools/binutils/binutils-2.30/bfd/stab-syms.c \
	/Users/dengyouxu/Developer/Software/openjdk8u/jdk8u-xcode10/jdk8u-dev/hotspot/src/share/tools/binutils/binutils-2.30/bfd/stabs.c \
	/Users/dengyouxu/Developer/Software/openjdk8u/jdk8u-xcode10/jdk8u-dev/hotspot/src/share/tools/binutils/binutils-2.30/bfd/sunos.c \
	/Users/dengyouxu/Developer/Software/openjdk8u/jdk8u-xcode10/jdk8u-dev/hotspot/src/share/tools/binutils/binutils-2.30/bfd/syms.c \
	/Users/dengyouxu/Developer/Software/openjdk8u/jdk8u-xcode10/jdk8u-dev/hotspot/src/share/tools/binutils/binutils-2.30/bfd/targets.c \
	/Users/dengyouxu/Developer/Software/openjdk8u/jdk8u-xcode10/jdk8u-dev/hotspot/src/share/tools/binutils/binutils-2.30/bfd/tekhex.c \
	/Users/dengyouxu/Developer/Software/openjdk8u/jdk8u-xcode10/jdk8u-dev/hotspot/src/share/tools/binutils/binutils-2.30/bfd/trad-core.c \
	/Users/dengyouxu/Developer/Software/openjdk8u/jdk8u-xcode10/jdk8u-dev/hotspot/src/share/tools/binutils/binutils-2.30/bfd/vax1knetbsd.c \
	/Users/dengyouxu/Developer/Software/openjdk8u/jdk8u-xcode10/jdk8u-dev/hotspot/src/share/tools/binutils/binutils-2.30/bfd/vaxbsd.c \
	/Users/dengyouxu/Developer/Software/openjdk8u/jdk8u-xcode10/jdk8u-dev/hotspot/src/share/tools/binutils/binutils-2.30/bfd/vaxnetbsd.c \
	/Users/dengyouxu/Developer/Software/openjdk8u/jdk8u-xcode10/jdk8u-dev/hotspot/src/share/tools/binutils/binutils-2.30/bfd/verilog.c \
	/Users/dengyouxu/Developer/Software/openjdk8u/jdk8u-xcode10/jdk8u-dev/hotspot/src/share/tools/binutils/binutils-2.30/bfd/versados.c \
	/Users/dengyouxu/Developer/Software/openjdk8u/jdk8u-xcode10/jdk8u-dev/hotspot/src/share/tools/binutils/binutils-2.30/bfd/version.h \
	/Users/dengyouxu/Developer/Software/openjdk8u/jdk8u-xcode10/jdk8u-dev/hotspot/src/share/tools/binutils/binutils-2.30/bfd/vms-alpha.c \
	/Users/dengyouxu/Developer/Software/openjdk8u/jdk8u-xcode10/jdk8u-dev/hotspot/src/share/tools/binutils/binutils-2.30/bfd/vms-lib.c \
	/Users/dengyouxu/Developer/Software/openjdk8u/jdk8u-xcode10/jdk8u-dev/hotspot/src/share/tools/binutils/binutils-2.30/bfd/vms-misc.c \
	/Users/dengyouxu/Developer/Software/openjdk8u/jdk8u-xcode10/jdk8u-dev/hotspot/src/share/tools/binutils/binutils-2.30/bfd/vms.h \
	/Users/dengyouxu/Developer/Software/openjdk8u/jdk8u-xcode10/jdk8u-dev/hotspot/src/share/tools/binutils/binutils-2.30/bfd/wasm-module.c \
	/Users/dengyouxu/Developer/Software/openjdk8u/jdk8u-xcode10/jdk8u-dev/hotspot/src/share/tools/binutils/binutils-2.30/bfd/xcofflink.c \
	/Users/dengyouxu/Developer/Software/openjdk8u/jdk8u-xcode10/jdk8u-dev/hotspot/src/share/tools/binutils/binutils-2.30/bfd/xsym.c \
	/Users/dengyouxu/Developer/Software/openjdk8u/jdk8u-xcode10/jdk8u-dev/hotspot/src/share/tools/binutils/binutils-2.30/bfd/xsym.h \
	/Users/dengyouxu/Developer/Software/openjdk8u/jdk8u-xcode10/jdk8u-dev/hotspot/src/share/tools/binutils/binutils-2.30/bfd/xtensa-isa.c \
	/Users/dengyouxu/Developer/Software/openjdk8u/jdk8u-xcode10/jdk8u-dev/hotspot/src/share/tools/binutils/binutils-2.30/bfd/xtensa-modules.c 

# Note - the following line gets processed by bfd/configure and amended
# to contain the full list of build dir POTFILES.
BLD-POTFILES = \
	../bfd_stdint.h \
	../bfdver.h \
	../elf32-aarch64.c \
	../elf32-ia64.c \
	../elf32-riscv.c \
	../elf32-target.h \
	../elf64-aarch64.c \
	../elf64-ia64.c \
	../elf64-riscv.c \
	../elf64-target.h \
	../peigen.c \
	../pepigen.c \
	../pex64igen.c \
	../targmatch.h 

CATALOGS = 
CATOBJEXT = 
INSTOBJEXT = 

.SUFFIXES:
.SUFFIXES: .c .o .po .pox .gmo .mo .msg .cat

.c.o:
	$(COMPILE) $<

.po.pox:
	$(MAKE) $(PACKAGE).pot
	$(MSGMERGE) $< $(srcdir)/$(PACKAGE).pot -o $*.pox

.po.mo:
	$(MSGFMT) -o $@ $<

.po.gmo:
	file=`echo $* | sed 's,.*/,,'`.gmo \
	  && rm -f $$file && $(GMSGFMT) -o $$file $<

.po.cat:
	sed -f ../intl/po2msg.sed < $< > $*.msg \
	  && rm -f $@ && $(GENCAT) $@ $*.msg


all: all-no

all-yes: $(CATALOGS) # $(PACKAGE).pot
all-no:

$(srcdir)/$(PACKAGE).pot: $(SRC-POTFILES) $(BLD-POTFILES)
	$(XGETTEXT) --default-domain=$(PACKAGE) \
	  --directory=$(top_srcdir) \
	  --add-comments --keyword=_ --keyword=N_ \
	  --msgid-bugs-address=bug-binutils@gnu.org \
	  --files-from=$(srcdir)/SRC-POTFILES.in 
	$(XGETTEXT) --default-domain=$(PACKAGE) \
	  --directory=.. \
	  --directory=. \
	  --add-comments --keyword=_ --keyword=N_ \
	  --join-existing \
	  --msgid-bugs-address=bug-binutils@gnu.org \
	  --files-from=$(srcdir)/BLD-POTFILES.in
	rm -f $(srcdir)/$(PACKAGE).pot
	mv $(PACKAGE).po $(srcdir)/$(PACKAGE).pot

$(srcdir)/cat-id-tbl.c: stamp-cat-id; @:
$(srcdir)/stamp-cat-id: $(PACKAGE).pot
	rm -f cat-id-tbl.tmp
	sed -f ../intl/po2tbl.sed $(srcdir)/$(PACKAGE).pot \
		| sed -e "s/@PACKAGE NAME@/$(PACKAGE)/" > cat-id-tbl.tmp
	if cmp -s cat-id-tbl.tmp $(srcdir)/cat-id-tbl.c; then \
	  rm cat-id-tbl.tmp; \
	else \
	  echo cat-id-tbl.c changed; \
	  rm -f $(srcdir)/cat-id-tbl.c; \
	  mv cat-id-tbl.tmp $(srcdir)/cat-id-tbl.c; \
	fi
	cd $(srcdir) && rm -f stamp-cat-id && echo timestamp > stamp-cat-id


install: install-exec install-data
install-exec:
install-info:
install-html:
install-pdf:
install-data: install-data-no
install-data-no: all
install-data-yes: all
	if test -r $(MKINSTALLDIRS); then \
	  $(MKINSTALLDIRS) $(DESTDIR)$(datadir); \
	else \
	  $(top_srcdir)/mkinstalldirs $(DESTDIR)$(datadir); \
	fi
	@catalogs='$(CATALOGS)'; \
	for cat in $$catalogs; do \
	  cat=`basename $$cat`; \
	  case "$$cat" in \
	    *.gmo) destdir=$(gnulocaledir);; \
	    *)     destdir=$(localedir);; \
	  esac; \
	  lang=`echo $$cat | sed 's/\$(CATOBJEXT)$$//'`; \
	  dir=$(DESTDIR)$$destdir/$$lang/LC_MESSAGES; \
	  if test -r $(MKINSTALLDIRS); then \
	    $(MKINSTALLDIRS) $$dir; \
	  else \
	    $(top_srcdir)/mkinstalldirs $$dir; \
	  fi; \
	  if test -r $$cat; then \
	    $(INSTALL_DATA) $$cat $$dir/$(PACKAGE)$(INSTOBJEXT); \
	    echo "installing $$cat as $$dir/$(PACKAGE)$(INSTOBJEXT)"; \
	  else \
	    $(INSTALL_DATA) $(srcdir)/$$cat $$dir/$(PACKAGE)$(INSTOBJEXT); \
	    echo "installing $(srcdir)/$$cat as" \
		 "$$dir/$(PACKAGE)$(INSTOBJEXT)"; \
	  fi; \
	  if test -r $$cat.m; then \
	    $(INSTALL_DATA) $$cat.m $$dir/$(PACKAGE)$(INSTOBJEXT).m; \
	    echo "installing $$cat.m as $$dir/$(PACKAGE)$(INSTOBJEXT).m"; \
	  else \
	    if test -r $(srcdir)/$$cat.m ; then \
	      $(INSTALL_DATA) $(srcdir)/$$cat.m \
		$$dir/$(PACKAGE)$(INSTOBJEXT).m; \
	      echo "installing $(srcdir)/$$cat as" \
		   "$$dir/$(PACKAGE)$(INSTOBJEXT).m"; \
	    else \
	      true; \
	    fi; \
	  fi; \
	done
	if test "$(PACKAGE)" = "gettext"; then \
	  if test -r $(MKINSTALLDIRS); then \
	    $(MKINSTALLDIRS) $(DESTDIR)$(gettextsrcdir); \
	  else \
	    $(top_srcdir)/mkinstalldirs $(DESTDIR)$(gettextsrcdir); \
	  fi; \
	  $(INSTALL_DATA) $(srcdir)/Makefile.in.in \
			  $(DESTDIR)$(gettextsrcdir)/Makefile.in.in; \
	else \
	  : ; \
	fi

# Define this as empty until I found a useful application.
installcheck:

uninstall:
	catalogs='$(CATALOGS)'; \
	for cat in $$catalogs; do \
	  cat=`basename $$cat`; \
	  lang=`echo $$cat | sed 's/\$(CATOBJEXT)$$//'`; \
	  rm -f $(DESTDIR)$(localedir)/$$lang/LC_MESSAGES/$(PACKAGE)$(INSTOBJEXT); \
	  rm -f $(DESTDIR)$(localedir)/$$lang/LC_MESSAGES/$(PACKAGE)$(INSTOBJEXT).m; \
	  rm -f $(DESTDIR)$(gnulocaledir)/$$lang/LC_MESSAGES/$(PACKAGE)$(INSTOBJEXT); \
	  rm -f $(DESTDIR)$(gnulocaledir)/$$lang/LC_MESSAGES/$(PACKAGE)$(INSTOBJEXT).m; \
	done
	rm -f $(DESTDIR)$(gettextsrcdir)/po-Makefile.in.in

check: all

cat-id-tbl.o: ../intl/libgettext.h

html dvi pdf ps info tags TAGS ID:

mostlyclean:
	rm -f core core.* *.pox $(PACKAGE).po *.old.po cat-id-tbl.tmp
	rm -fr *.o

clean: mostlyclean

distclean: clean
	rm -f Makefile Makefile.in *.mo *.msg *.cat *.cat.m
	rm -f SRC-POTFILES BLD-POTFILES 

maintainer-clean: distclean
	@echo "This command is intended for maintainers to use;"
	@echo "it deletes files that may require special tools to rebuild."
	rm -f $(GMOFILES) SRC-POTFILES.in BLD-POTFILES.in

distdir = ../$(PACKAGE)-$(VERSION)/$(subdir)
dist distdir: update-po $(DISTFILES)
	dists="$(DISTFILES)"; \
	for file in $$dists; do \
	  ln $(srcdir)/$$file $(distdir) 2> /dev/null \
	    || cp -p $(srcdir)/$$file $(distdir); \
	done

update-po: Makefile
	$(MAKE) $(PACKAGE).pot
	PATH=`pwd`/../src:$$PATH; \
	cd $(srcdir); \
	catalogs='$(CATALOGS)'; \
	for cat in $$catalogs; do \
	  cat=`basename $$cat`; \
	  lang=`echo $$cat | sed 's/\$(CATOBJEXT)$$//'`; \
	  mv $$lang.po $$lang.old.po; \
	  echo "$$lang:"; \
	  if $(MSGMERGE) $$lang.old.po $(PACKAGE).pot -o $$lang.po; then \
	    rm -f $$lang.old.po; \
	  else \
	    echo "msgmerge for $$cat failed!"; \
	    rm -f $$lang.po; \
	    mv $$lang.old.po $$lang.po; \
	  fi; \
	done

SRC-POTFILES: SRC-POTFILES.in
	( if test 'x$(srcdir)' != 'x.'; then \
	    posrcprefix='$(top_srcdir)/'; \
	  else \
	    posrcprefix="../"; \
	  fi; \
	  rm -f $@-t $@ \
	    && (sed -e '/^#/d' \
	            -e '/^[ 	]*$$/d' \
		    -e "s@.*@	$$posrcprefix& \\\\@" < $(srcdir)/$@.in \
		| sed -e '$$s/\\$$//') > $@-t \
	    && chmod a-w $@-t \
	    && mv $@-t $@ )

BLD-POTFILES: BLD-POTFILES.in
	  ( rm -f $@-t $@ \
	    && (sed -e '/^#/d' \
	            -e '/^[ 	]*$$/d' \
		    -e "s@.*@	../& \\\\@" < $(srcdir)/$@.in \
		| sed -e '$$s/\\$$//') > $@-t \
	    && chmod a-w $@-t \
	    && mv $@-t $@ )

SRC-POTFILES.in: # ../Makefile
	cd .. && $(MAKE) po/SRC-POTFILES.in

BLD-POTFILES.in: # ../Makefile
	cd .. && $(MAKE) po/BLD-POTFILES.in

# Note - The presence of SRC-POTFILES and BLD-POTFILES as dependencies
# here breaks the implementation of the 'distclean' rule for maintainers.
# This is because if 'make distclean' is run in the BFD directory, the
# Makefile there will be deleted before 'distclean' is made here, and so
# the dependency SRC-POTFILES -> SRC-POTFILES.in -> ../Makefile cannot
# be satisfied.
#
# The SRC-POTFILES and BLD-POTFILES dependencies cannot be removed,
# however since it is necessary that these files be built during
# *configure* time, so that configure can insert them into the
# po/Makefile that it is creating, so that the Makefile will have
# the correct dependencies.
Makefile: Make-in ../config.status SRC-POTFILES BLD-POTFILES
	cd .. \
	  && CONFIG_FILES=$(subdir)/Makefile.in:$(subdir)/Make-in \
	     CONFIG_HEADERS= $(SHELL) ./config.status

# Tell versions [3.59,3.63) of GNU make not to export all variables.
# Otherwise a system limit (for SysV at least) may be exceeded.
.NOEXPORT:
