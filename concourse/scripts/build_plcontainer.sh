#!/bin/bash

#------------------------------------------------------------------------------
#
# Copyright (c) 2017-Present Pivotal Software, Inc
#
#------------------------------------------------------------------------------

set -exo pipefail

CWDIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
TOP_DIR=${CWDIR}/../../../

build_plcontainer() {
  # source greenplum
  source /usr/local/greenplum-db/greenplum_path.sh
  source ${TOP_DIR}/gpdb_src/gpAux/gpdemo/gpdemo-env.sh

  source /opt/gcc_env.sh

  # build plcontainer
  pushd plcontainer_src
  if [ "${DEV_RELEASE}" == "release" ]; then
      if git describe --tags >/dev/null 2>&1 ; then
          echo "git describe failed" || exit 1
      fi
      PLCONTAINER_VERSION=$(git describe --tags | awk -F. '{printf("%d.%d", $1, $2)}')
      PLCONTAINER_RELEASE=$(git describe --tags | awk -F. '{print $3}')
  else
      PLCONTAINER_VERSION="0.0"
      PLCONTAINER_RELEASE="0"
  fi

  # copy clients into folder
  pushd ../plcontainer_client
  tar zxf plcontainer_client.tar.gz
  popd
  tar zxvf ../plcontainer_client/pyclient.tar.gz -C src/pyclient/bin/
  tar zxvf ../plcontainer_client/rclient.tar.gz -C src/rclient/bin/

  pushd package
  PLCONTAINER_VERSION=${PLCONTAINER_VERSION} PLCONTAINER_RELEASE=${PLCONTAINER_RELEASE} make cleanall;
  PLCONTAINER_VERSION=${PLCONTAINER_VERSION} PLCONTAINER_RELEASE=${PLCONTAINER_RELEASE} make;
  popd
  popd
  
  if [ "${DEV_RELEASE}" == "devel" ]; then
      cp plcontainer_src/package/plcontainer-*.gppkg $OUTPUT/plcontainer-concourse.gppkg
  else
      cp plcontainer_src/package/plcontainer-*.gppkg $OUTPUT/
  fi
}

build_plcontainer
