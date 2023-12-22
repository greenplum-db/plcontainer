-- Check if 'plpy' is an imported module
CREATE OR REPLACE FUNCTION check_plpy_imported() RETURNS bool AS $$
# container: plc_python_shared
import sys
return 'plpy' in sys.modules
$$ language plcontainer;

SELECT check_plpy_imported();

-- Check if 'plpy' is a built-in module
CREATE OR REPLACE FUNCTION check_plpy_builtin_module() RETURNS bool AS $$
# container: plc_python_shared
import sys
return 'plpy' in sys.builtin_module_names
$$ LANGUAGE plcontainer;

SELECT check_plpy_builtin_module();
