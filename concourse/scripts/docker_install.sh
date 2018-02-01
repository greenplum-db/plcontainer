#!/bin/bash

set -exo pipefail

install_docker() {
    local node=$1
    case "$platform" in
      centos6)
        ssh centos@$node "sudo bash -c \"sudo rpm -iUvh http://dl.fedoraproject.org/pub/epel/6/x86_64/epel-release-6-8.noarch.rpm; \
           sudo yum -y install docker-io; \
           sudo service docker start; \
           sudo groupadd docker; \
           sudo chown root:docker /var/run/docker.sock; \
           sudo usermod -a -G docker gpadmin; \
           sudo service docker stop; \
           sleep 5; \
           sudo umount /var/lib/docker/devicemapper; \
           sudo mv /var/lib/docker /data/gpdata/docker; \
           sudo ln -s /data/gpdata/docker /var/lib/docker; \
           sudo service docker start; \
	\""
        ;;
      centos7)
        ssh centos@$node "sudo bash -c \" \
           sudo yum install -y yum-utils device-mapper-persistent-data lvm2; \
           sudo yum-config-manager --add-repo https://download.docker.com/linux/centos/docker-ce.repo; \
           sudo yum makecache fast; \
           sudo yum install -y docker-ce; \
           sudo yum install -y cpan; \
           sudo yum install -y perl-Module-CoreList; \
           sudo systemctl start docker; \
           sudo groupadd docker; \
           sudo chown root:docker /var/run/docker.sock; \
           sudo usermod -a -G docker gpadmin; \
           sudo service docker stop; \
           sleep 5; \
           sudo mv /var/lib/docker /data/gpdata/docker; \
           sudo ln -s /data/gpdata/docker /var/lib/docker; \
           sudo service docker start; \
        \""
        ;;
    esac 
}

install_docker mdw
install_docker sdw1

