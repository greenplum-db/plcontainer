#!/bin/bash

set -exo pipefail

export GOROOT=/usr/local/go
export GOPATH=$HOME/go
export PATH=$PATH:/usr/local/go/bin

function _main() {
    pushd /home/gpadmin/plcontainer_src/k8s

    make manifests
    make generate

    # copy clientdir

    make docker-build IMG=gcr.io/todo/todo
    docker save gcr.io/todo/todo gzizp > x.x.tar.gz

    popd
}

_main
