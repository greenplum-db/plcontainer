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
 
  # install R
  apt update
  DEBIAN_FRONTEND=noninteractive apt install -y r-base pkg-config libcurl4-openssl-dev libjson-c-dev libssl-dev

  # setup gpdb environment
  install_gpdb
  ${TOP_DIR}/gpdb_src/concourse/scripts/setup_gpadmin_user.bash "ubuntu"
  make_cluster
 
  ln -s /usr/local/greenplum-db-devel /usr/local/greenplum-db
  chown -h gpadmin:gpadmin /usr/local/greenplum-db

  chown -R gpadmin:gpadmin  $(pwd)
  chown -R gpadmin:supergroup /opt

  # gpadmin need have write permission on TOP_DIR. 
  # we use chmod instead of chown -R, due to concourse known issue.
  chmod a+w ${TOP_DIR}
  find ${TOP_DIR} -type d -exec chmod a+w {} \;
  chown gpadmin:gpadmin ${CWDIR}/build_plcontainer_ubuntu.sh
  su gpadmin -c "OUTPUT=${OUTPUT} \
                 DEV_RELEASE=${DEV_RELEASE} \
                 bash ${CWDIR}/build_plcontainer_ubuntu.sh $(pwd)"
}

_main "$@"
