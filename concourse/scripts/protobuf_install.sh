#!/bin/bash

set -eox pipefail

install_grpc() {
  local node=$1
  case "$platform" in
    ubuntu18)
      ssh ubuntu@$node "sudo bash -c \" \
		chmod 777 /tmp/plcontainer-concourse-ubuntu18-lib.tar.gz; \
		\""
      ssh $node "bash -c \" \
        tar zxvf /tmp/plcontainer-concourse-ubuntu18-lib.tar.gz -C /usr/local/greenplum-db-devel/lib/; \
        \""
	  ;;
  esac
}

install_grpc mdw
install_grpc sdw1
