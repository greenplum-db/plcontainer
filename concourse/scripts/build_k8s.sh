#!/bin/bash

set -exo pipefail

export GOROOT=/usr/local/go
export GOPATH=$HOME/go
export PATH=$PATH:/usr/local/go/bin

function _main() {
    pushd /home/gpadmin/plcontainer_src/k8s

    make test

    # copy clientdir

    make docker-build IMG=gcr.io/todo/todo

    make plcontainer_on_k8s.yaml IMG=gcr.io/todo/todo
    docker save gcr.io/todo/todo | gzip > x.x.tar.gz

    mv plcontainer_on_k8s.yaml /tmp
    mv x.x.tar.gz /tmp

    popd
}

_main
