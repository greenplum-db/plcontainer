select * from plcontainer_containers_summary();

CREATE ROLE pluser;

SET ROLE pluser;

CREATE OR REPLACE FUNCTION pyconf() RETURNS int4 AS $$
# container: plc_python_shared
return 10
$$ LANGUAGE plcontainer;

SET ROLE gpadmin;

SELECT pyconf();

SET ROLE pluser;

select * from plcontainer_containers_summary();

SET ROLE gpadmin;
