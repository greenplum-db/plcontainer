-- do the parallel tests
select py_cpu_intensive();
select py_cpu_intensive();

-- test on QE
select py_cpu_intensive() from NUM_OF_LOOPS_CPU_PY;
select py_cpu_intensive() from NUM_OF_LOOPS_CPU_PY;
