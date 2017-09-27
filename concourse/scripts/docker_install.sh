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
        \""
        ;;
    esac 
    touch install_docker.done.$node
}

rm -f install_docker.done.mdw install_docker.done.sdw1
install_docker mdw &
install_docker sdw1 &

# monitor install progress.
times=0
while true; do
    if [ -f install_docker.done.mdw -a -f install_docker.done.sdw1 ]; then
        echo "Install docker finished."
        break
    fi
    sleep 5
    ((times++))
    if [ $times -gt 720 ]; then
        echo "Install docker timeout. Exiting."
        exit 1
    fi
done
