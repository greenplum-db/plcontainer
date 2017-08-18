#!/bin/bash

set -ex

docker_build() {
    local node=$1
    scp -r plcontainer_client_docker_image $node:~/
    scp -r plcontainer_gpdb_build $node:~/
    scp -r plcontainer_src $node:~/

    ssh $node "bash -c \" \
	docker load -i plcontainer_client_docker_image/plcontainer-devel-images.tar.gz; \
        mkdir -p /tmp/pyclient/bin; \
        source /usr/local/greenplum-db-devel/greenplum_path.sh; \
        pushd plcontainer_src/src/pyclient; \
        make; \
        cp bin/* /tmp/pyclient/bin/; \
        popd; \
        \""
}

docker_build mdw
docker_build sdw1
