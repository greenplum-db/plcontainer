-- memory consuming tests for python
CREATE OR REPLACE FUNCTION py_memory_allocate_c(num int) RETURNS void AS $$
# container: plc_python_shared
allocate = 'a' * num * 1024 * 1024
$$ LANGUAGE plcontainer;


CREATE OR REPLACE FUNCTION py_memory_allocate_return_c(num int) RETURNS text[] AS $$
# container: plc_python_shared
allocate = 'a' * num * 1024 * 1024
return ['Allocate:' + str(num) + 'MB as ' + allocate[1024*1024*(num/2)]]
$$ LANGUAGE plcontainer;

select py_memory_allocate_c(128);
select py_memory_allocate_c(128);
select py_memory_allocate_c(128);
select py_memory_allocate_c(128);
select py_memory_allocate_c(128);
select py_memory_allocate_c(128);
select py_memory_allocate_c(128);
select py_memory_allocate_c(128);
select py_memory_allocate_c(128);
select py_memory_allocate_c(128);

select py_memory_allocate_return_c(16);
select py_memory_allocate_return_c(32);
select py_memory_allocate_return_c(64);
select py_memory_allocate_return_c(128);
select py_memory_allocate_return_c(256);
select py_memory_allocate_return_c(256);
select py_memory_allocate_return_c(500);
