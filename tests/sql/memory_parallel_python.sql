-- memory consuming tests for python
CREATE OR REPLACE FUNCTION py_memory_allocate(num int) RETURNS int2 AS $$
# container: plc_python_shared
allocate = 'a' * num * 1024 * 1024
return 1
$$ LANGUAGE plcontainer;


CREATE OR REPLACE FUNCTION py_memory_allocate_return(num int) RETURNS text[] AS $$
# container: plc_python_shared
import time
allocate = 'a' * num * 1024 * 1024
time.sleep(1)
return ['Allocate:' + str(num) + 'MB as ' + allocate[1024*1024*(num/2)]]
$$ LANGUAGE plcontainer;

CREATE TABLE NUM_OF_LOOPS_PY (num int, aux int);
INSERT into NUM_OF_LOOPS_PY (num, aux) select 128 as num, aux from generate_series(1, 128) as aux;
INSERT into NUM_OF_LOOPS_PY (num, aux) select 127 as num, aux from generate_series(1, 128) as aux;
