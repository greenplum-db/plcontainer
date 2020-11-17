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

  # install R & PYTHON3
  apt-get update
  DEBIAN_FRONTEND=noninteractive apt-get -y install gnupg2 apt-utils
  DEBIAN_FRONTEND=noninteractive apt-get -y install --reinstall ca-certificates

  apt-key adv --keyserver keyserver.ubuntu.com --recv-keys E298A3A825C0D65DFD57CBB651716619E084DAB9
  echo 'deb https://cloud.r-project.org/bin/linux/ubuntu bionic-cran35/' >> /etc/apt/sources.list
  apt-get update
  DEBIAN_FRONTEND=noninteractive apt-get install -y r-base pkg-config libpython2.7-dev python2.7 python3.7-dev

  # build client only
  pushd plcontainer_src

  # Plcontainer version
  PLCONTAINER_VERSION=$(git describe --abbrev=0)
  echo "#define PLCONTAINER_VERSION \"${PLCONTAINER_VERSION}\"" > src/common/config.h

  make CFLAGS='-Werror -Wextra -Wall -Wno-sign-compare -O3 -g' -C src/pyclient all
  cp src/pyclient/bin/pyclient src/pyclient/bin/pyclient.bak
  make CFLAGS='-Werror -Wextra -Wall -Wno-sign-compare -O3 -g' -C src/pyclient clean
  make CFLAGS='-Werror -Wextra -Wall -Wno-sign-compare -O3 -g' PYTHON_VERSION=3 -C src/pyclient all
  mv src/pyclient/bin/pyclient.bak src/pyclient/bin/pyclient
  make CFLAGS='-Werror -Wextra -Wall -Wno-sign-compare -O3 -g' -C src/rclient all

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
