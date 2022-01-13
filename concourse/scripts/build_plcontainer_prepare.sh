#!/bin/bash

#------------------------------------------------------------------------------
#
# Copyright (c) 2017-Present Pivotal Software, Inc
#
#------------------------------------------------------------------------------

set -exo pipefail

CWDIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
TOP_DIR=${CWDIR}/../../../
source "${TOP_DIR}/gpdb_src/concourse/scripts/common.bash"

function _main() {
  # setup gpdb environment
  install_gpdb

  if [ "${BLD_OS}" = "rhel8" ]; then
    # build json-c staticly
    git clone https://github.com/json-c/json-c.git
    cd json-c
    git fetch --all --tags
    git checkout tags/json-c-0.15-20200726
    mkdir build && cd build
    cmake .. -DCMAKE_INSTALL_PREFIX="$(readlink -f .)" \
             -DBUILD_STATIC_LIBS=yes \
             -DBUILD_SHARED_LIBS=no \
             -DCMAKE_BUILD_TYPE=Release
    make -j$(nproc)

    export PKG_CONFIG_LIBDIR="$(readlink -f .)"

    cd ../..
  fi
  ln -s /usr/local/greenplum-db-devel /usr/local/greenplum-db

  # gpadmin need have write permission on TOP_DIR.
  # we use chmod instead of chown -R, due to concourse known issue.
  chmod a+w ${TOP_DIR}
  find ${TOP_DIR} -type d -exec chmod a+w {} \;

  export OUTPUT=${OUTPUT}
  export DEV_RELEASE=${DEV_RELEASE}

  bash ${CWDIR}/build_plcontainer.sh $(pwd)
}

_main "$@"
