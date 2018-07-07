#!/bin/bash

BUILD="$1"
groupadd -g $3 testuser
useradd -r -u $2 -g testuser testuser
ERROR=0

function runTest(){
  FLAVOR="$1"
  MPI_DIR="$2"
	export IOR_MPIRUN="$3"
  echo $FLAVOR in $BUILD/$FLAVOR
  update-alternatives --set mpi $MPI_DIR
	sudo -u testuser mkdir -p $BUILD/$FLAVOR

	pushd $BUILD/$FLAVOR > /dev/null
  sudo -u testuser /data/configure || exit 1
  sudo -u testuser make || exit 1

  #define the alias
  ln -sf $(which mpiexec.$FLAVOR) /usr/bin/mpiexec

	cd /data/

	sudo -u testuser IOR_BIN_DIR=$BUILD/$FLAVOR/src IOR_OUT=$BUILD/$FLAVOR/test ./testing/basic-tests.sh

  ERROR=$(($ERROR + $?))
  popd  > /dev/null
}

export MPI_ARGS=""
runTest openmpi /usr/lib/openmpi/include "mpiexec -n"
runTest mpich /usr/include/mpich "mpiexec -n"

exit $ERROR
