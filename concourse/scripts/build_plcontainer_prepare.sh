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

  # install json-c
  yum install -y json-c-devel

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
