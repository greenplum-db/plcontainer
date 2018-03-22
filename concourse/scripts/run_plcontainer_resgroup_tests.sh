#!/bin/bash

set -eox pipefail

CLUSTER_NAME=$(cat ./cluster_env_files/terraform/name)

if [ "$TEST_OS" = centos6 ]; then
    CGROUP_BASEDIR=/cgroup
else
    CGROUP_BASEDIR=/sys/fs/cgroup
fi

if [ "$TEST_OS" = centos7 -o "$TEST_OS" = sles12 ]; then
    CGROUP_AUTO_MOUNTED=1
fi


make_cgroups_dir() {
    local gpdb_host_alias=$1
    local basedir=$CGROUP_BASEDIR

    ssh -t $gpdb_host_alias sudo bash -ex <<EOF
        for comp in cpu cpuacct memory; do
            chmod -R 777 $basedir/\$comp
            mkdir -p $basedir/\$comp/gpdb
            chown -R gpadmin:gpadmin $basedir/\$comp/gpdb
            chmod -R 777 $basedir/\$comp/gpdb
        done
EOF
}

make_cgroups_dir ccp-${CLUSTER_NAME}-0
make_cgroups_dir ccp-${CLUSTER_NAME}-1

pushd gpdb_src
./configure --prefix=/usr/local/greenplum-db-devel \
            --without-zlib --without-rt --without-libcurl \
            --without-libedit-preferred --without-docdir --without-readline \
            --disable-gpcloud --disable-gpfdist --disable-orca \
            --disable-pxf
popd
pushd gpdb_src/src/test/isolation2
make
scp -r pg_isolation2_regress mdw:/usr/local/greenplum-db-devel/lib/postgresql/pgxs/src/test/regress/
scp -r ../regress/gpstringsubs.pl mdw:/usr/local/greenplum-db-devel/lib/postgresql/pgxs/src/test/regress/
scp -r ../regress/gpdiff.pl mdw:/usr/local/greenplum-db-devel/lib/postgresql/pgxs/src/test/regress/
scp -r ../regress/atmsort.pm mdw:/usr/local/greenplum-db-devel/lib/postgresql/pgxs/src/test/regress/
scp -r ../regress/explain.pm mdw:/usr/local/greenplum-db-devel/lib/postgresql/pgxs/src/test/regress/
popd


scp -r plcontainer_gpdb_build mdw:/tmp/
scp -r plcontainer_src mdw:~/
ssh mdw "bash -c \" \
set -eox pipefail; \
export MASTER_DATA_DIRECTORY=/data/gpdata/master/gpseg-1; \
source /usr/local/greenplum-db-devel/greenplum_path.sh; \
gppkg -i /tmp/plcontainer_gpdb_build/plcontainer*.gppkg; \
\""

scp -r plcontainer_pyclient_docker_image/plcontainer-*.tar.gz mdw:/usr/local/greenplum-db-devel/share/postgresql/plcontainer/plcontainer-python-images.tar.gz
scp -r plcontainer_rclient_docker_image/plcontainer-*.tar.gz mdw:/usr/local/greenplum-db-devel/share/postgresql/plcontainer/plcontainer-r-images.tar.gz
scp -r plcontainer_src/concourse/scripts/runtest.sh mdw:~/

ssh mdw "bash -c \" \
/home/gpadmin/runtest.sh; \
\""
