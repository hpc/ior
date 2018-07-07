#!/bin/bash

BUILD="$1"

groupadd -g $3 testuser
useradd -r -u $2 -g testuser testuser
ERROR=0

function runTest(){
  P=$PATH
  FLAVOR="$1"
  MPI_DIR="$2"

  echo $FLAVOR in $BUILD/$FLAVOR
	sudo -u testuser mkdir -p $BUILD/$FLAVOR

	pushd $BUILD/$FLAVOR > /dev/null

  export PATH=$MPI_DIR/bin:$PATH
  sudo -u testuser PATH=$PATH /data/configure || exit 1
  sudo -u testuser PATH=$PATH make || exit 1

	cd /data/
	sudo -u testuser PATH=$PATH IOR_BIN_DIR=$BUILD/$FLAVOR/src  IOR_OUT=$BUILD/$FLAVOR/test ./testing/basic-tests.sh

  ERROR=$(($ERROR + $?))
  popd  > /dev/null
  PATH=$P
}


runTest openmpi /usr/lib64/openmpi/
runTest mpich /usr/lib64/mpich

exit $ERROR
