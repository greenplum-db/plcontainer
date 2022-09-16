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
    cmake --build . --target pyclient
    # for make install
    cmake --build . --target rclient
    # build gppkg
    cmake --build . --target gppkg
    # build image artifact
    cmake --build . --target pyclient_image_artifact
    cmake --build . --target rclient_image_artifact
    popd
}

function _main() {
    time build_all
}

_main
