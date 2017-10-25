#!/bin/bash

set -eox pipefail

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

ssh mdw "bash -c \" \
set -eox pipefail; \
export MASTER_DATA_DIRECTORY=/data/gpdata/master/gpseg-1; \
source /usr/local/greenplum-db-devel/greenplum_path.sh; \
plcontainer install -n plc_python_shared -i /usr/local/greenplum-db-devel/share/postgresql/plcontainer/plcontainer-python-images.tar.gz -c pivotaldata/plcontainer_python_shared:devel -l python; \
plcontainer install -n plc_r_shared -i /usr/local/greenplum-db-devel/share/postgresql/plcontainer/plcontainer-r-images.tar.gz -c pivotaldata/plcontainer_r_shared:devel -l r; \
plcontainer configure -f plcontainer_src/tests/plcontainer_configuration_test.xml -y; \
gpstop -arf ; \
psql -d postgres -f /usr/local/greenplum-db-devel/share/postgresql/plcontainer/plcontainer_install.sql; \
pushd plcontainer_src/tests; \
make tests; \
popd; \
\""
