#!/bin/bash

set -exo pipefail

DockerFolder="plcontainer_src/dockerfiles/python/centos7/ds/"
docker_build() {
	local node=$1
	scp -r datascience-python/Data*.gppkg $node:~/
	scp -r python/python*.targz $node:~/
	scp -r plcontainer_src $node:~/
	scp -r openssl/openssl*.gz $node:~/

	ssh $node "bash -c \" \
	cp ~/openssl*.gz $DockerFolder; \
	cp ~/python*.targz $DockerFolder; \
	cp ~/Data*.gppkg $DockerFolder; \
	pushd $DockerFolder; \
	chmod +x install_python.sh
	tar -zxvf Data*.gppkg
	docker build -f Dockerfile.python.ds.centos7 -t pivotaldata/plcontainer_python_shared:devel ./ ; \
	popd; \
	docker save pivotaldata/plcontainer_python_shared:devel | gzip -c > ~/plcontainer-devel-images.tar.gz; \
	\""
}

docker_build mdw
scp mdw:~/plcontainer-devel-images.tar.gz plcontainer_docker_image/plcontainer-devel-images.tar.gz
