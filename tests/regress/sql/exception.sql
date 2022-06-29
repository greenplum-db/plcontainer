-- Test trigger (not supported at this moment).
CREATE TABLE trigger_tbl (a int);

CREATE OR REPLACE FUNCTION test_trigger_func() RETURNS TRIGGER AS $$
# container: plc_python_shared
plpy.notice("trigger not supported");
$$ LANGUAGE plcontainer;

CREATE TRIGGER test_trigger AFTER INSERT ON trigger_tbl FOR EACH ROW EXECUTE PROCEDURE test_trigger_func();

INSERT INTO trigger_tbl values(0);

DROP TRIGGER test_trigger on trigger_tbl;
DROP TABLE trigger_tbl;

--  Test <defunct> processes are reaped after a new backend is created.
select pykillself();
SELECT pg_sleep(5);
-- Wait for 5 seconds so that cleanup processes exit.
\!ps -ef |grep [p]ostgres|grep defunct |wc -l
-- Then start the backend so that those <defunct> processes could be reaped.
select pyzero();
\!ps -ef |grep [p]ostgres|grep defunct |wc -l

-- Test function ok immediately after container is kill-9-ed.
select pykillself();
select pyzero();
select rkillself();
select rint(0);

-- Test function ok immediately after container captures signal sigsegv.
select pysegvself();
select pyzero();
select rsegvself();
select rint(0);

-- Test function ok immediately after container exits.
select pyexit();
select pyzero();
select rexit();
select rint(0);
