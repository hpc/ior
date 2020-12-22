#!/bin/bash
mkdir build-hdfs
cd build-hdfs

VER=hadoop-3.2.1
if [[ ! -e $VER.tar.gz ]] ; then
  wget https://www.apache.org/dyn/closer.cgi/hadoop/common/$VER/$VER.tar.gz
  tar -xf $VER.tar.gz
fi

../configure --with-hdfs CFLAGS=-I$PWD/$VER/include/  LDFLAGS="-L$PWD/$VER/lib/native -Wl,-rpath=$PWD/$VER/lib/native"
make -j
