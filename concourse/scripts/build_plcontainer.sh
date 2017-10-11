#!/bin/bash

set -exo pipefail

CWDIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
TOP_DIR=${CWDIR}/../../../

build_plcontainer() {
  source /usr/local/greenplum-db/greenplum_path.sh
  
  source ${TOP_DIR}/gpdb_src/gpAux/gpdemo/gpdemo-env.sh
  
  #install plr
  pushd plr
  gppkg -i plr*.gppkg
  popd
  source /usr/local/greenplum-db/greenplum_path.sh
 
  # build plcontainer
  pushd plcontainer_src
  if [ "${DEV_RELEASE}" == "release" ]; then
      if git describe >/dev/null 2>&1 ; then
          echo "git describe failed" || exit 1
      fi
      PLCONTAINER_VERSION=$(git describe | awk -F. '{printf("%d.%d", $1, $2)}')
      PLCONTAINER_RELEASE=$(git describe | awk -F. '{print $3}')
  else
      PLCONTAINER_VERSION="0.0"
      PLCONTAINER_RELEASE="0"
  fi
  PLCONTAINER_VERSION=${PLCONTAINER_VERSION} PLCONTAINER_RELEASE=${PLCONTAINER_RELEASE} make clean
  pushd package
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
