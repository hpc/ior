#!/bin/bash

# Test basic S3 behavior using minio.

ROOT="$(dirname ${BASH_SOURCE[0]})"
TYPE="basic"

if [[ ! -e $ROOT/minio ]] ; then
  wget https://dl.min.io/server/minio/release/linux-amd64/minio
  mv minio $ROOT
  chmod +x $ROOT/minio
fi

export MINIO_ACCESS_KEY=accesskey
export MINIO_SECRET_KEY=secretkey

$ROOT/minio --quiet server /dev/shm &

export IOR_EXTRA="-o test"
export MDTEST_EXTRA="-d test"
source $ROOT/test-lib.sh

I=100 # Start with this ID
IOR 2 -a S3-libs3 --S3.host=localhost:9000  --S3.secret-key=secretkey --S3.access-key=accesskey -b $((10*1024*1024)) -t $((10*1024*1024))
MDTEST 2 -a S3-libs3 --S3.host=localhost:9000  --S3.secret-key=secretkey --S3.access-key=accesskey -n 10

IOR 1 -a S3-libs3 --S3.host=localhost:9000  --S3.secret-key=secretkey --S3.access-key=accesskey -b $((10*1024)) -t $((10*1024)) --S3.bucket-per-file
MDTEST 1 -a S3-libs3 --S3.host=localhost:9000  --S3.secret-key=secretkey --S3.access-key=accesskey --S3.bucket-per-file -n 10


kill -9 %1
