#!/bin/bash -x

#PBS -N IOR
#PBS -j oe
#PBS -q batch
#PBS -A stf006
#PBS -V
#PBS -l walltime=0:60:00,nodes=8:ppn=2

VERS=IOR-2.10.1
WORK=/tmp/work/${USER}
echo $PBS_O_WORKDIR

cd /ccs/proj/quadcore
tar -czvf ${WORK}/${VERS}.tar.gz ./${VERS}
cd ${WORK}
rm -fr ./${VERS}
tar -xzvf ${WORK}/${VERS}.tar.gz
cd ${WORK}/${VERS}
gmake clean
gmake mpiio
EXEC=${WORK}/${VERS}/src/C/IOR
IODIR=/tmp/work/swh13/test_files_x
cd ${WORK}/${VERS}/tests

which mpirun

rm -fr $IODIR
mkdir  $IODIR

let "w=128"
let "s=1024*1024"
let "i=3"

MPIRUN="mpirun -np"

RESULTS="."

let "tid=1"
XFERS="1048576 262144 32768 4096 1024"
XFERS="262144"
for xfer in `echo $XFERS`
do
       let "n=8"
until [ "$n" -gt 8 ]
do

    let "m=$n/4"
  #TESTS="POSIX MPIIO HDF5 NCMPI"
  TESTS="POSIX MPIIO"
  for test in `echo $TESTS`
  do
    runid="p$n.$xfer.${test}"
    date 

    V="  "
    BLOCKS="1 10 1 10 1 10"
    for blocks in `echo $BLOCKS`
    do

      let "block=${xfer} * ${blocks}"

    #fileperproc tests
    ${MPIRUN} $n ${EXEC} -A ${tid} -a ${test} -w    -z                  ${V} -F -o $IODIR/testwrite.${runid} -Y -e -i${i} -m -t ${xfer} -b ${block}  -d 0.1 
    ${MPIRUN} $n ${EXEC} -A ${tid} -a ${test} -w    -z                  ${V} -F -o $IODIR/testwrite.${runid} -k -e -i${i} -m -t ${xfer} -b ${block}  -d 0.1 
    ${MPIRUN} $n ${EXEC} -A ${tid} -a ${test} -r    -z                  ${V} -F -o $IODIR/testwrite.${runid} -k -e -i${i} -m -t ${xfer} -b ${block}  -d 0.1 
    ${MPIRUN} $n ${EXEC} -A ${tid} -a ${test} -r    -z  -C              ${V} -F -o $IODIR/testwrite.${runid} -k -e -i${i} -m -t ${xfer} -b ${block}  -d 0.1 
    ${MPIRUN} $n ${EXEC} -A ${tid} -a ${test} -r    -z  -C -Q $m        ${V} -F -o $IODIR/testwrite.${runid} -k -e -i${i} -m -t ${xfer} -b ${block}  -d 0.1 
    ${MPIRUN} $n ${EXEC} -A ${tid} -a ${test} -r    -z  -Z -Q $m        ${V} -F -o $IODIR/testwrite.${runid} -k -e -i${i} -m -t ${xfer} -b ${block}  -d 0.1 
    ${MPIRUN} $n ${EXEC} -A ${tid} -a ${test} -r    -z  -Z -Q $m -X  13 ${V} -F -o $IODIR/testwrite.${runid} -k -e -i${i} -m -t ${xfer} -b ${block}  -d 0.1 
    ${MPIRUN} $n ${EXEC} -A ${tid} -a ${test} -r    -z  -Z -Q $m -X -13 ${V} -F -o $IODIR/testwrite.${runid}    -e -i${i} -m -t ${xfer} -b ${block}  -d 0.1 

    #shared tests
    ${MPIRUN} $n ${EXEC} -A ${tid} -a ${test} -w    -z                  ${V}    -o $IODIR/testwrite.${runid} -Y -e -i${i} -m -t ${xfer} -b ${block}  -d 0.1 
    ${MPIRUN} $n ${EXEC} -A ${tid} -a ${test} -w                        ${V}    -o $IODIR/testwrite.${runid} -k -e -i${i} -m -t ${xfer} -b ${block}  -d 0.1 
    ${MPIRUN} $n ${EXEC} -A ${tid} -a ${test} -r    -z                  ${V}    -o $IODIR/testwrite.${runid} -k -e -i${i} -m -t ${xfer} -b ${block}  -d 0.1 

    #test mutually exclusive options
    ${MPIRUN} $n ${EXEC} -A ${tid} -a ${test} -r    -z  -C              ${V}    -o $IODIR/testwrite.${runid} -k -e -i${i} -m -t ${xfer} -b ${block}  -d 0.1 
    ${MPIRUN} $n ${EXEC} -A ${tid} -a ${test} -r    -z  -Z              ${V}    -o $IODIR/testwrite.${runid} -k -e -i${i} -m -t ${xfer} -b ${block}  -d 0.1 
    ${MPIRUN} $n ${EXEC} -A ${tid} -a ${test} -r -Z -C                  ${V}    -o $IODIR/testwrite.${runid}       -i${i} -m -t ${xfer} -b ${block}  -d 0.0 
    let "tid=$tid + 17"

    V=$V" -v"

    done #blocks

    date 
  done #test
  let "n = $n * 2"
 done #n
done #xfer
exit
