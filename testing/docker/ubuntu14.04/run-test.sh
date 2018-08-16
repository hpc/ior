#!/bin/bash

BUILD="$1"
groupadd -g $3 testuser
useradd -r -u $2 -g testuser testuser

ERROR=0

function runTest(){
  FLAVOR="$1"
  MPI_DIR="$2"
  echo $FLAVOR in $BUILD/$FLAVOR
  update-alternatives --set mpi $MPI_DIR
	sudo -u testuser mkdir -p $BUILD/$FLAVOR

	pushd $BUILD/$FLAVOR > /dev/null
  sudo -u testuser /data/configure --with-hdf5 CFLAGS=-I/usr/lib/x86_64-linux-gnu/hdf5/openmpi/include LDFLAGS=-L/usr/lib/x86_64-linux-gnu/hdf5/openmpi/lib|| exit 1
  sudo -u testuser make V=1 || exit 1

  #define the alias
  ln -sf $(which mpiexec.$FLAVOR) /usr/bin/mpiexec

	cd /data/
	sudo -u testuser IOR_BIN_DIR=$BUILD/$FLAVOR/src IOR_OUT=$BUILD/$FLAVOR/test ./testing/basic-tests.sh

  ERROR=$(($ERROR + $?))
  popd  > /dev/null
}

runTest openmpi /usr/lib/openmpi/include
runTest mpich /usr/include/mpich

exit $ERROR
