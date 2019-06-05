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
  DEBIAN_FRONTEND=noninteractive apt install -y r-base pkg-config libpython2.7-dev python2.7

  # build client only
  pushd plcontainer_src

  # Plcontainer version
  PLCONTAINER_VERSION=$(git describe)
  echo "#define PLCONTAINER_VERSION \"${PLCONTAINER_VERSION}\"" > src/common/config.h

  make CFLAGS='-Werror -Wextra -Wall -Wno-sign-compare -O3' -C src/pyclient all
  make CFLAGS='-Werror -Wextra -Wall -Wno-sign-compare -O3' -C src/rclient all

  pushd src/pyclient/bin
  tar czf pyclient.tar.gz *
  popd
  mv src/pyclient/bin/pyclient.tar.gz .
  pushd src/rclient/bin
  tar czf rclient.tar.gz *
  popd
  mv src/rclient/bin/rclient.tar.gz .

  tar czf plcontainer_client.tar.gz pyclient.tar.gz rclient.tar.gz

  mkdir -p ../plcontainer_client
  cp plcontainer_client.tar.gz ../plcontainer_client/

  popd
}

_main "$@"
