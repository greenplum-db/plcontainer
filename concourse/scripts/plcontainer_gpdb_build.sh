#!/bin/bash

set -x

GPDBBIN=$1
OUTPUT=$2
MODE=$3
mkdir /usr/local/greenplum-db-devel
cp $GPDBBIN/bin_gpdb.tar.gz /usr/local/greenplum-db-devel/
pushd /usr/local/greenplum-db-devel/
tar zxvf bin_gpdb.tar.gz
popd
source /usr/local/greenplum-db-devel/greenplum_path.sh

if [ "$MODE" == "test" ]; then
    export CIBUILD=1
fi

pushd plcontainer_src
make clean
pushd package
make cleanall && make
popd
popd

if [ "$MODE" == "test" ]; then
    cp plcontainer_src/package/plcontainer-*.gppkg $OUTPUT/plcontainer-concourse.gppkg
else
    cp plcontainer_src/package/plcontainer-*.gppkg $OUTPUT/
fi
