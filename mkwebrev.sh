#!/bin/bash

set -e

jdk=jdk8u-dev

REPO_DIR=`pwd`/$jdk
SCRIPT_DIR=`pwd`/xx
WEBREV_BASE=`pwd`/webrevs
. $SCRIPT_DIR/tools.sh `pwd`/tools webrev mercurial
mkdir -p "$WEBREV_BASE"
repos="jdk hotspot corba nashorn langtools jaxp jaxws"
#repos=""

# NOTE: the sed RE is very different on a mac vs Linux!

mkwebrev() {
	# $1 repo-dir $2 webrev-dir $3 CR#
  RD=$1
  WD=$2
  CR=`echo $3 | sed -E 's/[^0-9]*([0-9]+)[^0-9]*.*/\1/g'`
  pushd "$RD" >/dev/null
  N=`hg status | wc -l`
  if [ $N != 0 ] ; then
    webrev.ksh -b -w -o "$WD" -c $CR
    mv $WD/webrev/* $WD/webrev/..
    rmdir $WD/webrev
  else
    echo "  (no differences)"
  fi
  popd >/dev/null
}

mkrevs() {
	# $1 CR $2 NUM
	find "$REPO_DIR" -name \*.rej  -exec rm {} \; 2>/dev/null || true
	find "$REPO_DIR" -name \*.orig -exec rm {} \; 2>/dev/null || true
	WEBREV_DIR=$WEBREV_BASE/jdk-$1/$2
	mkwebrev "$REPO_DIR" $WEBREV_DIR/$2 $1
	for a in $repos ; do 
	  echo processing "$REPO_DIR/$a"
	  mkwebrev "$REPO_DIR/$a" $WEBREV_DIR/$a.$2 $1
	done
	echo "don't forget to run"
	echo "  rsync -v -r  webrevs/jdk-$1/$2 stooke@cr.openjdk.java.net:webrevs"
}

update() {
	pushd "$REPO_DIR" >/dev/null
	find "$REPO_DIR" -name \*.rej  -exec rm {} \; 2>/dev/null || true 
	find "$REPO_DIR" -name \*.orig -exec rm {} \; 2>/dev/null || true
	hg pull -u
	for a in $repos ; do 
	  cd $a
	  hg pull -u
	  cd ..
	done
	popd >/dev/null
}

clean() {
	rm -fr "$REPO_DIR/build"
	find "$REPO_DIR" -name \*.rej  -exec rm {} \; 2>/dev/null || true 
	find "$REPO_DIR" -name \*.orig -exec rm {} \; 2>/dev/null || true
}

revert() {
	pushd "$REPO_DIR" >/dev/null
	find "$REPO_DIR" -name \*.rej  -exec rm {} \; 2>/dev/null || true 
	find "$REPO_DIR" -name \*.orig -exec rm {} \; 2>/dev/null || true
	hg revert .
	for a in $repos ; do 
	  cd $a
	  hg revert .
	  cd ..
	done
	popd >/dev/null
}

revert
update
#mkrevs 8215756-jdk8u 02

exit 0

echo ## for jtreg testing
echo export PRODUCT_HOME=`pwd`/build/linux-x86_64-server-release/images/jdk
echo export JTREG_HOME=$TOOL_DIR/jtreg/build/image/jtreg
echo export PATH=$JTREG_HOME/bin:$PATH

echo ./configure --with-jtreg=$JTHOME
echo make test TEST="test/hotspot/jtreg/gc/TestAllocateHeapAtMultiple.java"
echo make test TEST="jtreg:test/hotspot:hotspot_gc"

