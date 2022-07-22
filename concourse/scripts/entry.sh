#!/bin/bash

# Entry point for GPDB source & cluster related tasks.
# This script setup basic build/test environment including:
# - Create a gpadmin user
# - Copy all files from /tmp/build/xxx (concourse WORKDIR) to /home/gpadmin/ and chown to gpadmin
# - Some dependencies doesn't exist in build/test image.
# - Special setup for individual task which needs root permission.
# - At the end, call the task script with gpadmin permission.
#
# Simple rules:
# 1. Any root operations should happen in this script.
# 2. Task script only requires gpadmin permission.
# 3. Since everything has been copied to the /home/gpadmin directory, use absolute path as much as
#    as possible in the task script, it will reduce the confusion when we fly into the concourse
#    container.
# 4. Bash functions should be idempotent as much as possible to make fly hijack debugging easier.

set -eox

if [[ ! ${PWD} =~ /tmp/build/[0-9a-z]* ]]; then
    echo "This script should always be started from concourse WORKDIR."
fi

# Internal utilty functions
_determine_os() {
    local name version
    if [ -f /etc/redhat-release ]; then
        name="rhel"
        version=$(sed </etc/redhat-release 's/.*release *//' | cut -f1 -d.)
    elif [ -f /etc/SuSE-release ]; then
        name="sles"
        version=$(awk -F " *= *" '$1 == "VERSION" { print $2 }' /etc/SuSE-release)
    elif grep -q photon /etc/os-release; then
        name="photon"
        version=$(awk -F " *= *" '$1 == "VERSION_ID" { print $2 }' /etc/os-release | cut -f1 -d.)
    elif grep -q ubuntu /etc/os-release; then
        name="ubuntu"
        version=$(awk -F " *= *" '$1 == "VERSION_ID" { print $2 }' /etc/os-release | tr -d \")
    else
        echo "Could not determine operating system type" >/dev/stderr
        exit 1
    fi
    echo "${name}${version}"
}

OS_NAME=$(_determine_os)
CMAKE_HOME="/opt/cmake_new"

# Global ENV defines
# /tmp/build/xxxxx. it should not be used in normal conditions. Use /home/gpadmin instead.
# Everything has been linked there.
export CONCOURSE_WORK_DIR=${PWD}

# Create gpadmin user and chown all files in the PWD. All files will be linked to /home/gpadmin.
# All of our work should be started from there.
setup_gpadmin() {
    # If the gpadmin exist, quit
    if grep -c '^gpadmin:' /etc/passwd; then
        return
    fi

    # If the image has sshd, then we call gpdb's setup_gpadmin_user.sh to create the gpadmin user
    # and setup the ssh.
    # Otherwise, create the gpadmin user only.
    if [ -f /etc/ssh/sshd_config ]; then
        pushd "${CONCOURSE_WORK_DIR}"
        local gpdb_concourse_dir="${CONCOURSE_WORK_DIR}/gpdb_src/concourse/scripts"
        "${gpdb_concourse_dir}/setup_gpadmin_user.bash"
        popd
    else
        # Below is copied from setup_gpadmin_user.bash
        groupadd supergroup
        case "$OS_NAME" in
        rhel*)
            /usr/sbin/useradd -G supergroup,tty gpadmin
            ;;
        ubuntu*)
            /usr/sbin/useradd -G supergroup,tty gpadmin -s /bin/bash
            ;;
        sles*)
            # create a default group gpadmin, and add user gpadmin to group gapdmin, supergroup,
            # tty
            /usr/sbin/useradd -U -G supergroup,tty gpadmin
            ;;
        photon*)
            /usr/sbin/useradd -U -G supergroup,tty,root gpadmin
            ;;
        *)
            echo "Unknown OS: $OS_NAME"
            exit 1
            ;;
        esac
        echo -e "password\npassword" | passwd gpadmin
    fi
    mkdir -p /home/gpadmin
    chown gpadmin:gpadmin /home/gpadmin

    chown -R gpadmin:gpadmin /tmp/build
    ln -s "${CONCOURSE_WORK_DIR}"/* /home/gpadmin
}
## Currently, isolation2 testing framework relies on pg_isolation2_regress, we
## should build it from source. However, in concourse, the gpdb_bin is fetched
## from remote machine, the $(abs_top_srcdir) variable points to a non-existing
## location, we fixes this issue by creating a symbolic link for it.
function create_fake_gpdb_src() {
    local fake_gpdb_src
    fake_gpdb_src="$(
        grep -rhw '/usr/local/greenplum-db-devel' -e 'abs_top_srcdir = .*' |
            head -n 1 | awk '{ print $NF; }'
    )"

    if [ -d "${fake_gpdb_src}" ]; then
        echo "Fake gpdb source directory has been configured."
        return
    fi

    pushd /home/gpadmin/gpdb_src
    ./configure --prefix=/usr/local/greenplum-db-devel \
        --without-zstd \
        --disable-orca --disable-gpcloud --enable-debug-extensions
    popd

    local fake_root
    fake_root=$(dirname "${fake_gpdb_src}")
    mkdir -p "${fake_root}"
    ln -s /home/gpadmin/gpdb_src "${fake_gpdb_src}"
}

# Extract gpdb binary
function install_gpdb() {
    [ ! -d /usr/local/greenplum-db-devel ] && mkdir -p /usr/local/greenplum-db-devel
    tar -xzf "${CONCOURSE_WORK_DIR}"/bin_gpdb/*.tar.gz -C /usr/local/greenplum-db-devel
    chown -R gpadmin:gpadmin /usr/local/greenplum-db-devel
    # Start cluster
    source "/home/gpadmin/gpdb_src/concourse/scripts/common.bash"
    make_cluster
    source /home/gpadmin/gpdb_src/gpAux/gpdemo/gpdemo-env.sh
}

# Dependency installers
# Ideally all dependencies should exist in the docker image. Use this script to install them only
# if it is more difficult to change it in the image side.
# Download the dependencies with concourse resources as much as possible, then we could benifit from
# concourse's resource cache system.
install_cmake() {
    # cmake_new to avoid name collision with the docker image.
    if [ -e "${CMAKE_HOME}" ]; then
        echo "cmake might have been installed in ${CMAKE_HOME}"
        return
    fi
    echo "Installing cmake to ${CMAKE_HOME}..."
    pushd bin_cmake
    mkdir -p "${CMAKE_HOME}"
    sh cmake-*-linux-x86_64.sh --skip-license --prefix="${CMAKE_HOME}"
    popd
}

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

function setup_gpadmin_bashrc() {
    {   
        # ubuntu need to source PATH
        echo "source /usr/local/greenplum-db-devel/greenplum_path.sh"
        echo "source /home/gpadmin/gpdb_src/gpAux/gpdemo/gpdemo-env.sh"
        echo "export OS_NAME=${OS_NAME}"
        echo "export PATH=${CMAKE_HOME}/bin:\$PATH"
    } >>/home/gpadmin/.bashrc
}

function start_docker_server() {
    source /home/gpadmin/plcontainer_src/concourse/scripts/docker-lib.sh
    ln -s /usr/libexec/docker/docker-runc-current /usr/bin/docker-runc
    start_docker
}

# Setup common environment
setup_gpadmin
install_cmake
install_extra_build_dependencies
install_gpdb
setup_gpadmin_bashrc
# cmake executable
export PATH=${CMAKE_HOME}/bin:$PATH

# Do the special setup with root permission for the each task, then run the real task script with
# gpadmin. bashrc won't be read by 'su', it needs to be sourced explicitly.
case "$1" in
build)
    start_docker_server
    # run the build need run as root
    # for build task $2 means the container name suffix
    export CONTAINER_NAME_SUFFIX="$2"
    /home/gpadmin/plcontainer_src/concourse/scripts/build_plcontainer_cmake.sh "$2"
    # save doker file
    docker save python39."${CONTAINER_NAME_SUFFIX}" -o plcontainer_artifacts/plcontainer_python3_shared.tar.gz
    # r-image
    docker save r."${CONTAINER_NAME_SUFFIX}" -o plcontainer_artifacts/plcontainer_r_shared.tar.gz
    docker volume rm "$(docker volume ls -q)"
    ;;
test)
    start_docker_server
    # gpadmin docker permission
    usermod -aG docker gpadmin
    # we need to this permission without reboot
    chown gpadmin /var/run/docker.sock
    # print the test diff to stdout in our CI
    export SHOW_REGRESS_DIFF=1
    # test python39
    # $3 is test py schedule
    # $4 is test r schedule
    su gpadmin -c \
        "source /home/gpadmin/.bashrc &&\
            /home/gpadmin/plcontainer_src/concourse/scripts/test_plcontainer_py39.sh $2 $3" 

    # test r
    su gpadmin -c \
        "source /home/gpadmin/.bashrc &&\
            /home/gpadmin/plcontainer_src/concourse/scripts/test_plcontainer_r.sh $2 $4"
    docker volume rm "$(docker volume ls -q)"
    ;;
*)
    echo "Unknown target task $1"
    exit 1
    ;;
esac
