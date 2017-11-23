------------------------------- python ---------------------------------------

-- cpu-bound
CREATE OR REPLACE FUNCTION py_cpu_intensive() RETURNS integer AS $$
# container: plc_python_shared
def is_prime(n):
    for i in range(2, n):
        if n%i == 0:
            return False
    return True
prime_num = 0
for p in range(2, 10000):
    if is_prime(p):
        prime_num = prime_num + 1
return prime_num
$$ LANGUAGE plcontainer;

-- spi with large io
CREATE OR REPLACE FUNCTION py_large_spi() RETURNS int8 AS $$
# container: plc_python_shared
output = plpy.execute("select * from generate_series(1,1000000) id")
sum = 0
for i in range(0, len(output)):
	sum += output[i]['id']
sum /= len(output)
return sum
$$ LANGUAGE plcontainer;

-- write local disk.
CREATE OR REPLACE FUNCTION py_io_intensive() RETURNS integer AS $$
# container: plc_python_shared

outfile = open("/tmp/testfile_py", "w")
allocate = '2' * 1024 * 1024 * 10
outfile.write(allocate)
outfile.close()

infile = open("/tmp/testfile_py", "r")
content = infile.read()
infile.close()

ret = sum(int(i) for i in content[:])

return ret
$$ LANGUAGE plcontainer;

--------------------------------- r -----------------------------------------

-- cpu-bound
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

-- spi with large io
CREATE OR REPLACE FUNCTION r_large_spi() RETURNS int8 AS $$
# container: plc_r_shared
res<-pg.spi.exec('select * from generate_series(1, 1000000) id')
mean(res[, 1])
$$ LANGUAGE plcontainer;

-- write local disk.
CREATE OR REPLACE FUNCTION r_io_intensive() RETURNS integer AS $$
# container: plc_r_shared
a <- matrix(1, ncol=1024, nrow=1024*10)
write.csv(a, file = "/tmp/testfile_r", row.names = F, quote = F)
b <- read.csv("/tmp/testfile_r")
ret <- sum(b)
return (ret)
$$ LANGUAGE plcontainer;
