
select py_cpu_intensive();
select py_cpu_intensive() from generate_series(1,2);



select py_large_spi();
select py_io_intensive();

select pylargeint8in(array_agg(id)) from generate_series(1, 1123123) id;
select round(avg(x)) from (select unnest(pylargeint8out(1123123)) as x) as q;
