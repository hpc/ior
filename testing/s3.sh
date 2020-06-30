#!/bin/bash

# Test basic S3 behavior using minio.

ROOT="$(dirname ${BASH_SOURCE[0]})"
TYPE="basic"

cd $ROOT

if [[ ! -e minio ]] ; then
  wget https://dl.min.io/server/minio/release/linux-amd64/minio
  chmod +x minio
fi

export MINIO_ACCESS_KEY=accesskey
export MINIO_SECRET_KEY=secretkey

./minio --quiet server /dev/shm &

source $ROOT/test-lib.sh
IOR 2 -a S3-libs3 --S3.host=localhost:9000  --S3.secret-key=secretkey --S3.access-key=accesskey 
MDTEST 2 -a S3-libs3 --S3.host=localhost:9000  --S3.secret-key=secretkey --S3.access-key=accesskey 

kill -9 %1
