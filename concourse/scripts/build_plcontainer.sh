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
  # build plcontainer
  pushd plcontainer_src
  if [ "${DEV_RELEASE}" == "release" ]; then
      if git describe --tags >/dev/null 2>&1 ; then
          echo "git describe failed" || exit 1
      fi
      PLCONTAINER_VERSION=$(git describe --tags | awk -F. '{printf("%d.%d", $1, $2)}')
      PLCONTAINER_RELEASE=$(git describe --tags | awk -F. '{print $3}')
  else
      PLCONTAINER_VERSION="1.0"
      PLCONTAINER_RELEASE="0"
  fi
  PLCONTAINER_VERSION=${PLCONTAINER_VERSION} PLCONTAINER_RELEASE=${PLCONTAINER_RELEASE} make clean
  pushd src/pyclient
  make cleanall
  make all
  make clean
  unset PYTHONPATH
  unset PYTHONHOME
  make PYTHON_VERSION=3 all
  make clean
  popd
  pushd package
  # only source greenplum path for gppkg
  source ${TOP_DIR}/gpdb_src/gpAux/gpdemo/gpdemo-env.sh
  
  source /usr/local/greenplum-db/greenplum_path.sh

  export R_HOME=/usr/lib64/R
  PLCONTAINER_VERSION=${PLCONTAINER_VERSION} PLCONTAINER_RELEASE=${PLCONTAINER_RELEASE} make cleanall;
  PLCONTAINER_VERSION=${PLCONTAINER_VERSION} PLCONTAINER_RELEASE=${PLCONTAINER_RELEASE} make
  popd
  popd
  
  if [ "${DEV_RELEASE}" == "devel" ]; then
      cp plcontainer_src/package/plcontainer-*.gppkg $OUTPUT/plcontainer-concourse.gppkg
  else
      cp plcontainer_src/package/plcontainer-*.gppkg $OUTPUT/
  fi
}  

build_plcontainer
