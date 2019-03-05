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

  # install json-c
  pushd ${TOP_DIR}
    git clone https://github.com/json-c/json-c.git
	cd json-c
	git checkout json-c-0.12.1-20160607
	./autogen.sh
	./configure
	make -j 4
	make install
  popd

  # setup gpdb environment
  install_gpdb
  ${TOP_DIR}/gpdb_src/concourse/scripts/setup_gpadmin_user.bash "centos"
  make_cluster
 
  ln -s /usr/local/greenplum-db-devel /usr/local/greenplum-db
  chown -h gpadmin:gpadmin /usr/local/greenplum-db
  
  # gpadmin need have write permission on TOP_DIR. 
  # we use chmod instead of chown -R, due to concourse known issue.
  chmod a+w ${TOP_DIR}
  find ${TOP_DIR} -type d -exec chmod a+w {} \;
  chown gpadmin:gpadmin ${CWDIR}/build_plcontainer.sh
  su gpadmin -c "OUTPUT=${OUTPUT} \
                 DEV_RELEASE=${DEV_RELEASE} \
                 bash ${CWDIR}/build_plcontainer.sh $(pwd)"
}

_main "$@"
