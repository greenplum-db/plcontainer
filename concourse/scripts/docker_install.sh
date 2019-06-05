#!/bin/bash

#------------------------------------------------------------------------------
#
# Copyright (c) 2017-Present Pivotal Software, Inc
#
#------------------------------------------------------------------------------

set -exo pipefail

install_docker() {
    local node=$1
    case "$platform" in
      centos6)
        ssh centos@$node "sudo bash -c \" \
           yum -y install http://dl.fedoraproject.org/pub/epel/epel-release-latest-6.noarch.rpm; \
           yum -y install docker-io; \
           service docker start; \
           groupadd docker; \
           chown root:docker /var/run/docker.sock; \
           usermod -a -G docker gpadmin; \
           service docker stop; \
           sleep 5; \
           umount /var/lib/docker/devicemapper; \
           set -exo pipefail; \
           mv /var/lib/docker /data/gpdata/docker; \
           ln -s /data/gpdata/docker /var/lib/docker; \
           service docker start; \
	\""
        ;;
      centos7)
        ssh centos@$node "sudo bash -c \" \
           yum install -y yum-utils device-mapper-persistent-data lvm2; \
           yum-config-manager --add-repo https://download.docker.com/linux/centos/docker-ce.repo; \
           yum makecache fast; \
           yum install -y docker-ce; \
           yum install -y cpan; \
           yum install -y perl-Module-CoreList; \
           systemctl start docker; \
           groupadd docker; \
           chown root:docker /var/run/docker.sock; \
           usermod -a -G docker gpadmin; \
           service docker stop; \
           sleep 5; \
           set -exo pipefail; \
           mv /var/lib/docker /data/gpdata/docker; \
           ln -s /data/gpdata/docker /var/lib/docker; \
           service docker start; \
        \""
        ;;
      ubuntu18)
        ssh ubuntu@$node "sudo bash -c \" \
           sudo apt-get update; \
	   sudo apt-get -y install wget apt-transport-https ca-certificates curl gnupg-agent software-properties-common; \
           wget https://download.docker.com/linux/ubuntu/gpg -O /tmp/docker.key ; \ 
	   sudo apt-key add /tmp/docker.key ; \
           sudo add-apt-repository -y 'deb [arch=amd64] https://download.docker.com/linux/ubuntu bionic stable' ; \
           sudo apt-get update; \
	   sudo apt-get -y install docker-ce docker-ce-cli containerd.io; \
           sudo systemctl start docker; \
           sudo groupadd docker; \
           sudo chown root:docker /var/run/docker.sock; \
           usermod -a -G docker gpadmin; \
           sudo service docker stop; \
           sleep 5; \
           set -exo pipefail; \
           sudo mv /var/lib/docker /data/gpdata/docker; \
           sudo ln -s /data/gpdata/docker /var/lib/docker; \
           sudo service docker start; \
        \""
        ;;
    esac 
}

install_docker mdw
install_docker sdw1

