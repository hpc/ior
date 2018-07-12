#!/bin/bash

# Test script for more complex IOR functionality testing various patterns
# You can override the defaults by setting the variables before invoking the script, or simply set them here...
# Example: export IOR_EXTRA="-v -v -v"

ROOT=${0%/*}

source $ROOT/test-lib.sh

#stonewalling tests
IOR 2 -a DUMMY -w -O stoneWallingStatusFile=stonewall.log -O stoneWallingWearOut=1 -D 1 -t 1000 -b 1000 -s 15
IOR 2 -a DUMMY -r -O stoneWallingStatusFile=stonewall.log -D 1 -t 1000 -b 1000 -s 30 # max 15 still!
IOR 2 -a DUMMY -r -O stoneWallingStatusFile=stonewall.log -t 1000 -b 1000 -s 30

MDTEST 2 -I 20 -a DUMMY -W 1 -x stonewall-md.log -C
MDTEST 2 -I 20 -a DUMMY -x stonewall-md.log -T -v
MDTEST 2 -I 20 -a DUMMY -x stonewall-md.log -D -v

#shared tests
IOR 2 -a POSIX -w -z -Y -e -i1 -m -t 100k -b 100k
IOR 2 -a POSIX -w -k -e -i1 -m -t 100k -b 100k
IOR 2 -a POSIX -r -z-k -e -i1 -m -t 100k -b 100k

#test mutually exclusive options
IOR 2 -a POSIX -w -z -k -e -i1 -m -t 100k -b 100k
IOR 2 -a POSIX -w -z -k -e -i1 -m -t 100k -b 100k
IOR 2 -a POSIX -w -Z -i1 -m -t 100k -b 100k -d 0.1

# Now set the num tasks per node to 1:
export IOR_FAKE_TASK_PER_NODES=1
IOR 2 -a POSIX -f $ROOT/bug-multi-node.conf

END
