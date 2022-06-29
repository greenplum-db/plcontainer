CREATE OR REPLACE FUNCTION r_memory_test_c(num int) RETURNS void AS $$
# container: plc_r_shared
a <- matrix(1, ncol=1024, nrow=1024*num)
$$ LANGUAGE plcontainer;

select r_memory_test_c(10);
select r_memory_test_c(20);
select r_memory_test_c(40);
select r_memory_test_c(80);
select r_memory_test_c(100);
select r_memory_test_c(120);
