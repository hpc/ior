#!/bin/bash

# Test script for basic IOR functionality testing various patterns
# It is kept as simple as possible and outputs the parameters used such that any test can be rerun easily.

# You can override the defaults by setting the variables before invoking the script, or simply set them here...
# Example: export IOR_EXTRA="-v -v -v"

IOR_MPIRUN=${IOR_MPIRUN:-mpiexec -np}
IOR_EXEC=${IOR_EXEC:-./build/src/ior}
IOR_OUT=${IOR_OUT:-./build/test}
IOR_EXTRA=${IOR_EXTRA:-./build/test} # Add global options like verbosity

################################################################################
mkdir -p ${IOR_OUT}

## Sanity check

if [[ ! -e ${IOR_OUT} ]]; then
  echo "Could not create output dir ${IOR_OUT}"
  exit 1
fi

if [[ ! -e $IOR_EXEC ]]; then
  echo "IOR Executable \"$IOR_EXEC\" does not exist! Call me from the root directory!"
  exit 1
fi


ERRORS=0 # Number of errors detected while running
I=0
function TEST(){
  ${IOR_MPIRUN} ${@} ${IOR_EXTRA} 1>${IOR_OUT}/$I 2>&1
  if [[ $? != 0 ]]; then
    echo -n "ERR"
    ERRORS=$(($ERRORS + 1))
  else
    echo -n "OK "
  fi
  echo " ${IOR_OUT}/${I} ${IOR_MPIRUN} ${@} -o /dev/shm/ior"
  I=$((${I}+1))
}

TEST 1 ${IOR_EXEC} -a POSIX -w    -z                  -F -Y -e -i1 -m -t 100k -b 1000k
TEST 1 ${IOR_EXEC} -a POSIX -w    -z                  -F -k -e -i2 -m -t 100k -b 100k
TEST 1 ${IOR_EXEC} -a POSIX -r    -z                  -F -k -e -i1 -m -t 100k -b 100k

TEST 2 ${IOR_EXEC} -a POSIX -w    -z  -C              -F -k -e -i1 -m -t 100k -b 100k
TEST 2 ${IOR_EXEC} -a POSIX -w    -z  -C -Q 1        -F -k -e -i1 -m -t 100k -b 100k
TEST 2 ${IOR_EXEC} -a POSIX -r    -z  -Z -Q 2        -F -k -e -i1 -m -t 100k -b 100k
TEST 2 ${IOR_EXEC} -a POSIX -r    -z  -Z -Q 3 -X  13 -F -k -e -i1 -m -t 100k -b 100k
TEST 2 ${IOR_EXEC} -a POSIX -w    -z  -Z -Q 1 -X -13 -F    -e -i1 -m -t 100k -b 100k

#shared tests
TEST 2 ${IOR_EXEC} -a POSIX -w    -z                     -Y -e -i1 -m -t 100k -b 100k
TEST 2 ${IOR_EXEC} -a POSIX -w                            -k -e -i1 -m -t 100k -b 100k
TEST 2 ${IOR_EXEC} -a POSIX -r    -z                      -k -e -i1 -m -t 100k -b 100k

#test mutually exclusive options
TEST 2 ${IOR_EXEC} -a POSIX -w    -z                    -k -e -i1 -m -t 100k -b 100k
TEST 2 ${IOR_EXEC} -a POSIX -w    -z  -                 -k -e -i1 -m -t 100k -b 100k
TEST 2 ${IOR_EXEC} -a POSIX -w -Z                             -i1 -m -t 100k -b 100k  -d 0.1

if [[ ${ERRORS} == 0 ]] ; then
  echo "PASSED"
else
  echo "Error, check the output files!"
fi

exit ${ERRORS}
