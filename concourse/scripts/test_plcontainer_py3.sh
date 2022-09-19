#!/bin/bash -l

set -exo pipefail

function _main() {
    # Run testing
    pushd plcontainer_artifacts
    # FIXME tricky to solve problem for now.
    # \! psql -d ${PL_TESTDB} -c "select rlogging_fatal();"
    export PL_TESTDB=contrib_regression
    gppkg -i plcontainer*.gppkg
    # image add for both python and r
    time plcontainer image-add -f plcontainer-python-image-*-gp6.tar.gz
    time plcontainer image-add -f plcontainer-r-image-*-gp6.tar.gz

    time cmake --build . --target prepare_runtime
    time cmake --build . --target installcheck
    if [[ ${CONTAINER_NAME_SUFFIX_PYTHON} == *_b ]]; then
        time gpstop -ar
        time cmake --build . --target testpy3_bundle 
    fi
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
