select count(*) from plcontainer_containers_summary();

CREATE ROLE pluser;

SET ROLE pluser;

CREATE OR REPLACE FUNCTION pyconf() RETURNS int4 AS $$
# container: plc_python_shared
return 10
$$ LANGUAGE plcontainer;

SET ROLE gpadmin;

SELECT pyconf();
select count(*) from plcontainer_containers_summary();

SET ROLE pluser;

select count(*) from plcontainer_containers_summary();

SET ROLE gpadmin;

DROP FUNCTION pyconf();
DROP ROLE pluser;

-- Test non-exsited images
CREATE OR REPLACE FUNCTION py_no_exsited() RETURNS int4 AS $$
# container: plc_python_shared1
return 10
$$ LANGUAGE plcontainer;

SELECT py_no_exsited();
