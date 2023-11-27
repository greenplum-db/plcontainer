-- start_ignore
\! plcontainer runtime-add -r plc_python3_shared_remote_docker -i ${CONTAINER_NAME_SUFFIX_PYTHON} -l python3 -s enable_network=yes -s roles=$USER
\! plcontainer runtime-backup -f test_config_remote_docker.xml
\! ./data/test_assign_backend.py ./test_config_remote_docker.xml -r plc_python3_shared_remote_docker
\! plcontainer runtime-restore -f test_config_remote_docker.xml
\! rm test_config_remote_docker.xml
-- end_ignore
select * from plcontainer_refresh_config order by gp_segment_id DESC;

create function with_conf_remote_docker() returns TEXT as $$
  # container: plc_python3_shared_remote_docker
  return 'hello'
$$ LANGUAGE plcontainer;

select * from with_conf_remote_docker();

-- start_ignore
\! plcontainer runtime-delete -r plc_python3_shared_remote_docker
-- end_ignore

drop function with_conf_remote_docker();

