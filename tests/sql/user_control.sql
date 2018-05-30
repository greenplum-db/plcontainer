CREATE OR REPLACE FUNCTION pylog100() RETURNS double precision AS $$
# container: plc_python_user
import math
return math.log10(100)
$$ LANGUAGE plcontainer;

-- start_ignore
\! plcontainer runtime-add -r plc_python_user -i pivotaldata/plcontainer_python_shared:devel -l python -s use_container_logging=yes -s roles=gpadmin;
SELECT plcontainer_refresh_local_config(false);
-- end_ignore

-- should success
SELECT pylog100();

-- start_ignore
\! plcontainer runtime-replace -r plc_python_user -i pivotaldata/plcontainer_python_shared:devel -l python -s use_container_logging=yes -s roles=plc1;
SELECT plcontainer_refresh_local_config(false);
-- end_ignore

-- should success, as gpadmin is admin
SELECT pylog100();

-- start_ignore
\! plcontainer runtime-replace -r plc_python_user -i pivotaldata/plcontainer_python_shared:devel -l python -s use_container_logging=yes -s roles=plc1,gpadmin;
SELECT plcontainer_refresh_local_config(false);
-- end_ignore

-- should success
SELECT pylog100();

-- start_ignore
\! plcontainer runtime-replace -r plc_python_user -i pivotaldata/plcontainer_python_shared:devel -l python -s use_container_logging=yes -s roles=plc1;
SELECT plcontainer_refresh_local_config(false);
-- end_ignore

-- start_ignore
DROP ROLE plc1;
DROP ROLE plc2;
-- end_ignore

CREATE ROLE plc1;
CREATE ROLE plc2;

SET ROLE plc1;

-- should success
SELECT pylog100();

SET ROLE plc2;

-- should fail
SELECT pylog100();

SET ROLE gpadmin;

-- start_ignore
\! plcontainer runtime-replace -r plc_python_user -i pivotaldata/plcontainer_python_shared:devel -l python -s use_container_logging=yes -s roles=gpadmin;
SELECT plcontainer_refresh_local_config(false);
-- end_ignore

SET ROLE plc1;

-- should fail
SELECT pylog100();

SET ROLE gpadmin;

-- should success
SELECT pylog100();
