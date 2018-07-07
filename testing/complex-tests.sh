#!/bin/bash

# Test script for more complex IOR functionality testing various patterns
# You can override the defaults by setting the variables before invoking the script, or simply set them here...
# Example: export IOR_EXTRA="-v -v -v"

ROOT=${0%/*}

source $ROOT/test-lib.sh

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
