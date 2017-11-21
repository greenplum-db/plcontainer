--  Test <defunct> processes are reaped after a new backend is created.
select pykillself();
SELECT pg_sleep(5);
-- Wait for 5 seconds so that cleanup processes exit.
\!ps -ef |grep [p]ostgres|grep defunct |wc -l
-- Then start the backend so that those <defunct> processes could be reaped.
select pyzero();
\!ps -ef |grep [p]ostgres|grep defunct |wc -l

-- Test function ok immediately after container is kill-9-ed.
select pyzero();
select pykillself();
select pyzero();

--  Test shared path write permission for unix domain socket connection.
select py_shared_path_perm();
select r_shared_path_perm();
