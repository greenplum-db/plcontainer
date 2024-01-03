-- start_ignore
\! docker ps -a
-- end_ignore
select count(*) from plcontainer_containers_summary() WHERE "UP_TIME" LIKE 'Up %';

SET client_min_messages TO WARNING;
CREATE ROLE pluser;
RESET client_min_messages;

SET ROLE pluser;

CREATE OR REPLACE FUNCTION pyconf() RETURNS int4 AS $$
# container: plc_python_shared
return 10
$$ LANGUAGE plcontainer;

SET ROLE gpadmin;

SELECT pyconf();
-- start_ignore
\! docker ps -a
-- end_ignore
select count(*) from plcontainer_containers_summary() WHERE "UP_TIME" LIKE 'Up %';

SET ROLE pluser;

select count(*) from plcontainer_containers_summary() WHERE "UP_TIME" LIKE 'Up %';

SET ROLE gpadmin;

DROP FUNCTION pyconf();
DROP ROLE pluser;

-- Test non-exsited images
CREATE OR REPLACE FUNCTION py_no_exsited() RETURNS int4 AS $$
# container: plc_python_shared1
return 10
$$ LANGUAGE plcontainer;

SELECT py_no_exsited();

-- start_ignore
\! docker pull alpine
\! docker save alpine | gzip > ./alpine.tar.gz
\! docker rmi alpine
-- end_ignore

\! plcontainer image-add -f ./alpine.tar.gz > /dev/null 2>&1 && echo $?
\! docker images --format '{{.Repository}}' | grep -P '^alpine' | sort | uniq
\! plcontainer image-delete -i alpine > /dev/null 2>&1 && echo $?
\! docker images --format '{{.Repository}}' | grep -P '^alpine' | sort | uniq

\! plcontainer image-add -f ./alpine.tar.gz --hosts 'localhost,localhost' > /dev/null 2>&1 && echo $?
\! docker images --format '{{.Repository}}' | grep -P '^alpine' | sort | uniq
\! plcontainer image-delete -i alpine --hosts 'localhost' > /dev/null 2>&1 && echo $?
\! docker images --format '{{.Repository}}' | grep -P '^alpine' | sort | uniq

\! plcontainer image-add -f ./alpine.tar.gz --hosts 'unreachable' | grep ssh
\! docker images --format '{{.Repository}}' | grep -P '^alpine' | sort | uniq

\! plcontainer remote-setup --hosts 'unreachable' | grep ssh
\! ls -A -1 /tmp/xxxxremotedockertestxxx/plcontainer_clients

\! plcontainer remote-setup --hosts 'localhost' --clientdir '/tmp/xxxxremotedockertestxxx/plcontainer_clients' > /dev/null 2>&1 && echo $?
\! ls -A -1 /tmp/xxxxremotedockertestxxx/plcontainer_clients
\! rm -rf /tmp/xxxxremotedockertestxxx
\! rm ./alpine.tar.gz
