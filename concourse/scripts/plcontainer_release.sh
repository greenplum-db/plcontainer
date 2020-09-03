#!/bin/bash

#------------------------------------------------------------------------------
#
# Copyright (c) 2017-Present Pivotal Software, Inc
#
#------------------------------------------------------------------------------

set -exo pipefail

CWDIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
TOP_DIR=${CWDIR}/../../../

release_plcontainer() {

  # build plcontainer
  pushd plcontainer_src
  if git describe --tags >/dev/null 2>&1 ; then
      echo "git describe failed" || exit 1
  fi
  PLCONTAINER_VERSION=$(git describe --tags | awk -F. '{printf("%d.%d", $1, $2)}')
  PLCONTAINER_RELEASE=$(git describe --tags | awk -F. '{print $3}')
  popd

  # release ubuntu 18
  mkdir -p release_bin_ubuntu18 
  cp plcontainer_gpdb_ubuntu18_build/plcontainer-*.gppkg release_bin_ubuntu18/plcontainer-${PLCONTAINER_VERSION}.${PLCONTAINER_RELEASE}-gp6-ubuntu18.04_x86_64.gppkg
  # release centos 7
  mkdir -p release_bin_centos7
  cp plcontainer_gpdb_centos7_build/plcontainer-*.gppkg release_bin_centos7/plcontainer-${PLCONTAINER_VERSION}.${PLCONTAINER_RELEASE}-gp6-rhel7_x86_64.gppkg
  # release r image
  mkdir -p release_image_r
  cp plcontainer_docker_image_build_r/plcontainer*.tar.gz release_image_r/plcontainer-r-image-${PLCONTAINER_VERSION}.${PLCONTAINER_RELEASE}-gp6.tar.gz
  # release plcontainer ubuntu lib
  mkdir -p release_lib
  cp plcontainer_gpdb_ubuntu18_build_lib/plcontainer*.tar.gz release_lib/plcontainer-lib-${PLCONTAINER_VERSION}.${PLCONTAINER_RELEASE}-gp6-ubuntu18.04_x86_64.tar.gz
  cp plcontainer_gpdb_ubuntu18_build_lib/pl4k*.tar.gz release_lib/pl4k-${PLCONTAINER_VERSION}.${PLCONTAINER_RELEASE}.tar.gz
}

release_plcontainer
