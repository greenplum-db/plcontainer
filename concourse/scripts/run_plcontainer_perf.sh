#!/bin/bash

#------------------------------------------------------------------------------
#
# Copyright (c) 2017-Present Pivotal Software, Inc
#
#------------------------------------------------------------------------------

set -eox pipefail

scp -r plcontainer_gpdb_build mdw:/tmp/
scp -r plcontainer_src mdw:~/

ssh mdw "bash -c \" \
set -eox pipefail; \
export MASTER_DATA_DIRECTORY=/data/gpdata/master/gpseg-1; \
source /usr/local/greenplum-db-devel/greenplum_path.sh; \
gppkg -i /tmp/plcontainer_gpdb_build/plcontainer-*.gppkg; \
\""

scp -r plcontainer_pyclient_docker_image/plcontainer-*-images*.tar.gz mdw:/usr/local/greenplum-db-devel/share/postgresql/plcontainer/plcontainer-python-images.tar.gz
scp -r plcontainer_rclient_docker_image/plcontainer-*-images*.tar.gz mdw:/usr/local/greenplum-db-devel/share/postgresql/plcontainer/plcontainer-r-images.tar.gz

ssh mdw "bash -c \" \
set -eox pipefail; \
export MASTER_DATA_DIRECTORY=/data/gpdata/master/gpseg-1; \
source /usr/local/greenplum-db-devel/greenplum_path.sh; \
plcontainer image-add -f /usr/local/greenplum-db-devel/share/postgresql/plcontainer/plcontainer-python-images.tar.gz; \
plcontainer image-add -f /usr/local/greenplum-db-devel/share/postgresql/plcontainer/plcontainer-r-images.tar.gz; \
plcontainer runtime-add -r plc_python_shared -i pivotaldata/plcontainer_python_shared:devel -l python -s use_container_logging=yes; \
plcontainer runtime-add -r plc_r_shared -i pivotaldata/plcontainer_r_shared:devel -l r -s use_container_logging=yes; \
plcontainer runtime-add -r plc_python_shared_oom -i pivotaldata/plcontainer_python_shared:devel -l python -s use_container_logging=yes -s memory_mb=100; \
gpstop -arf ; \
psql -d postgres -f /usr/local/greenplum-db-devel/share/postgresql/plcontainer/plcontainer_install.sql; \
psql -d postgres -f plcontainer_src/tests/perfsql/function_setup.sql; \
psql -d postgres -f plcontainer_src/tests/perfsql/perf_prepare1.sql; \
nohup psql -d postgres -f plcontainer_src/tests/perfsql/perf_pl1.sql > perf_pl1 2>&1; \
nohup psql -d postgres -f plcontainer_src/tests/perfsql/perf_py1.sql > perf_py1 2>&1; \
\""

scp mdw:~/perf_pl1 plcontainer_perf_result/perf_pl1
scp mdw:~/perf_py1 plcontainer_perf_result/perf_py1
