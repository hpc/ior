#!/bin/bash

# Test script for basic IOR functionality testing various patterns
# It is kept as simple as possible and outputs the parameters used such that any test can be rerun easily.

# You can override the defaults by setting the variables before invoking the script, or simply set them here...
# Example: export IOR_EXTRA="-v -v -v"

ROOT="$(dirname ${BASH_SOURCE[0]})"

source $ROOT/test-lib.sh

MDTEST 1 -a POSIX
MDTEST 2 -a POSIX -W 2

IOR 1 -a POSIX -w    -z                  -F -Y -e -i1 -m -t 100k -b 1000k
IOR 1 -a POSIX -w    -z                  -F -k -e -i2 -m -t 100k -b 100k
IOR 1 -a MMAP -r    -z                  -F -k -e -i1 -m -t 100k -b 100k

IOR 2 -a POSIX -w    -z  -C             -F -k -e -i1 -m -t 100k -b 100k
IOR 2 -a POSIX -w    -z  -C -Q 1        -F -k -e -i1 -m -t 100k -b 100k
IOR 2 -a POSIX -r    -z  -Z -Q 2        -F -k -e -i1 -m -t 100k -b 100k
IOR 2 -a POSIX -r    -z  -Z -Q 3 -X  13 -F -k -e -i1 -m -t 100k -b 100k
IOR 2 -a POSIX -w    -z  -Z -Q 1 -X -13 -F    -e -i1 -m -t 100k -b 100k


IOR 2 -f "$ROOT/test_comments.ior"

END
