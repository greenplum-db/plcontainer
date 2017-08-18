#!/bin/bash

set -e -x

# Install R
yum -y install R R-devel R-core

# Clean up and creating target directories
rm -rf plcontainer_clients_build/python26 && mkdir plcontainer_clients_build/python26
rm -rf plcontainer_clients_build/python27 && mkdir plcontainer_clients_build/python27
rm -rf plcontainer_clients_build/r32 && mkdir plcontainer_clients_build/r32

# Build Python 2.7 and R 3.2 - default ones
pushd plcontainer_src
export R_HOME=/usr/lib64/R
pushd src/pyclient
make clean && make
popd
pushd src/rclient
make clean && make
popd
cp src/pyclient/client     ../plcontainer_clients_build/python27/client
cp src/rclient/client      ../plcontainer_clients_build/r32/client
cp src/rclient/librcall.so ../plcontainer_clients_build/r32/librcall.so
popd
unset R_HOME

# Build Python 2.6 and R 3.1 - the ones shipped with GPDB
mkdir /usr/local/greenplum-db
cp bin_gpdb/bin_gpdb.tar.gz /usr/local/greenplum-db/
pushd /usr/local/greenplum-db/
tar zxvf bin_gpdb.tar.gz
popd
source /usr/local/greenplum-db/greenplum_path.sh

pushd plcontainer_src
pushd src/pyclient
make clean && make
popd
cp src/pyclient/client     ../plcontainer_clients_build/python26/client
popd
