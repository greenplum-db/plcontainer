#!/bin/bash -l

#------------------------------------------------------------------------------
#
# Copyright (c) 2017-Present Pivotal Software, Inc
#
#------------------------------------------------------------------------------

set -eox pipefail

ccp_src/aws/setup_ssh_to_cluster.sh
plcontainer_src/concourse/scripts/docker_install.sh
plcontainer_src/concourse/scripts/run_plcontainer_tests.sh

