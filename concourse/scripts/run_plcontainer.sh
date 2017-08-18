#!/bin/bash

set -e -x

ssh mdw "bash -c \" \
export MASTER_DATA_DIRECTORY=/data/gpdata/master/gpseg-1; \
source /usr/local/greenplum-db-devel/greenplum_path.sh; \
gppkg -i plcontainer_gpdb_build/plcontainer-concourse-centos6.gppkg; \
plcontainer-config --reset; \
gpstop -arf; \
psql -d postgres -f /usr/local/greenplum-db-devel/share/postgresql/plcontainer/plcontainer_install.sql; \
pushd plcontainer_src/tests; \
make tests; \
popd; \
\""
