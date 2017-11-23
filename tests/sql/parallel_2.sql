select r_large_spi();
select r_io_intensive();

select py_cpu_intensive();
select py_cpu_intensive() from generate_series(1,2);

select r_cpu_intensive();
select r_cpu_intensive() from generate_series(1,2);

select rlargeint8in(array_agg(id)) from generate_series(1, 10123123) id;
select avg(x) from (select unnest(rlargeint8out(10123123)) as x) as q;

select py_large_spi();
select py_io_intensive();

select pylargeint8in(array_agg(id)) from generate_series(1, 10123123) id;
select avg(x) from (select unnest(pylargeint8out(10123123)) as x) as q;
