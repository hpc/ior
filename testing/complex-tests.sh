#!/bin/bash

# Test script for more complex IOR functionality testing various patterns
# You can override the defaults by setting the variables before invoking the script, or simply set them here...
# Example: export IOR_EXTRA="-v -v -v"

ROOT=${0%/*}
TYPE="advanced"

source $ROOT/test-lib.sh

#stonewalling tests
IOR 2 -a DUMMY -w -O stoneWallingStatusFile=stonewall.log -O stoneWallingWearOut=1 -D 1 -t 1000 -b 1000 -s 15 -k
IOR 2 -a DUMMY -r -O stoneWallingStatusFile=stonewall.log -D 1 -t 1000 -b 1000 -s 30 -k # max 15 still!
IOR 2 -a DUMMY -r -O stoneWallingStatusFile=stonewall.log -t 1000 -b 1000 -s 30 -k

MDTEST 2 -I 20 -a DUMMY -W 1 -x stonewall-md.log -C
MDTEST 2 -I 20 -a DUMMY -x stonewall-md.log -T -v
MDTEST 2 -I 20 -a DUMMY -x stonewall-md.log -D -v

# Regression test: "-B" (no barriers between phases) must not be rejected
# by the stonewall-timer validation when no stonewall timer (-W) was
# requested. md_validate_tests_rank0() used to fail unconditionally
# whenever barriers were disabled, even without stonewalling:
#   Error, stone wall timer does only work with a branch factor <= 1
#   (current is 1) and with barriers
MDTEST 2 -a DUMMY -F -B 1 -n 4

# Negative control: stonewalling (-W) really is incompatible with having
# barriers disabled (-B), so this combination must still be rejected.
WHAT="${IOR_MPIRUN} 2 ${IOR_BIN_DIR}/mdtest -a DUMMY -F -B 1 -W 5 -n 4 -d ${IOR_TMP}/mdest ${MDTEST_EXTRA}"
$WHAT 1>"${IOR_OUT}/test_out.$I" 2>&1
if [[ $? == 0 ]]; then
  echo "ERR  -W with -B should have been rejected but succeeded: $WHAT"
  ERRORS=$(($ERRORS + 1))
else
  echo "OK (expected failure)  $WHAT"
fi
I=$((${I}+1))

#shared tests
IOR 2 -a POSIX -w -z -Y -e -i1 -m -t 100k -b 200k
IOR 2 -a POSIX -w -k -e -i1 -m -t 100k -b 200k
IOR 2 -a POSIX -r -z-k -e -i1 -m -t 100k -b 200k

#test mutually exclusive options
IOR 2 -a POSIX -w -z -k -e -i1 -m -t 100k -b 200k
IOR 2 -a POSIX -w -z -k -e -i1 -m -t 100k -b 200k
IOR 2 -a POSIX -w -Z -i1 -m -t 100k -b 100k -d 0.1

# Now set the num tasks per node to 1:
export IOR_FAKE_TASK_PER_NODES=1
IOR 2 -a POSIX -f $ROOT/bug-multi-node.conf

END
