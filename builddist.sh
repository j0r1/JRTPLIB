#!/bin/bash

VERSION_MAJOR=`cat CMakeLists.txt|grep VERSION_MAJOR | head -n 1 | grep set | head -n 1 |cut -f 2 -d " "|cut -f 1 -d ")"`
VERSION_MINOR=`cat CMakeLists.txt|grep VERSION_MINOR | head -n 1 | grep set | head -n 1 |cut -f 2 -d " "|cut -f 1 -d ")"`
VERSION_DEBUG=`cat CMakeLists.txt|grep VERSION_DEBUG | head -n 1 | grep set | head -n 1 |cut -f 2 -d " "|cut -f 1 -d ")"`
VERSION=$VERSION_MAJOR.$VERSION_MINOR.$VERSION_DEBUG
LIBNAME=jrtplib

TMPDIR=`tempfile`
CURDIR=`pwd`
rm -r $TMPDIR
if ! mkdir $TMPDIR ; then
	echo "Couldn't create temporary directory"
	exit -1
fi

cd $TMPDIR
TMPDIR=`pwd` # Get the full path
cd $CURDIR

if ! git archive --format tar --prefix=${LIBNAME}-${VERSION}/ HEAD | (cd $TMPDIR && tar xf -) ; then
	echo "Couldn't archive repository"
	exit -1
fi

cd $TMPDIR/${LIBNAME}-${VERSION}

rm -f `find . -name ".git*"`
rm -f builddist.sh
rm -rf sphinxdoc
rm -f TODO
rm -rf nodist
rm -f aboutrfc3550.txt
	
cd ..

if ! tar cfz ${LIBNAME}-${VERSION}.tar.gz ${LIBNAME}-${VERSION}/ ; then
	echo "Couldn't create archive"
	exit -1
fi

if ! tar cfj ${LIBNAME}-${VERSION}.tar.bz2 ${LIBNAME}-${VERSION}/ ; then
	echo "Couldn't create archive"
	exit -1
fi

if ! zip ${LIBNAME}-${VERSION}.zip `find ${LIBNAME}-${VERSION}/` ; then
	echo "Couldn't create archive"
	exit -1
fi

mv $TMPDIR $CURDIR/


