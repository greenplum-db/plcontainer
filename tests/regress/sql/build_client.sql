-- start_ignore
\! plcontainer runtime-add -r plc_python2_build -i ${CONTAINER_NAME_SUFFIX_PYTHON} -l python -s roles=$USER -s client=python27
-- end_ignore
select * from plcontainer_refresh_config;

create function build_client() returns int as $$
  # container: plc_python2_build
  return 0
$$ LANGUAGE plcontainer;

\! mv $GPHOME/bin/plcontainer_clients/client_python27_ubuntu-18.04 $GPHOME/bin/plcontainer_clients/__client_python27_ubuntu-18.04
select build_client();
\! mv $GPHOME/bin/plcontainer_clients/__client_python27_ubuntu-18.04 $GPHOME/bin/plcontainer_clients/client_python27_ubuntu-18.04

-- start_ignore
\! plcontainer runtime-delete -r plc_python2_build
-- end_ignore

drop function build_client();
