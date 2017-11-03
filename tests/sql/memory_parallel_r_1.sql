-- test parallel 
select r_memory_parallel(20);
select r_memory_parallel(20);
select r_memory_parallel(20);
select r_memory_parallel(20);
select r_memory_parallel(20);

-- test on QE
select count(r_memory_parallel(num)) from NUM_OF_LOOPS_R;
