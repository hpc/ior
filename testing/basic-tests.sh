#!/bin/bash

# Test script for basic IOR functionality testing various patterns
# It is kept as simple as possible and outputs the parameters used such that any test can be rerun easily.

# You can override the defaults by setting the variables before invoking the script, or simply set them here...
# Example: export IOR_EXTRA="-v -v -v"

ROOT="$(dirname ${BASH_SOURCE[0]})"
TYPE="basic"

source $ROOT/test-lib.sh


IOR 2 -a POSIX -w     -C              -k -e -i1 -m -t 100k -b 200k
# Random read the file previously created
IOR 2 -a POSIX -r                     -k -e -i1 -m -t 100k -b 200k -s 10 -z -z

exit 1

MDTEST 1 -a POSIX
MDTEST 2 -a POSIX -W 2
MDTEST 1 -C -T -r -F -I 1 -z 1 -b 1 -L -u
MDTEST 1 -C -T -I 1 -z 1 -b 1 -u
MDTEST 2 -n 1 -f 1 -l 2

IOR 1 -a POSIX -w    -z                  -F -Y -e -i1 -m -t 100k -b 2000k
IOR 1 -a POSIX -w    -z                  -F -k -e -i2 -m -t 100k -b 200k
IOR 1 -a MMAP -r    -z                  -F -k -e -i1 -m -t 100k -b 200k

IOR 2 -a POSIX -w     -C              -k -e -i1 -m -t 100k -b 200k
# Random read the file previously created
IOR 2 -a POSIX -r                     -k -e -i1 -m -t 100k -b 200k -s 10 -z -z


IOR 2 -a POSIX -w    -z  -C             -F -k -e -i1 -m -t 100k -b 200k
IOR 2 -a POSIX -w    -z  -C -Q 1        -F -k -e -i1 -m -t 100k -b 200k
IOR 2 -a POSIX -r    -z  -Z -Q 2        -F -k -e -i1 -m -t 100k -b 200k
IOR 2 -a POSIX -r    -z  -Z -Q 3 -X  13 -F -k -e -i1 -m -t 100k -b 200k
IOR 3 -a POSIX -w    -z  -Z -Q 1 -X -13 -F    -e -i1 -m -t 100k -b 200k

IOR 2 -f "$ROOT/test_comments.ior"

# Test for JSON output
IOR 2 -a DUMMY -e -F -t 1m -b 1m -A 328883 -O summaryFormat=JSON -O summaryFile=OUT.json
python -mjson.tool OUT.json >/dev/null  && echo "JSON OK"

# MDWB
MDWB 3 -a POSIX -O=1 -D=1 -G=10 -P=1 -I=1 -R=2 -X
MDWB 3 -a POSIX -O=1 -D=4 -G=10 -P=4 -I=1 -R=2 -X -t=0.001 -L=latency.txt
MDWB 3 -a POSIX -O=1 -D=2 -G=10 -P=4 -I=3 -R=2 -X -W -w 1
MDWB 3 -a POSIX -O=1 -D=2 -G=10 -P=4 -I=3 -1 -W -w 1 --run-info-file=mdw.tst --print-detailed-stats
MDWB 3 -a POSIX -O=1 -D=2 -G=10 -P=4 -I=3 -2 -W -w 1 --run-info-file=mdw.tst --print-detailed-stats
MDWB 3 -a POSIX -O=1 -D=2 -G=10 -P=4 -I=3 -2 -W -w 1 --read-only --run-info-file=mdw.tst --print-detailed-stats
MDWB 3 -a POSIX -O=1 -D=2 -G=10 -P=4 -I=3 -2 -W -w 1 --read-only --run-info-file=mdw.tst --print-detailed-stats
MDWB 3 -a POSIX -O=1 -D=2 -G=10 -P=4 -I=3 -3 -W -w 1 --run-info-file=mdw.tst --print-detailed-stats

MDWB 2 -a POSIX -O=1 -D=1 -G=3 -P=2 -I=2 -R=2 -X -S 772 --dataPacketType=t
DELETE=0
MDWB 2 -a POSIX -D=1 -P=2 -I=2 -R=2 -X -G=2252 -S 772 --dataPacketType=i -1 
MDWB 2 -a POSIX -D=1 -P=2 -I=2 -R=2 -X -G=2252 -S 772 --dataPacketType=i -2
MDWB 2 -a POSIX -D=1 -P=2 -I=2 -R=2 -X -G=2252 -S 772 --dataPacketType=i -3
END
