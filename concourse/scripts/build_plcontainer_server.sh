#!/bin/bash

#------------------------------------------------------------------------------
#
# Copyright (c) 2017-Present Pivotal Software, Inc
#
#------------------------------------------------------------------------------

set -exo pipefail

CWDIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
TOP_DIR=${CWDIR}/../../../

function _main() {

  # install R
  apt update
  DEBIAN_FRONTEND=noninteractive apt install -y r-base pkg-config git

  git clone --recursive --branch v1.24.3 --depth 1 https://github.com/grpc/grpc.git
  pushd ${TOP_DIR}/grpc/third_party/protobuf
  ./autogen.sh
  ./configure
  make -j
  make install
  export LD_LIBRARY_PATH=/usr/local/lib/:$LD_LIBRARY_PATH
  popd
  pushd ${TOP_DIR}/grpc/
  make -j
  make install
  popd

  # generate proto file firstly
  pushd plcontainer_src/
  make proto
  popd

  # build and pack R server  only
  pushd plcontainer_src/src/rclient

  make copy-proto

  # build server
  make

  # test server
  export R_HOME=/usr/lib/R
  make clone-gtest
  make test

  popd

  pushd plcontainer_src/src/rclient/bin
  tar czf rserver.tar.gz *
  popd
  pushd plcontainer_src/
  mv src/rclient/bin/rserver.tar.gz .

  # build and pack Python server
  #make CFLAGS='-Werror -Wextra -Wall -Wno-sign-compare -O3' -C src/rclient all
  #pushd src/pyclient/bin
  #tar czf pyclient.tar.gz *
  #popd
  #mv src/pyclient/bin/pyclient.tar.gz .

  tar czf plcontainer_server.tar.gz rserver.tar.gz

  mkdir -p ../plcontainer_server
  cp plcontainer_server.tar.gz ../plcontainer_server/

  popd
}

_main "$@"
