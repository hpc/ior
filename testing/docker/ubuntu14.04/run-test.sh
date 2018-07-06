#!/bin/bash

BUILD="$1"
if [[ $UID == 0 ]]; then
  groupadd -g $3 testuser
  useradd -r -u $2 -g testuser testuser
  sudo -u testuser $0 $1
  exit $?
fi
ERROR=0

function runTest(){
  FLAVOR="$1"
  MPI_DIR="$2"
  echo $FLAVOR in $BUILD/$FLAVOR
  update-alternatives --set mpi $MPI_DIR
	mkdir -p $BUILD/$FLAVOR

	pushd $BUILD/$FLAVOR > /dev/null
  /data/configure || exit 1
  make || exit 1

  #define the alias
  ln -sf $(which mpiexec.$FLAVOR) /usr/bin/mpiexec

	cd /data/
	export IOR_EXEC=$BUILD/$FLAVOR/src/ior
	export IOR_OUT=$BUILD/$FLAVOR/test
	./testing/basic-tests.sh

  ERROR=$(($ERROR + $?))
  popd  > /dev/null
}

runTest openmpi /usr/lib/openmpi/include
runTest mpich /usr/include/mpich

exit $ERROR
