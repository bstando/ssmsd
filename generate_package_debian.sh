#!/bin/sh

BASEDIR=$PWD
BUILDDIR=$BASEDIR/build_debian
PACKAGEDIR=$BASEDIR/debian_package
DEBIANDIR=$BASEDIR/package_deb

apt install cpack

mkdir -p $PACKAGEDIR

if [ ! -f "$BUILDDIR/ssmsd" ];
then
	sh ./build_debian.sh
fi

mkdir -p $DEBIANDIR/usr/bin
mkdir -p $DEBIANDIR/var/log/ssmsd
mkdir -p $DEBIANDIR/var/ssmsd

cp $BUILDDIR/ssmsd $DEBIANDIR/usr/bin

cd $PACKAGEDIR

cmake -DCMAKE_INSTALL_PREFIX:PATH=/usr $DEBIANDIR

cpack
