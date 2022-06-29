-- do the parallel tests
select py_memory_allocate(128);
select py_memory_allocate(128);
select py_memory_allocate(128);
select py_memory_allocate(128);
select py_memory_allocate(128);

select py_memory_allocate_return(128);
select py_memory_allocate_return(128);
select py_memory_allocate_return(128);
select py_memory_allocate_return(128);
select py_memory_allocate_return(128);

-- test on QE
select count(py_memory_allocate(num)) from NUM_OF_LOOPS_PY;
