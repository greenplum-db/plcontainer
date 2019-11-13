#!/bin/bash -l

#------------------------------------------------------------------------------
#
# Copyright (c) 2017-Present Pivotal Software, Inc
#
#------------------------------------------------------------------------------

set -eox pipefail

ccp_src/scripts/setup_ssh_to_cluster.sh
plcontainer_src/concourse/scripts/docker_install.sh
scp -r plcontainer_gpdb_ubuntu18_build_lib/* mdw:/tmp/
scp -r plcontainer_gpdb_ubuntu18_build_lib/* sdw1:/tmp/
plcontainer_src/concourse/scripts/protobuf_install.sh
plcontainer_src/concourse/scripts/run_plcontainer_tests.sh

