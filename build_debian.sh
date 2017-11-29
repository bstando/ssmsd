#!/bin/sh

if [ ! -d "build_debian" ]; then
	mkdir build_debian
fi

cd build
apt install libavahi-client-dev libavahi-common-dev cmake libsqlite3-dev libboost-all-dev avahi-daemon libboost-dev libboost-log-dev libboost-program-options-dev

cmake ..

make
