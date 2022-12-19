#!/bin/bash

mkdir aws-s3
cd aws-s3
INSTALL=$PWD/install

# The build instructions stem from the README in https://github.com/awslabs/aws-c-s3, only the INSTALL var is modified

git clone git@github.com:awslabs/aws-lc.git
cmake -S aws-lc -B aws-lc/build -DCMAKE_INSTALL_PREFIX=$INSTALL
cmake --build aws-lc/build --target install

git clone git@github.com:aws/s2n-tls.git
cmake -S s2n-tls -B s2n-tls/build -DCMAKE_INSTALL_PREFIX=$INSTALL -DCMAKE_PREFIX_PATH=$INSTALL
cmake --build s2n-tls/build --target install

git clone git@github.com:awslabs/aws-c-common.git
cmake -S aws-c-common -B aws-c-common/build -DCMAKE_INSTALL_PREFIX=$INSTALL
cmake --build aws-c-common/build --target install

git clone git@github.com:awslabs/aws-checksums.git
cmake -S aws-checksums -B aws-checksums/build -DCMAKE_INSTALL_PREFIX=$INSTALL -DCMAKE_PREFIX_PATH=$INSTALL
cmake --build aws-checksums/build --target install

git clone git@github.com:awslabs/aws-c-cal.git
cmake -S aws-c-cal -B aws-c-cal/build -DCMAKE_INSTALL_PREFIX=$INSTALL -DCMAKE_PREFIX_PATH=$INSTALL
cmake --build aws-c-cal/build --target install

git clone git@github.com:awslabs/aws-c-io.git
cmake -S aws-c-io -B aws-c-io/build -DCMAKE_INSTALL_PREFIX=$INSTALL -DCMAKE_PREFIX_PATH=$INSTALL
cmake --build aws-c-io/build --target install

git clone git@github.com:awslabs/aws-c-compression.git
cmake -S aws-c-compression -B aws-c-compression/build -DCMAKE_INSTALL_PREFIX=$INSTALL -DCMAKE_PREFIX_PATH=$INSTALL
cmake --build aws-c-compression/build --target install

git clone git@github.com:awslabs/aws-c-http.git
cmake -S aws-c-http -B aws-c-http/build -DCMAKE_INSTALL_PREFIX=$INSTALL -DCMAKE_PREFIX_PATH=$INSTALL
cmake --build aws-c-http/build --target install

git clone git@github.com:awslabs/aws-c-sdkutils.git
cmake -S aws-c-sdkutils -B aws-c-sdkutils/build -DCMAKE_INSTALL_PREFIX=$INSTALL -DCMAKE_PREFIX_PATH=$INSTALL
cmake --build aws-c-sdkutils/build --target install

git clone git@github.com:awslabs/aws-c-auth.git
cmake -S aws-c-auth -B aws-c-auth/build -DCMAKE_INSTALL_PREFIX=$INSTALL -DCMAKE_PREFIX_PATH=$INSTALL
cmake --build aws-c-auth/build --target install

git clone git@github.com:awslabs/aws-c-s3.git
cmake -S aws-c-s3 -B aws-c-s3/build -DCMAKE_INSTALL_PREFIX=$INSTALL -DCMAKE_PREFIX_PATH=$INSTALL
cmake --build aws-c-s3/build --target install
