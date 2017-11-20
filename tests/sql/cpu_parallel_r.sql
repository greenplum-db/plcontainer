CREATE OR REPLACE FUNCTION r_cpu_intensive() RETURNS integer AS $$
# container: plc_r_shared
primes <- function(n){
    p <- 2:n
    i <- 1
    while (p[i] <= sqrt(n)) {
        p <-  p[p %% p[i] != 0 | p==p[i]]
        i <- i+1
    }
    return(length(p))
}
ret <- primes(100000)
return (ret)
$$ LANGUAGE plcontainer;

CREATE TABLE NUM_OF_LOOPS_CPU_R (num int, aux int);
insert into NUM_OF_LOOPS_CPU_R values (1,1),(2,2);
