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
  if [ "$MODE" == "test" ]; then
      export CIBUILD=1
  fi
  
  # build plcontainer
  pushd plcontainer_src
  make clean
  pushd package
  make cleanall && make
  popd
  popd
  
  if [ "$MODE" == "test" ]; then
      cp plcontainer_src/package/plcontainer-*.gppkg $OUTPUT/plcontainer-concourse.gppkg
  else
      cp plcontainer_src/package/plcontainer-*.gppkg $OUTPUT/
  fi
}  

build_plcontainer
