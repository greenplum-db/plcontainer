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
  [ -f 'opt/gcc_env.sh' ] && source /opt/gcc_env.sh
  source /usr/local/greenplum-db/greenplum_path.sh
  source ${TOP_DIR}/gpdb_src/gpAux/gpdemo/gpdemo-env.sh
  
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
  git describe > VERSION
  fi

  # copy servers into folder
  pushd ../plcontainer_server
  tar zxf plcontainer_server.tar.gz
  popd
  #tar zxvf ../plcontainer_server/pyserver.tar.gz -C src/pyserver/bin/
  tar zxvf ../plcontainer_server/rserver.tar.gz -C src/rclient/bin/

  # copy grpc libs
  mkdir ${TOP_DIR}/grpc_lib
  pushd ${TOP_DIR}/grpc_lib 
  cp -d /usr/local/lib/libgrpc++.so* . 
  cp -d /usr/local/lib/libgrpc++_reflection.so* .
  cp -d /usr/local/lib/libgrpc.so* .
  cp -d /usr/local/lib/libgpr.so* .
  cp -d /usr/local/lib/libprotobuf.so* .
  cp -d /usr/lib/x86_64-linux-gnu/libjson-c.so* .
  tar czf plcontainer-libs.tar.gz *
  popd
  
  export LD_LIBRARY_PATH=/usr/local/lib/:$LD_LIBRARY_PATH
  make docker-dep
  make proto

  pushd package
  cp Makefile.ubuntu Makefile


  PLCONTAINER_VERSION=${PLCONTAINER_VERSION} PLCONTAINER_RELEASE=${PLCONTAINER_RELEASE} make cleanall;
  PLCONTAINER_VERSION=${PLCONTAINER_VERSION} PLCONTAINER_RELEASE=${PLCONTAINER_RELEASE} make
  popd
  popd
  
  if [ "${DEV_RELEASE}" == "devel" ]; then
      cp plcontainer_src/package/plcontainer-*.gppkg $OUTPUT/plcontainer-concourse.gppkg
      cp grpc_lib/plcontainer-libs.tar.gz $OUTPUT/plcontainer-concourse-lib.tar.gz
  else
      cp plcontainer_src/package/plcontainer-*.gppkg $OUTPUT/
      cp grpc_lib/plcontainer-libs.tar.gz $OUTPUT/plcontainer-concourse-lib.tar.gz
  fi
}

create_pl4k_package() {
  mkdir pl4k
  cp plcontainer_src/VERSION pl4k
  cp -r plcontainer_src/package/UBUNTU/bin pl4k
  cp -r plcontainer_src/package/UBUNTU/lib pl4k
  cp -r plcontainer_src/package/UBUNTU/share pl4k
  cp -d /usr/local/lib/libgrpc++.so* pl4k/lib 
  cp -d /usr/local/lib/libgrpc++_reflection.so* pl4k/lib
  cp -d /usr/local/lib/libgrpc.so* pl4k/lib
  cp -d /usr/local/lib/libgpr.so* pl4k/lib
  cp -d /usr/local/lib/libprotobuf.so* pl4k/lib
  cp -d /usr/lib/x86_64-linux-gnu/libjson-c.so* pl4k/lib
  pushd pl4k
  tar czf pl4k.tar.gz *
  popd
  cp pl4k/pl4k.tar.gz $OUTPUT/pl4k-$PLCONTAINER_VERSION.tar.gz
}
build_plcontainer
create_pl4k_package
