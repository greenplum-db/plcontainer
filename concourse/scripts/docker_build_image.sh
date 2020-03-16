#!/bin/bash

#------------------------------------------------------------------------------
#
# Copyright (c) 2017-Present Pivotal Software, Inc
#
#------------------------------------------------------------------------------

set -exo pipefail

pushd plcontainer_src
if [ "$DEV_RELEASE" == "devel" ]; then
	IMAGE_NAME="plcontainer-$language-images-devel.tar.gz"
else
	PLCONTAINER_VERSION=$(git describe --tags)
	IMAGE_NAME="plcontainer-$language-images-${PLCONTAINER_VERSION}.tar.gz"
fi
popd

docker_build() {
    local node=$1
    ssh $node "bash -c \" mkdir -p ~/artifacts_$language\" "

    scp -r plcontainer_src $node:~/
    scp -r data-science-bundle $node:~/
    if [[ $language = "python" ]]; then
        DockerFolder="~/data-science-bundle/plcontainer_dockerfiles/python/"
        echo "language python in pipeline."
    elif [[ $language = "python3" ]]; then
        DockerFolder="~/data-science-bundle/plcontainer_dockerfiles/python/"
        echo "language python3 in pipeline."
    elif [[ $language = "r" ]]; then
        DockerFolder="~/data-science-bundle/plcontainer_dockerfiles/r/"
        echo "language R in pipeline."
    else
        echo "Wrong language in pipeline." || exit 1
    fi

    ssh $node "bash -c \" \
    set -eox pipefail; \
    pushd $DockerFolder; \
    mv ../../concourse/scripts/rlibs ./ ;\
    mv ../../concourse/scripts/rlibs.higher_gcc ./ ;\
    chmod +x *.sh; \
    ls -lh; \
    docker build -f Dockerfile.$language -t pivotaldata/plcontainer_${language}_shared:devel ./ ; \
    popd; \
    docker save pivotaldata/plcontainer_${language}_shared:devel | gzip -c > ~/${IMAGE_NAME}; \
    \""
}

docker_build mdw
scp mdw:~/plcontainer-*.tar.gz plcontainer_docker_image/
