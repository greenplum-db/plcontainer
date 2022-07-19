#!/bin/bash -l

set -exo pipefail

function _main() {
    # Run testing
    pushd plcontainer_artifacts
    # FIXME tricky to solve problem for now.
    # \! psql -d ${PL_TESTDB} -c "select rlogging_fatal();"
    export PL_TESTDB=contrib_regression
    export IMAGE_ENV="$1"
    local test_target=testpy39
    if [ "${IMAGE_ENV}" == "ubuntu" ]; then
        test_target=testpy39_dsb
    fi
    gppkg -i plcontainer*.gppkg
    time plcontainer image-add -f plcontainer_python3_shared.tar.gz
    # TODO for now drop logging for test maybe bring it back in the future
    time plcontainer runtime-add -r plc_python_shared -i python39."${IMAGE_ENV}" -l python3
    time plcontainer runtime-add -r plc_python_shared_oom -i python39."${IMAGE_ENV}" -l python3 -s memory_mb=100
    time cmake --build . --target ${test_target}
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

_main "$@"
