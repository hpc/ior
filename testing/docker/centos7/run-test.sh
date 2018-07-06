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
  P=$PATH
  FLAVOR="$1"
  MPI_DIR="$2"

  echo $FLAVOR in $BUILD/$FLAVOR
	mkdir -p $BUILD/$FLAVOR

	pushd $BUILD/$FLAVOR > /dev/null
  
  export PATH=$MPI_DIR/bin:$PATH
  /data/configure || exit 1
  make || exit 1

	cd /data/
	export IOR_EXEC=$BUILD/$FLAVOR/src/ior
	export IOR_OUT=$BUILD/$FLAVOR/test
	./testing/basic-tests.sh

  ERROR=$(($ERROR + $?))
  popd  > /dev/null
  PATH=$P
}


runTest openmpi /usr/lib64/openmpi/
runTest mpich /usr/lib64/mpich

exit $ERROR
