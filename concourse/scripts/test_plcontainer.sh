#!/bin/bash -l

set -exo pipefail

function _main() {
    # Run testing
    pushd plcontainer_artifacts
    # Install gppkg
    gppkg -i plcontainer*.gppkg
    # Image add for both python and r
    # Python3
    time plcontainer image-add -f plcontainer-python-image-*.tar.gz
    # Python2
    time plcontainer image-add -f plcontainer-python2-image-*.tar.gz
    # R
    time plcontainer image-add -f plcontainer-r-image-*.tar.gz

    time cmake --build . --target prepare_runtime

    docker images
    docker ps -a
    plcontainer runtime-show

    time cmake --build . --target installcheck
    # Test gppkg uninstall
    gppkg -q --all
    # Find the package name
    local pkg_name
    pkg_name=$(gppkg -q --all | awk -F"[-]+" '/plcontainer/{print $1}')
    # Uninstall gppkg test if it can uninstall or not
    gppkg -r "${pkg_name}"
    # Install gppkg again
    gppkg -i plcontainer*.gppkg
    popd
}

_main
