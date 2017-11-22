-- Install a helper function to inject faults, using the fault injection
-- mechanism built into the server.
set log_min_messages='DEBUG1';
CREATE EXTENSION gp_inject_fault;

CREATE OR REPLACE FUNCTION pyint(i int) RETURNS int AS $$
# container: plc_python_shared
return i+1
$$ LANGUAGE plcontainer;

-- reset the injection points
SELECT gp_inject_fault('plcontainer_before_container_started', 'reset', 1);
SELECT gp_inject_fault('plcontainer_after_send_request', 'reset', 1);

-- After QE log(error, ...), related docker containers should be deleted.
-- Test on entrydb.
-- start_ignore
show optimizer;
SELECT gp_inject_fault('plcontainer_before_container_started', 'error', 1);
SELECT pyint(0);
SELECT pg_sleep(5);
-- end_ignore

\! docker ps -a | wc -l
\! ps -ef | grep -v grep | grep "plcontainer cleaner" | wc -l
SELECT pyint(1);

-- start_ignore
SELECT gp_inject_fault('plcontainer_after_send_request', 'error', 1);
SELECT pyint(2);
SELECT pg_sleep(5);
-- end_ignore

\! docker ps -a | wc -l
\! ps -ef | grep -v grep | grep "plcontainer cleaner" | wc -l
SELECT pyint(3);
-- Detect for the process name change (from "plcontainer cleaner" to other).
-- In such case, above cases will still succeed as unexpected.
\! docker ps -a | wc -l
\! ps -ef | grep -v grep | grep "plcontainer cleaner" | wc -l

-- reset the injection points
SELECT gp_inject_fault('plcontainer_before_container_started', 'reset', 1);
SELECT gp_inject_fault('plcontainer_after_send_request', 'reset', 1);

DROP FUNCTION pyint(i int);
DROP EXTENSION gp_inject_fault;
