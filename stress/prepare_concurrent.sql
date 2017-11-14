CREATE OR REPLACE FUNCTION r_function_1(num int) RETURNS int4 AS $$
# container: plc_r_shared
a <- matrix(rnorm(200), ncol=10, nrow=20)
b <- matrix(rnorm(200), ncol=20, nrow=10)
a %*% b
return (num)
$$ LANGUAGE plcontainer;

CREATE OR REPLACE FUNCTION r_function_2(num int) RETURNS int4 AS $$
# container: plc_r_shared
for (i in 1:100){
    a <- pg.spi.exec("select count(*) from NUM_OF_LOOPS where num < 5")
}
return (num)
$$ LANGUAGE plcontainer;

CREATE OR REPLACE FUNCTION r_function_3(num int) RETURNS int4 AS $$
# container: plc_r_shared
a <- 0
for (i in 1:10000){
  a <- a + num
}
return (a)
$$ LANGUAGE plcontainer;

CREATE OR REPLACE FUNCTION r_function_4(num int) RETURNS int4 AS $$
# container: plc_r_shared
return (num * 2)
$$ LANGUAGE plcontainer;

CREATE OR REPLACE FUNCTION r_function_5(num int) RETURNS int4 AS $$
# container: plc_r_shared
return (num + 1)
$$ LANGUAGE plcontainer;

CREATE OR REPLACE FUNCTION python_function_1(num int) RETURNS int4 AS $$
# container: plc_python_shared
import numpy
a = numpy.mat(numpy.random.randint(200,size=(10,20)))
b = numpy.mat(numpy.random.randint(200,size=(20,10)))
a*b
return num
$$ LANGUAGE plcontainer;

CREATE OR REPLACE FUNCTION python_function_2(num int) RETURNS int4 AS $$
# container: plc_python_shared
for i in range(1, 500):
    a = plpy.execute("select count(*) from NUM_OF_LOOPS where num < 5") 
return num
$$ LANGUAGE plcontainer;

CREATE OR REPLACE FUNCTION python_function_3(num int) RETURNS int4 AS $$
# container: plc_python_shared
a = 0
for i in range(1, 10000):
    a = a + num 
return a
$$ LANGUAGE plcontainer;

CREATE OR REPLACE FUNCTION python_function_4(num int) RETURNS int4 AS $$
# container: plc_python_shared
import time
return num * 2
$$ LANGUAGE plcontainer;

CREATE OR REPLACE FUNCTION python_function_5(num int) RETURNS int4 AS $$
# container: plc_python_shared
return num + 1
$$ LANGUAGE plcontainer;

CREATE TABLE NUM_OF_LOOPS (num int, aux int);
INSERT into NUM_OF_LOOPS (num, aux) select trunc(random() * 120 + 1) as num, aux from generate_series(1,2000) as aux;
