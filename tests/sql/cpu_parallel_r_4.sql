-- test parallel 
select r_cpu_intensive();
select r_cpu_intensive();

-- test on QE
select r_cpu_intensive() from NUM_OF_LOOPS_CPU_R;
select r_cpu_intensive() from NUM_OF_LOOPS_CPU_R;
