CREATE OR REPLACE FUNCTION r_memory_parallel(num int) RETURNS int2 AS $$
# container: plc_r_shared
a <- matrix(1, ncol=1024, nrow=1024*num)
return (1)
$$ LANGUAGE plcontainer;

CREATE TABLE NUM_OF_LOOPS_R (num int, aux int);
INSERT into NUM_OF_LOOPS_R (num, aux) select 20 as num, aux from generate_series(1, 128) as aux;
INSERT into NUM_OF_LOOPS_R (num, aux) select 19 as num, aux from generate_series(1, 128) as aux;
