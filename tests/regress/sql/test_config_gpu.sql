-- start_ignore
\! plcontainer runtime-add -r plc_python3_shared_gpu -i ${CONTAINER_NAME_SUFFIX_PYTHON} -l python3 -s roles=$USER $(./data/auto_select_client.py -i ${CONTAINER_NAME_SUFFIX_PYTHON})
\! plcontainer runtime-backup -f test_config_gpu.xml
\! ./data/test_assign_gpu.py --file ./test_config_gpu.xml -i --runtime plc_python3_shared_gpu --action all
\! plcontainer runtime-restore -f test_config_gpu.xml
\! rm test_config_gpu.xml
-- end_ignore
select * from plcontainer_refresh_config;

create function with_conf_gpu() returns int as $$
  # container: plc_python3_shared_gpu
  return 0
$$ LANGUAGE plcontainer;

-- gpu=all means assign all available to this runtime. and not emit error when the GPU is not exist
-- start_ignore
-- TODO
-- Failed to start container return code: 500, detail: {"message":"could not select device driver \"\" with capabilities: [[gpu]]"}
select * from with_conf_gpu();
-- end_ignore

-- start_ignore
\! plcontainer runtime-delete -r plc_python3_shared_gpu
-- end_ignore

-- start_ignore
\! plcontainer runtime-add -r plc_python3_shared_gpu -i ${CONTAINER_NAME_SUFFIX_PYTHON} -l python3 $(./data/auto_select_client.py -i ${CONTAINER_NAME_SUFFIX_PYTHON})
\! plcontainer runtime-backup -f test_config_gpu.xml
\! ./data/test_assign_gpu.py --file ./test_config_gpu.xml -i --runtime plc_python3_shared_gpu --action all
\! plcontainer runtime-restore -f test_config_gpu.xml
\! rm test_config_gpu.xml
-- end_ignore
select * from plcontainer_refresh_config;

-- strict permission check, expect error
select * from with_conf_gpu();

drop function with_conf_gpu();

-- start_ignore
\! plcontainer runtime-delete -r plc_python3_shared_gpu
-- end_ignore
