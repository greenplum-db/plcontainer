-- start_ignore
\! plcontainer runtime-add -r plc_python3_shared_gpu -i ${CONTAINER_NAME_SUFFIX_PYTHON} -l python3
\! plcontainer runtime-backup -f test_config_gpu.xml
\! ./test_assign_gpu.py --file ./test_config_gpu.xml -i --runtime plc_python3_shared_gpu --action all
\! plcontainer runtime-restore -f test_config_gpu.xml
\! rm test_config_gpu.xml
-- end_ignore

create function with_conf_gpu() returns int as $$
  # container: plc_python3_shared_gpu
  return 0
$$ LANGUAGE plcontainer;

-- gpu=all means assign all available to this runtime. and not emit error when the GPU is not exist
select * from with_conf_gpu();

drop function with_conf_gpu();

-- start_ignore
\! plcontainer runtime-delete -r plc_python3_shared_gpu
-- end_ignore
