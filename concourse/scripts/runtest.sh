set -eox pipefail; \
export MASTER_DATA_DIRECTORY=/data/gpdata/master/gpseg-1;
source /usr/local/greenplum-db-devel/greenplum_path.sh;

plcontainer image-add -f /usr/local/greenplum-db-devel/share/postgresql/plcontainer/plcontainer-python-images.tar.gz;
plcontainer image-add -f /usr/local/greenplum-db-devel/share/postgresql/plcontainer/plcontainer-r-images.tar.gz;

export MASTER_DATA_DIRECTORY=/data/gpdata/master/gpseg-1;
gpconfig -c gp_resource_manager -v "group";
gpstop -arf;
psql -d postgres -f /usr/local/greenplum-db-devel/share/postgresql/plcontainer/plcontainer_install.sql;
psql -d postgres -c "create resource group plgroup with(concurrency=0,cpu_rate_limit=10,memory_limit=30,memory_auditor='cgroup');";

groupid=`psql -d postgres -t -q -c "select groupid from gp_toolkit.gp_resgroup_config where groupname='plgroup';"`;
groupid=`echo $groupid | awk '{$1=$1};1'`;
plcontainer runtime-add -r plc_python_shared -i pivotaldata/plcontainer_python_shared:devel -l python -s use_container_logging=yes -s resource_group_id=${groupid};
plcontainer runtime-add -r plc_r_shared -i pivotaldata/plcontainer_r_shared:devel -l r -s use_container_logging=yes -s resource_group_id=${groupid};

pushd plcontainer_src/tests/isolation2;
timeout -s 9 60m make resgroup;
popd;

psql -d postgres -c "alter resource group admin_group set cpu_rate_limit 20;";
psql -d postgres -c "alter resource group admin_group set memory_limit 35;";
pushd plcontainer_src/tests;
timeout -s 9 60m make resgroup;
popd;
