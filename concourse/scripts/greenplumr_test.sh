#!/bin/bash -l

set -exo pipefail

CWDIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
TOP_DIR=${CWDIR}/../../../
GPDB_CONCOURSE_DIR=${TOP_DIR}/gpdb_src/concourse/scripts

source "${GPDB_CONCOURSE_DIR}/common.bash"
function test(){

    cat > /home/gpadmin/test.sh <<-EOF
	#!/bin/bash -l
        set -exo pipefail

        pushd ${TOP_DIR}/greenplum-r        

          source ${TOP_DIR}/gpdb_src/gpAux/gpdemo/gpdemo-env.sh
          source /usr/local/greenplum-db-devel/greenplum_path.sh
          createdb test

          # start test
          R CMD check .
        popd

	EOF

    chown -R gpadmin:gpadmin $(pwd)
    chown gpadmin:gpadmin /home/gpadmin/test.sh
    chmod a+x /home/gpadmin/test.sh
    su gpadmin -c "/bin/bash /home/gpadmin/test.sh $(pwd)"
}

function prepare_lib() {
    ${CWDIR}/install_r_package.R devtools
    ${CWDIR}/install_r_package.R testthat
    ${CWDIR}/install_r_package.R DBI
    ${CWDIR}/install_r_package.R RPostgreSQL
}

function setup_gpadmin_user() {
   ${GPDB_CONCOURSE_DIR}/setup_gpadmin_user.bash
}

function install_pkg()
{
case $OSVER in
centos*)
    yum install -y epel-release
    yum install -y R
    ;;
ubuntu*)
    apt update
    DEBIAN_FRONTEND=noninteractive apt install -y r-base pkg-config texlive-latex-base texinfo texlive-fonts-extra
    ;;
*)
    echo "unknown OSVER = $OSVER"
    exit 1
    ;;
esac
}

function _main() {
    time install_pkg
    time install_gpdb
    time setup_gpadmin_user

    time make_cluster
    time prepare_lib
    time test
}

_main "$@"
