create or replace function test_udf_return_simple() returns int4 as $$
# container: plc_python_shared
return 10
$$ language plcontainer;
select * from test_udf_return_simple();
drop function test_udf_return_simple();

create or replace function test_udf_return_array() returns int4[] as $$
# container: plc_python_shared
return [1, 2];
$$ language plcontainer;
select * from test_udf_return_array();
drop function test_udf_return_array();

-- composite types in test_python3.sql

create or replace function test_udf_return_set_object() returns table(a float, b float) as $$
# container: plc_python_shared
from collections import namedtuple
A = namedtuple('A', 'a b')
return [A(1, 2), A(2, 3)];
$$ language plcontainer;
select * from test_udf_return_set_object();
drop function test_udf_return_set_object();

create or replace function test_udf_return_set_seq() returns table(a float, b float) as $$
# container: plc_python_shared
return [(1, 2), (2, 3)];
$$ language plcontainer;
select * from test_udf_return_set_seq();
drop function test_udf_return_set_seq();

create or replace function test_udf_return_set_dict() returns table(a float, b float) as $$
# container: plc_python_shared
return [{'a': 1, 'b': 2}, {'a': 2, 'b': 3}];
$$ language plcontainer;
select * from test_udf_return_set_dict();
drop function test_udf_return_set_dict();
