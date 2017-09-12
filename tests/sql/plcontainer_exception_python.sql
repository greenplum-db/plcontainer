--  Test <defunct> processes are reaped after a new backend is created.
select pykillself();
select pykillself();
SELECT pg_sleep(5);
-- Then start the backend.
\!ps -ef |grep [p]ostgres|grep defunct |wc -l
select pyzero();
\!ps -ef |grep [p]ostgres|grep defunct |wc -l

-- Test function ok immediately after container is kill-9-ed.
select pyzero();
select pykillself();
select pyzero();
