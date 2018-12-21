#!/bin/bash

# Test script for timing of mdtest
# It also tests some more patterns

ROOT="$(dirname ${BASH_SOURCE[0]})"
TYPE="timer"
source $ROOT/test-lib.sh
VERBOSITY=""
CREATE_TEST_PATTERN="false"

# The accepted deviation of individual measurements
TOLERANCE=0.2

# Support functions to extract performance
function MDTEST_CMP_VAL(){
  EXPECTED=$(echo "$1" | sed "s/,/ /")
  WHAT="$2"
  RATE=$(grep "$WHAT" "$TESTFILE_OUT" | tail -n 2 | head -n 1 | awk '{print $6}' )
  TIME=$(grep "$WHAT" "$TESTFILE_OUT" | tail -n 1 | awk '{print $6}' )

  if [[ "$EXPECTED" == "0 0" ]] ; then
    CMP=$(echo $TIME $RATE | awk '{print ($1 == 0)}')
  else
    CMP=$(echo $EXPECTED $TIME $RATE | awk '{print (($3 <= $1 * 1.2) && ($3 >= $1 * 0.8) && ($4 <= $2 * 1.2) && ($4 >= $2 * 0.8))}')
  fi
  if [[ $CMP == 0 ]] ; then
    echo -n "\"$WHAT\" expected: $EXPECTED observed: $TIME $RATE "
    ERR=1
  fi
}

function MDTEST_TIMING(){
  C=$1; shift
  D=$1; shift
  T1=$1; shift
  T2=$1; shift
  T3=$1; shift
  T4=$1; shift
  #T5=$1; shift
  #T6=$1; shift
  MDTEST $C -a DUMMY --dummy.delay-create=$D -P $@
  ERR=0
  echo -n "TIMER "

  MDTEST_CMP_VAL $T1 "File creation"
  MDTEST_CMP_VAL $T2 "File stat"
  MDTEST_CMP_VAL $T3 "File read"
  MDTEST_CMP_VAL $T4 "File removal"
  #MDTEST_CMP_VAL $T5 "Tree creation"
  #MDTEST_CMP_VAL $T6 "Tree removal"
  if [[ "$ERR" == "0" ]] ; then
    echo "OK"
  else
    echo "ERR"
  fi
}

# The actual tests
#MDTEST_TIMING 1 1000  0.01,1000  0,0 0,0 0,0 -n 10 -F -L
#MDTEST_TIMING 2 1000  0.01,2000  0,0 0,0 0,0 -n 10 -F -L
#MDTEST_TIMING 4 1000  0.01,4000  0,0 0,0 0,0 -n 10 -F -L

MDTEST_TIMING 1 1000  1.0,900   0,0 0,0 0,0  -n 10000000 -b 3 -z 3 -L -F -u -s=2
MDTEST_TIMING 1 0  1.0,900   0,0 0,0 0,0 -n 10000  -C -T -b 2 -z 2 -I 2
MDTEST_TIMING 1 1000  1.0,900   0,0 0,0 0,0  -C -n 1000 -w 1000
MDTEST_TIMING 1    0  1.0,900   0,0 0,0 0,0  -C -n 1000 -w 1000

# Tests with Stonewall

MDTEST_TIMING 1 1000     1.0,900   0,0 0,0 0,0  -C -n 10000    -W 1 -F -L -u
MDTEST_TIMING 1 1000000  1.0,1   0,0 0,0 0,0  -C -n 10000    -W 1 -F
# one process races ahead, the other does 1 op/s
MDTEST_TIMING 2 1000000  3.0,2   0,0 0,0 0,0  -C -n 3    -W 1 -F --dummy.delay-only-rank0

MDTEST_TIMING 1 1000  1.0,900   0,0 0,0 0,0  -C -n 1000000000 -W 1 -w 1000
MDTEST_TIMING 1 1000  1.0,900   0,0 0,0 0,0  -C -n 10000000 -W 1 -b 1 -z 1

# -B is not valid with -W
# -b > 1 is not valid with -W
