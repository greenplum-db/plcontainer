\! plcontainer runtime-add -r plc_python_shared_net -i python39.${CONTAINER_NAME_SUFFIX} -l python3 --setting enable_network=yes

CREATE FUNCTION access_network_err() RETURNS int AS $$
  # container: plc_python_shared
  import urllib.request
  try:
    return urllib.request.urlopen("https://www.vmware.com/").status
  except:
    return 0
$$ LANGUAGE plcontainer;

CREATE FUNCTION access_network_ok() RETURNS int AS $$
  # container: plc_python_shared_net
  import urllib.request
  return urllib.request.urlopen("https://www.vmware.com/").status
$$ LANGUAGE plcontainer;

select * from access_network_err();
select * from access_network_ok();

drop function access_network_err();
drop function access_network_ok();

\! plcontainer runtime-delete -r plc_python_shared_net
