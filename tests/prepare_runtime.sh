#!/bin/bash
set -ex

# prepare for plcontainer tests
# this script is for local install
# we need to set two argments for the env
# this script is mainly for `CI` cmake -> make prepare_runtime
# if we need to install it locally we need set CONTAINER_NAME_SUFFIX_PYTHON
# and CONTAINER_NAME_SUFFIX_R env
# first delete the runtime make sure we install the clean one
plcontainer runtime-delete -r plc_python_shared
plcontainer runtime-delete -r plc_python_shared_oom
plcontainer runtime-delete -r plc_r_shared

# then add the python and r container image
plcontainer runtime-add -r plc_python_shared -i "${CONTAINER_NAME_SUFFIX_PYTHON}:latest" -l python3
plcontainer runtime-add -r plc_python_shared_oom -i "${CONTAINER_NAME_SUFFIX_PYTHON}:latest" -l python3 -s memory_mb=100
plcontainer runtime-add -r plc_r_shared -i "${CONTAINER_NAME_SUFFIX_R}:latest" -l r

# for test faultinject_python we rm all the container first
containers_cnt=$(ssh `psql -d ${PL_TESTDB} -c 'select address from gp_segment_configuration where dbid=2' -t -A` docker ps -a -q | wc -l)
if ((containers_cnt > 0)); then
    ssh `psql -d ${PL_TESTDB} -c 'select address from gp_segment_configuration where dbid=2' -t -A` docker rm -f $(docker ps -a -q)
fi
