#!/bin/bash -l

set -exo pipefail

function _main() {
    # Run testing
    export PL_TESTDB=contrib_regression
    pushd plcontainer_artifacts
    time plcontainer image-add -f plcontainer_r_shared.tar.gz
    # TODO for now drop logging for test maybe bring it back in the future
    time plcontainer runtime-add -r plc_r_shared -i "${CONTAINER_NAME_SUFFIX_R}" -l r
    time cmake --build . --target testr 
    # Test gppkg uninstall
    gppkg -q --all
    # Find the package name
    local pkg_name
    pkg_name=$(gppkg -q --all | awk -F"[-]+" '/plcontainer/{print $1}')
    # Uninstall it
    gppkg -r "${pkg_name}"
    # Install again
    gppkg -i plcontainer*.gppkg
    popd
}

_main 
