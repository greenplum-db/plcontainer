#!/usr/bin/env bash

set -exo

SCRIPT_DIR=$( cd -- "$( dirname -- "${BASH_SOURCE[0]}" )" &> /dev/null && pwd )

source "$CI_REPO_DIR/common/entry_common.sh"

function install_extra_build_dependencies() {
    case "$OS_NAME" in
    rhel*)
        yum install -y yum-utils
        # because using `yum install docker` did not have the command --data-root
        # if we do not choose `--data-root` we maybe increase the size
        # and then cause a `can not create volume` problem
        # so we add the docker source and install the latest one
        yum-config-manager --add-repo https://download.docker.com/linux/centos/docker-ce.repo
        yum install -y docker-ce docker-ce-cli
        ;;
    ubuntu*)
        # ubuntu also need to install latest docker
        apt update
        apt install -y apt-transport-https ca-certificates curl software-properties-common
        curl -fsSL https://download.docker.com/linux/ubuntu/gpg | apt-key add -
        add-apt-repository "deb [arch=amd64] https://download.docker.com/linux/ubuntu bionic stable"
        apt install -y docker-ce
        ;;

    *) ;;
    esac
}

function start_docker_server() {
    source "$SCRIPT_DIR/docker-lib.sh"
    ln -s /usr/libexec/docker/docker-runc-current /usr/bin/docker-runc
    start_docker
}

install_cmake
install_extra_build_dependencies
start_docker_server

# gpadmin docker permission
usermod -aG docker gpadmin
# we need to this permission without reboot
chown gpadmin /var/run/docker.sock
