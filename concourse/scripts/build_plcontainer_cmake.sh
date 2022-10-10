#!/bin/bash -l
# TODO when PR merge replace it with build_plcontainer.sh
set -exo pipefail

function build_all() {
    [ -f /opt/gcc_env.sh ] && source /opt/gcc_env.sh
    pushd /home/gpadmin/plcontainer_src
    popd

    pushd /home/gpadmin/plcontainer_artifacts

    cmake /home/gpadmin/plcontainer_src -DCONTAINER_NAME_SUFFIX_PYTHON="${CONTAINER_NAME_SUFFIX_PYTHON}" -DCONTAINER_NAME_SUFFIX_R="${CONTAINER_NAME_SUFFIX_R}"
    cmake --build .
    # for make install and build python3 python2 and r clients
    cmake --build . --target clients
    # build gppkg and gppkg_artifact
    cmake --build . --target gppkg_artifact
    # build image artifact
    cmake --build . --target images_artifact
    popd
}

function _main() {
    time build_all
}

_main
