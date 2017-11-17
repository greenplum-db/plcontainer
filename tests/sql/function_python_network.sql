\! plcontainer configure -f $(pwd)/plcontainer_configuration_test.xml -y

drop table if exists test_python_network;

create table test_python_network(i int);
insert into test_python_network select generate_series(0,10);

create or replace function python_network_test1(i integer) returns integer as $$
#container: plc_python_network
return i + 1024
$$ language plcontainer;

create or replace function python_network_test2() returns integer as $$
#container: plc_python_network
return 1024
$$ language plcontainer;

select python_network_test1(i) from test_python_network;
select python_network_test2() from test_python_network;

-- Test permission.
CREATE OR REPLACE FUNCTION py_shared_path_perm_network() RETURNS integer AS $$
# container: plc_python_network
import os
os.open("/tmp/plcontainer/test_file", os.O_RDWR|os.O_CREAT)
return 0
$$ LANGUAGE plcontainer;

select py_shared_path_perm_network();

\! plcontainer configure --restore -y
