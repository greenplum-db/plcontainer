-- Check if the "plpy" module is created
CREATE OR REPLACE FUNCTION check_plpy_created() RETURNS bool AS $$
# container: plc_python_shared
import sys
return 'plpy' in sys.modules
$$ language plcontainer;

SELECT check_plpy_created();

-- Check if 'plpy' is a built-in module
CREATE OR REPLACE FUNCTION check_plpy_builtin_module() RETURNS bool AS $$
# container: plc_python_shared
import sys
return 'plpy' in sys.builtin_module_names
$$ LANGUAGE plcontainer;

SELECT check_plpy_builtin_module();

CREATE OR REPLACE FUNCTION dir_plpy() RETURNS SETOF text AS $$
# container: plc_python_shared
return dir(plpy)
$$ LANGUAGE plcontainer;

SELECT att_name FROM dir_plpy() AS att_name WHERE att_name NOT LIKE '\_%' 
ORDER BY att_name;
