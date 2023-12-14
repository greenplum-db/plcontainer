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

    popd

    mkdir -p plcontainer_artifacts
    mv /home/gpadmin/plcontainer_src/k8s/x.x.tar.gz plcontainer_artifacts/bin_plcontainer_k8s_controller_intermediates.tar.gz
    mv /home/gpadmin/plcontainer_src/k8s/plcontainer_on_k8s.yaml plcontainer_artifacts/bin_plcontainer_k8s_yaml_intermediates.yaml
}

_main
