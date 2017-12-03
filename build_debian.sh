#!/bin/sh

BASEDIR=$PWD
BUILDDIR=$BASEDIR/build_debian

mkdir -p $BUILDDIR

cd $BUILDDIR

apt install libavahi-client-dev libavahi-common-dev cmake libsqlite3-dev libboost-all-dev avahi-daemon libboost-dev libboost-log-dev libboost-program-options-dev

cmake $BASEDIR

make
