-- Install a helper function to inject faults, using the fault injection
-- mechanism built into the server.
CREATE EXTENSION gp_inject_fault;

CREATE OR REPLACE FUNCTION pyint(i int) RETURNS int AS $$
# container: plc_python_shared
return i+1
$$ LANGUAGE plcontainer;

CREATE TABLE tbl(i int);

INSERT INTO tbl SELECT * FROM generate_series(1, 10);

-- start_ignore
-- QE crash after start a container 
SELECT gp_inject_fault('plcontainer_before_container_started', 'fatal', 2);
SELECT pyint(i) from tbl;
SELECT pg_sleep(5);
-- end_ignore

\! docker ps -a | wc -l
\! ps -ef | grep "plcontainer cleaner" | grep -v pg_regress | wc -l

-- start_ignore
-- Start a container
SELECT pyint(i) from tbl;

-- QE crash when connnecting to an existing container
SELECT gp_inject_fault('plcontainer_before_container_connected', 'fatal', 2);
SELECT pyint(i) from tbl;
SELECT pg_sleep(5);
-- end_ignore

\! docker ps -a | wc -l
\! ps -ef | grep "plcontainer cleaner" | grep -v pg_regress | wc -l

-- start_ignore
SELECT gp_inject_fault('plcontainer_after_send_request', 'fatal', 2);
SELECT pyint(i) from tbl;
SELECT pg_sleep(5);
-- end_ignore

\! docker ps -a | wc -l
\! ps -ef | grep "plcontainer cleaner" | grep -v pg_regress | wc -l

-- start_ignore
SELECT gp_inject_fault('plcontainer_after_recv_request', 'fatal', 2);
SELECT pyint(i) from tbl;
SELECT pg_sleep(5);
-- end_ignore

\! docker ps -a | wc -l
\! ps -ef | grep "plcontainer cleaner" | grep -v pg_regress | wc -l

-- start_ignore
SELECT gp_inject_fault('plcontainer_before_udf_finish', 'fatal', 2);
SELECT pyint(i) from tbl;
SELECT pg_sleep(5);
-- end_ignore

\! docker ps -a | wc -l
\! ps -ef | grep "plcontainer cleaner" | grep -v pg_regress | wc -l

-- reset the injection points
SELECT gp_inject_fault('plcontainer_before_container_started', 'reset', 2);
SELECT gp_inject_fault('plcontainer_before_container_connected', 'reset', 2);
SELECT gp_inject_fault('plcontainer_after_send_request', 'reset', 2);
SELECT gp_inject_fault('plcontainer_after_recv_request', 'reset', 2);
SELECT gp_inject_fault('plcontainer_before_udf_finish', 'reset', 2);
