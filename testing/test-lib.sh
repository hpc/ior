# Test script for basic IOR functionality testing various patterns
# It is kept as simple as possible and outputs the parameters used such that any
# test can be rerun easily.

# You can override the defaults by setting the variables before invoking the
# script, or simply set them here...
# Example: export IOR_EXTRA="-v -v -v"

IOR_MPIRUN=${IOR_MPIRUN:-mpiexec -np}
IOR_BIN_DIR=${IOR_BIN_DIR:-./src}
IOR_OUT=${IOR_OUT:-./test_logs}
IOR_TMP=${IOR_TMP:-/dev/shm}
IOR_EXTRA=${IOR_EXTRA:-} # Add global options like verbosity
MDTEST_EXTRA=${MDTEST_EXTRA:-}
MDTEST_TEST_PATTERNS=${MDTEST_TEST_PATTERNS:-../testing/mdtest-patterns/$TYPE}

################################################################################
mkdir -p ${IOR_OUT}
mkdir -p ${IOR_TMP}/mdest

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
  WHAT="${IOR_MPIRUN} $RANKS ${IOR_BIN_DIR}/ior ${@} ${IOR_EXTRA} -o ${IOR_TMP}/ior"
  $WHAT 1>"${IOR_OUT}/test_out.$I" 2>&1
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
  rm -rf ${IOR_TMP}/mdest
  WHAT="${IOR_MPIRUN} $RANKS ${IOR_BIN_DIR}/mdtest ${@} ${MDTEST_EXTRA} -d ${IOR_TMP}/mdest -V=4"
  $WHAT 1>"${IOR_OUT}/test_out.$I" 2>&1
  if [[ $? != 0 ]]; then
    echo -n "ERR"
    ERRORS=$(($ERRORS + 1))
  else
    # compare basic pattern
    if [[ -r ${MDTEST_TEST_PATTERNS}/$I.txt ]] ; then
      grep "V-3" "${IOR_OUT}/test_out.$I" > "${IOR_OUT}/tmp"
      cmp -s "${IOR_OUT}/tmp" ${MDTEST_TEST_PATTERNS}/$I.txt
      if [[ $? != 0 ]]; then
        mv "${IOR_OUT}/tmp" ${IOR_OUT}/tmp.$I
        echo -n "Pattern differs! check: diff -u ${MDTEST_TEST_PATTERNS}/$I.txt ${IOR_OUT}/tmp.$I "
      fi
    else
      if [[ ! -e ${MDTEST_TEST_PATTERNS} ]] ; then
        mkdir -p ${MDTEST_TEST_PATTERNS}
      fi
      grep "V-3" "${IOR_OUT}/test_out.$I" > ${MDTEST_TEST_PATTERNS}/$I.txt
    fi
    echo -n "OK "
  fi
  echo " $WHAT"
  I=$((${I}+1))
}

function END(){
  if [[ ${ERRORS} == 0 ]] ; then
    echo "PASSED"
  else
    echo "Error, check the output files!"
  fi

  exit ${ERRORS}
}
