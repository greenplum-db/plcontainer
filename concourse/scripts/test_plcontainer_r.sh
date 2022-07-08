#!/bin/bash -l

set -exo pipefail

function _main() {
    # Run testing
    export PL_TESTDB=contrib_regression
    pushd plcontainer_artifacts
    time cmake --build . --target install
    time plcontainer image-add -f plcontainer_r_shared.tar.gz
    # TODO for now drop logging for test maybe bring it back in the future
    time plcontainer runtime-add -r plc_r_shared -i r.alpine -l r
    time cmake --build . --target testr
    popd
}

_main
