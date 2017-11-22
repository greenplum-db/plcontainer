-- Install a helper function to inject faults, using the fault injection
-- mechanism built into the server.
set log_min_messages='DEBUG1';
CREATE EXTENSION gp_inject_fault;

CREATE OR REPLACE FUNCTION pyint(i int) RETURNS int AS $$
# container: plc_python_shared
return i+1
$$ LANGUAGE plcontainer;

CREATE TABLE tbl(i int);

INSERT INTO tbl SELECT * FROM generate_series(1, 10);

-- reset the injection points
SELECT gp_inject_fault('plcontainer_before_container_started', 'reset', 2);
SELECT gp_inject_fault('plcontainer_before_container_connected', 'reset', 2);
SELECT gp_inject_fault('plcontainer_after_send_request', 'reset', 2);
SELECT gp_inject_fault('plcontainer_after_recv_request', 'reset', 2);
SELECT gp_inject_fault('plcontainer_before_udf_finish', 'reset', 2);

-- start_ignore
-- QE crash after start a container
show optimizer;
SELECT gp_inject_fault('plcontainer_before_container_started', 'fatal', 2);
SELECT pyint(i) from tbl;
SELECT pg_sleep(5);
-- end_ignore

\! ssh `psql -d ${PL_TESTDB} -c 'select address from gp_segment_configuration where dbid=2' -t -A` docker ps -a | wc -l
\! ssh `psql -d ${PL_TESTDB} -c 'select address from gp_segment_configuration where dbid=2' -t -A` ps -ef | grep -v grep | grep "plcontainer cleaner" | wc -l
SELECT sum(pyint(i)) from tbl;

-- start_ignore
-- Start a container

-- QE crash when connnecting to an existing container
SELECT gp_inject_fault('plcontainer_before_container_connected', 'fatal', 2);
SELECT pyint(i) from tbl;
SELECT pg_sleep(5);
-- end_ignore

\! ssh `psql -d ${PL_TESTDB} -c 'select address from gp_segment_configuration where dbid=2' -t -A` docker ps -a | wc -l
\! ssh `psql -d ${PL_TESTDB} -c 'select address from gp_segment_configuration where dbid=2' -t -A` ps -ef | grep -v grep | grep "plcontainer cleaner" | wc -l
SELECT sum(pyint(i)) from tbl;

-- start_ignore
SELECT gp_inject_fault('plcontainer_after_send_request', 'fatal', 2);
SELECT pyint(i) from tbl;
SELECT pg_sleep(5);
-- end_ignore

\! ssh `psql -d ${PL_TESTDB} -c 'select address from gp_segment_configuration where dbid=2' -t -A` docker ps -a | wc -l
\! ssh `psql -d ${PL_TESTDB} -c 'select address from gp_segment_configuration where dbid=2' -t -A` ps -ef | grep -v grep | grep "plcontainer cleaner" | wc -l
SELECT sum(pyint(i)) from tbl;

-- start_ignore
SELECT gp_inject_fault('plcontainer_after_recv_request', 'fatal', 2);
SELECT pyint(i) from tbl;
SELECT pg_sleep(5);
-- end_ignore

\! ssh `psql -d ${PL_TESTDB} -c 'select address from gp_segment_configuration where dbid=2' -t -A` docker ps -a | wc -l
\! ssh `psql -d ${PL_TESTDB} -c 'select address from gp_segment_configuration where dbid=2' -t -A` ps -ef | grep -v grep | grep "plcontainer cleaner" | wc -l
SELECT sum(pyint(i)) from tbl;

-- start_ignore
SELECT gp_inject_fault('plcontainer_before_udf_finish', 'fatal', 2);
SELECT pyint(i) from tbl;
SELECT pg_sleep(5);
-- end_ignore

\! ssh `psql -d ${PL_TESTDB} -c 'select address from gp_segment_configuration where dbid=2' -t -A` docker ps -a | wc -l
\! ssh `psql -d ${PL_TESTDB} -c 'select address from gp_segment_configuration where dbid=2' -t -A` ps -ef | grep -v grep | grep "plcontainer cleaner" | wc -l

-- reset the injection points
SELECT gp_inject_fault('plcontainer_before_container_started', 'reset', 2);
SELECT gp_inject_fault('plcontainer_before_container_connected', 'reset', 2);
SELECT gp_inject_fault('plcontainer_after_send_request', 'reset', 2);
SELECT gp_inject_fault('plcontainer_after_recv_request', 'reset', 2);
SELECT gp_inject_fault('plcontainer_before_udf_finish', 'reset', 2);

DROP TABLE tbl;
DROP FUNCTION pyint(i int);
DROP EXTENSION gp_inject_fault;
