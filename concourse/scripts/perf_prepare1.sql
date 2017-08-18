drop table if exists a;
create table a(i int);
insert into a select generate_series(1,1000);

drop table if exists a4;
create table a4(i int);
insert into a4 select generate_series(1,10000);

drop table if exists a5;
create table a5(i int);
insert into a5 select generate_series(1,100000);
