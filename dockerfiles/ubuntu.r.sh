#!/bin/bash

pushd /tempdir
apt update 
DEBIAN_FRONTEND=noninteractive apt-get install -y r-base wget pkg-config libcurl4-openssl-dev git
git clone --recursive --branch v1.24.3 --depth 1 https://github.com/grpc/grpc.git
pushd /tempdir/grpc/third_party/protobuf
./autogen.sh
./configure
make -j
make install
popd
pushd /tempdir/grpc
make -j
make install
popd

mkdir -p /clientdir
apt-get clean
popd

