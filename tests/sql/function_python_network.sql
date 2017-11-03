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

\! plcontainer configure --restore -y
