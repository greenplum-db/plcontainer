#!/bin/bash -l

set -eox pipefail

ccp_src/aws/setup_ssh_to_cluster.sh
plcontainer_src/concourse/scripts/docker_install.sh
plcontainer_src/concourse/scripts/docker_build_image.sh

