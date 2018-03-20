-- Parallel tests on QD
select py_memory_allocate_oom(1);
select py_memory_allocate_oom(1);
select py_memory_allocate_oom(1);

select py_memory_allocate_normal(512);
select py_memory_allocate_normal(512);
select py_memory_allocate_normal(512);

-- Parallel tests on QE
select count(py_memory_allocate_oom(num)) from OOM_TEST;
select count(py_memory_allocate_oom(num)) from OOM_TEST;
select count(py_memory_allocate_oom(num)) from OOM_TEST;

select count(py_memory_allocate_normal(aux)) from OOM_TEST;
select count(py_memory_allocate_oom(num)) from OOM_TEST;
select count(py_memory_allocate_normal(aux)) from OOM_TEST;
