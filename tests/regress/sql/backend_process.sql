SET plcontainer.backend_type = 'process';

CREATE OR REPLACE FUNCTION hello() RETURNS text AS $$
# container: plc_python_shared
return 'hello'
$$ LANGUAGE plcontainer;

SELECT hello();

SHOW plcontainer.backend_type;

RESET plcontainer.backend_type;

SHOW plcontainer.backend_type;
