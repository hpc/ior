#!/bin/bash

# Test script for basic IOR functionality testing various patterns
# It is kept as simple as possible and outputs the parameters used such that any test can be rerun easily.

# You can override the defaults by setting the variables before invoking the script, or simply set them here...
# Example: export IOR_EXTRA="-v -v -v"

IOR_MPIRUN=${IOR_MPIRUN:-mpiexec -np}
IOR_BIN_DIR=${IOR_EXEC:-./build/src}
IOR_OUT=${IOR_OUT:-./build/test}
IOR_EXTRA=${IOR_EXTRA:-} # Add global options like verbosity
MDTEST_EXTRA=${MDTEST_EXTRA:-}

################################################################################
mkdir -p ${IOR_OUT}

## Sanity check

if [[ ! -e ${IOR_OUT} ]]; then
  echo "Could not create output dir ${IOR_OUT}"
  exit 1
fi

if [[ ! -e ${IOR_BIN_DIR}/ior ]]; then
  echo "IOR Executable \"${IOR_BIN_DIR}/ior\" does not exist! Call me from the root directory!"
  exit 1
fi

if [[ ! -e ${IOR_BIN_DIR}/mdtest ]]; then
  echo "MDTest Executable \"${IOR_BIN_DIR}/mdtest\" does not exist! Call me from the root directory!"
  exit 1
fi



ERRORS=0 # Number of errors detected while running
I=0
function IOR(){
  RANKS=$1
  shift
  WHAT="${IOR_MPIRUN} $RANKS ${IOR_BIN_DIR}/ior ${@} ${IOR_EXTRA} -o /dev/shm/ior"
  $WHAT 1>${IOR_OUT}/$I 2>&1
  if [[ $? != 0 ]]; then
    echo -n "ERR"
    ERRORS=$(($ERRORS + 1))
  else
    echo -n "OK "
  fi
  echo " $WHAT"
  I=$((${I}+1))
}

function MDTEST(){
  RANKS=$1
  shift
  WHAT="${IOR_MPIRUN} $RANKS ${IOR_BIN_DIR}/mdtest ${@} ${MDTEST_EXTRA} -d /dev/shm/ior"
  $WHAT 1>${IOR_OUT}/$I 2>&1
  if [[ $? != 0 ]]; then
    echo -n "ERR"
    ERRORS=$(($ERRORS + 1))
  else
    echo -n "OK "
  fi
  echo " $WHAT"
  I=$((${I}+1))
}

MDTEST 1 -a POSIX

IOR 1 -a POSIX -w    -z                  -F -Y -e -i1 -m -t 100k -b 1000k
IOR 1 -a POSIX -w    -z                  -F -k -e -i2 -m -t 100k -b 100k
IOR 1 -a POSIX -r    -z                  -F -k -e -i1 -m -t 100k -b 100k

IOR 2 -a POSIX -w    -z  -C              -F -k -e -i1 -m -t 100k -b 100k
IOR 2 -a POSIX -w    -z  -C -Q 1        -F -k -e -i1 -m -t 100k -b 100k
IOR 2 -a POSIX -r    -z  -Z -Q 2        -F -k -e -i1 -m -t 100k -b 100k
IOR 2 -a POSIX -r    -z  -Z -Q 3 -X  13 -F -k -e -i1 -m -t 100k -b 100k
IOR 2 -a POSIX -w    -z  -Z -Q 1 -X -13 -F    -e -i1 -m -t 100k -b 100k

#shared tests
IOR 2 -a POSIX -w    -z                     -Y -e -i1 -m -t 100k -b 100k
IOR 2 -a POSIX -w                            -k -e -i1 -m -t 100k -b 100k
IOR 2 -a POSIX -r    -z                      -k -e -i1 -m -t 100k -b 100k

#test mutually exclusive options
IOR 2 -a POSIX -w    -z                    -k -e -i1 -m -t 100k -b 100k
IOR 2 -a POSIX -w    -z  -                 -k -e -i1 -m -t 100k -b 100k
IOR 2 -a POSIX -w -Z                             -i1 -m -t 100k -b 100k  -d 0.1

if [[ ${ERRORS} == 0 ]] ; then
  echo "PASSED"
else
  echo "Error, check the output files!"
fi

exit ${ERRORS}
