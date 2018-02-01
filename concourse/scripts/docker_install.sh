#!/bin/bash

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
    esac 
}

install_docker mdw
install_docker sdw1

