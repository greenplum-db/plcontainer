#!/bin/bash

# prepare for plcontainer tests
# we need to set two argments for the env
# 1. image name 


# ---
# this script is mainly for `CI`
# docker images list --> grep if we have these images if so we do nothing
# if not we ues command plcontainer-image-add to add
plcontainer runtime-delete -r plc_python_shared
plcontainer runtime-delete -r plc_python_shared_oom
plcontainer runtime-delete -r plc_r_shared 
# plcontainer runtime-add -r plc_python_shared -i "${CONTAINER_NAME_SUFFIX_PYTHON}:latest" -l python3
# plcontainer runtime-add -r plc_python_shared_oom -i "${CONTAINER_NAME_SUFFIX_PYTHON}:latest" -l python3 -s memory_mb=100
plcontainer runtime-add -r plc_python_shared -i ttt:latest -l python3
plcontainer runtime-add -r plc_python_shared_oom -i ttt:latest -l python3 -s memory_mb=100

# plcontainer runtime-add -r plc_r_shared -i "${CONTAINER_NAME_SUFFIX_R}:latest" -l r
plcontainer runtime-add -r plc_r_shared -i tttt:latest -l r
