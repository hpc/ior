#!/bin/bash -e

mkdir build-pnetcdf
cd build-pnetcdf
VER=pnetcdf-1.12.2.tar.gz
if [[ ! -e $VER ]] ; then
  wget https://parallel-netcdf.github.io/Release/$VER
  tar -xf $VER
  CUR_DIR=$(pwd)
  pushd pnetcdf*/
  ./configure --prefix=$CUR_DIR/install
  make -j install
  popd
fi

../configure --with-ncmpi LDFLAGS=-L$(pwd)/install/lib CFLAGS=-I$(pwd)/install/include
