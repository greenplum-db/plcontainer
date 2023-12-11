#!/bin/bash

set -exo pipefail

function _main() {
    pushd /home/gpadmin/plcontainer_src/k8s

    make manifests
    make generate

    # copy clientdir

    make docker-build IMG=gcr.io/TODO/TODO
    docker save gcr.io/TODO/TODO gzizp > x.x.tar.gz

    popd
}

_main
