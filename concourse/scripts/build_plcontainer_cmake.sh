#!/bin/bash -l
# TODO when PR merge replace it with build_plcontainer.sh
set -exo pipefail

function build_rclient() {
    [ -f /opt/gcc_env.sh ] && source /opt/gcc_env.sh
    pushd /home/gpadmin/plcontainer_src
    popd 

    pushd /home/gpadmin/plcontainer_artifacts

    cmake /home/gpadmin/plcontainer_src
    cmake --build .
    cmake --build . --target pyclient
    popd
}

function _main() {
    time build_rclient
}

_main "$@"
