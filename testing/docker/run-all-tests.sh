#!/bin/bash

# This script runs the testscript for all supported docker images
cd "${0%/*}"
if [[ ! -e run-all-tests.sh ]] ; then
	echo "Error, this script must run from the ./testing/docker directory"
	exit 1
fi

TARGET=../../build-docker
mkdir -p $TARGET

ARGS="$@"
GID=$(id -g $USER)
OPT="-it --rm -v $PWD/../../:/data/:z"
ERROR=0
VERBOSE=0

set -- `getopt -u -l "clean" -l verbose -o "" -- "$ARGS"`
test $# -lt 1  && exit 1
while test $# -gt 0
do
	case "$1" in
		--clean) echo "Cleaning build dirs!"; rm -rf $TARGET/* ;;
		--verbose) VERBOSE=1 ;;
		--) ;;
		*) echo "Unknown option $1"; exit 1;;
	esac
	shift
done

for IMAGE in $(find -type d | cut -b 3- |grep -v "^$") ; do
	echo "RUNNING $IMAGE"
	mkdir -p $TARGET/$IMAGE
	WHAT="docker run $OPT -h $IMAGE hpc/ior:$IMAGE /data/testing/docker/$IMAGE/run-test.sh /data/build-docker/$IMAGE $UID $GID"
	if [[ $VERBOSE == 1 ]] ; then
		echo $WHAT
	fi
	$WHAT 2>$TARGET/$IMAGE/LastTest.log 1>&2
	ERR=$?
	ERROR=$(($ERROR+$ERR))
	if [[ $ERR != 0 ]]; then
		echo $WHAT
		echo "Error, see $TARGET/$IMAGE/LastTest.log"
	fi
done

if [[ $ERROR != 0 ]] ; then
	echo "Errors occured!"
else
	echo "OK: all tests passed!"
fi
