#!/bin/bash

set -eox pipefail

scp -r plcontainer_gpdb_build mdw:/tmp/
scp -r plcontainer_src mdw:~/
ssh mdw "bash -c \" \
set -eox pipefail; \
export MASTER_DATA_DIRECTORY=/data/gpdata/master/gpseg-1; \
source /usr/local/greenplum-db-devel/greenplum_path.sh; \
gppkg -i /tmp/plcontainer_gpdb_build/plcontainer-concourse-centos*.gppkg; \
\""

scp -r plcontainer_client_docker_image/plcontainer-devel-images.tar.gz mdw:/usr/local/greenplum-db-devel/share/postgresql/plcontainer/

ssh mdw "bash -c \" \
set -eox pipefail; \
export MASTER_DATA_DIRECTORY=/data/gpdata/master/gpseg-1; \
source /usr/local/greenplum-db-devel/greenplum_path.sh; \
plcontainer install -n plc_python_shared -i /usr/local/greenplum-db-devel/share/postgresql/plcontainer/plcontainer-devel-images.tar.gz; \
gpstop -arf
psql -d postgres -f /usr/local/greenplum-db-devel/share/postgresql/plcontainer/plcontainer_install.sql; \
pushd plcontainer_src/tests; \
make tests; \
popd; \
\""
