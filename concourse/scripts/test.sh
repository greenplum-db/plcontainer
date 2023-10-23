#!/bin/bash -l

set -exo pipefail

SCRIPT_DIR=$( cd -- "$( dirname -- "${BASH_SOURCE[0]}" )" &> /dev/null && pwd )

function start_docker_server() {
    source "$SCRIPT_DIR/docker-lib.sh"
    ln -s /usr/libexec/docker/docker-runc-current /usr/bin/docker-runc
    start_docker
}

function _install_gppkg() {
    if [[ ${GP_MAJOR_VERSION} == "7" ]]; then
        "/home/gpadmin/bin_gppkg_v2/gppkg" install -a ./*.gppkg
    else
	gppkg --install ./*.gppkg
    fi
}

function _uninstall_gppkg() {
    if [[ ${GP_MAJOR_VERSION} == "7" ]]; then
        "/home/gpadmin/bin_gppkg_v2/gppkg" remove -a plcontainer
    else
	gppkg --remove plcontainer
    fi
}

function _main() {
    # Run testing
    pushd plcontainer_artifacts
    # Install gppkg
    _install_gppkg
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
    _uninstall_gppkg
    _install_gppkg
    popd
}

# print the test diff to stdout in our CI
export SHOW_REGRESS_DIFF=1
start_docker_server
_main
