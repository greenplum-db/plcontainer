-- cpu consuming tests for python
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
CREATE TABLE NUM_OF_LOOPS_CPU_PY (num int, aux int);
INSERT into NUM_OF_LOOPS_CPU_PY values(1,1), (2,2);
