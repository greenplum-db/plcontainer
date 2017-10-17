----- Test text, bool, char, int4
set log_min_messages='DEBUG1';
drop table if exists t2;
create table t2 (name text, online bool, sex char, id int4);
insert into t2 values('bob1', true, 'm', 7000);
insert into t2 values('bob1', true, 'm', 8000);
insert into t2 values('bob1', true, 'm', 9000);
insert into t2 values('bob2', true, 'f', 9001);
insert into t2 values('bob3', false, 'm', 9002);
insert into t2 values('alice1', false, 'm', 9003);
insert into t2 values('alice2', false, 'f', 9004);
insert into t2 values('alice3', true, 'm', 9005);

CREATE OR REPLACE FUNCTION py_spi_pexecute1() RETURNS void AS $$
# container: plc_python_shared

plpy.notice("Test query execution")
rv = plpy.execute("select * from t2 where name='bob1' and online=True and sex='m' order by id limit 2")
for r in rv:
    plpy.notice(str(r))

plpy.notice("Test text")
plan = plpy.prepare("select * from t2 where name=$1 order by id", ["text"])
rv = plpy.execute(plan, ["bob1"])
for r in rv:
    plpy.notice(str(r))

plpy.notice("Test bool")
plan = plpy.prepare("select * from t2 where online=$1 order by id", ["boolean"])
rv = plpy.execute(plan, [True])
for r in rv:
    plpy.notice(str(r))

plan1 = plpy.prepare("select * from t2 where sex=$1 order by id", ["char"])
plan2 = plpy.prepare("select * from t2 where id>$1 order by id", ["int4"])
plpy.notice("Test int4")
rv = plpy.execute(plan2, [9001])
for r in rv:
    plpy.notice(str(r))
plpy.notice("Test char")
rv = plpy.execute(plan1, ['m'])
for r in rv:
    plpy.notice(str(r))

plpy.notice("Test text+bool+char+int4")
plan = plpy.prepare("select * from t2 where name=$1 and online=$2 and sex=$3 order by id limit $4", ["text", "bool", "char", "int4"])
rv = plpy.execute(plan, ["bob1", True, 'm', 2]);
for r in rv:
    plpy.notice(str(r))

$$ LANGUAGE plcontainer;

select py_spi_pexecute1();
drop table if exists t2;

----- Test text, int2, int8
drop table if exists t3;
create table t3 (name text, age int2, id int8);
insert into t3 values('bob0', 20, 9000);
insert into t3 values('bob1', 20, 8000);
insert into t3 values('bob2', 30, 7000);
insert into t3 values('alice1', 40, 6000);
insert into t3 values('alice2', 50, 6000);

CREATE OR REPLACE FUNCTION py_spi_pexecute2() RETURNS void AS $$
# container: plc_python_shared

plpy.notice("Test int2")
plan = plpy.prepare("select * from t3 where age=$1 order by id", ["int2"])
rv = plpy.execute(plan, [20]);
for r in rv:
    plpy.notice(str(r))

plpy.notice("Test int8")
plan = plpy.prepare("select * from t3 where id=$1 order by age", ["int8"])
rv = plpy.execute(plan, [6000]);
for r in rv:
    plpy.notice(str(r))

plpy.notice("Test text + int8")
plan = plpy.prepare("select * from t3 where name=$1 and age=$2 order by id", ["text", "int8"])
rv = plpy.execute(plan, ["bob0", 20]);
for r in rv:
    plpy.notice(str(r))

$$ LANGUAGE plcontainer;

select py_spi_pexecute2();
drop table if exists t3;


----- Test bytea, float4, float8
drop table if exists t4;
create table t4 (name bytea, score1 float4, score2 float8);
insert into t4 values('bob0', 8.5, 16.5);
insert into t4 values('bob1', 8.75, 16.75);
insert into t4 values('bob2', 8.875, 16.875);
insert into t4 values('bob3', 9.125, 16.125);
insert into t4 values('bob4', 9.25, 16.25);

CREATE OR REPLACE FUNCTION py_spi_pexecute3() RETURNS void AS $$
# container: plc_python_shared

plan1 = plpy.prepare("select * from t4 where name=$1 order by score1", ["bytea"])
plan2 = plpy.prepare("select * from t4 where score1<$1 order by name", ["float4"])
plan3 = plpy.prepare("select * from t4 where score2<$1 order by name", ["float8"])
plan4 = plpy.prepare("select * from t4 where score1<$1 and score2<$2 order by name", ["float4", "float8"])

plpy.notice("Test bytea");
rv = plpy.execute(plan1, ["bob0"]);
for r in rv:
    plpy.notice(str(r))

plpy.notice("Test float4");
rv = plpy.execute(plan2, [8.9]);
for r in rv:
    plpy.notice(str(r))

plpy.notice("Test float8");
rv = plpy.execute(plan3, [16.8]);
for r in rv:
    plpy.notice(str(r))

plpy.notice("Test float4+float8");
rv = plpy.execute(plan4, [9.18, 16.80]);
for r in rv:
    plpy.notice(str(r))

$$ LANGUAGE plcontainer;

select py_spi_pexecute3();
----- Do not drop table t4 here since it is used below.

----- negative tests
CREATE OR REPLACE FUNCTION py_spi_illegal_pexecute1() RETURNS void AS $$
# container: plc_python_shared

plan1 = plpy.prepare("select * from t4 where name=$1 order by score1", ["integer"])
rv = plpy.execute(plan1, ["bob0"]);
for r in rv:
    plpy.notice(str(r))

$$ LANGUAGE plcontainer;

CREATE OR REPLACE FUNCTION py_spi_illegal_pexecute2() RETURNS void AS $$
# container: plc_python_shared

plan1 = plpy.prepare("select * from t4 where score1<$1 order by score1", ["bytea"])
rv = plpy.execute(plan1, ["bob0"]);
for r in rv:
    plpy.notice(str(r))

$$ LANGUAGE plcontainer;

CREATE OR REPLACE FUNCTION py_spi_illegal_pexecute3() RETURNS void AS $$
# container: plc_python_shared

plan1 = plpy.prepare("select * from t4 where score1<$1 order by score1", ["float4"])
rv = plpy.execute(plan1, ["bob0"]);
for r in rv:
    plpy.notice(str(r))

$$ LANGUAGE plcontainer;

CREATE OR REPLACE FUNCTION py_spi_illegal_pexecute4() RETURNS void AS $$
# container: plc_python_shared

plan1 = plpy.prepare("select * from t4 where score1<$1 and score2<$2 order by score1", ["float4", "float8"])
rv = plpy.execute(plan1, [9.5, "bob0"]);
for r in rv:
    plpy.notice(str(r))

$$ LANGUAGE plcontainer;


CREATE OR REPLACE FUNCTION py_spi_simple_t4() RETURNS void AS $$
# container: plc_python_shared

plan1 = plpy.prepare("select * from t4 where name='bob0' order by score1")
rv = plpy.execute(plan1);
for r in rv:
    plpy.notice(str(r))

$$ LANGUAGE plcontainer;

select py_spi_simple_t4();
select py_spi_illegal_pexecute1();
select py_spi_illegal_pexecute2();
select py_spi_simple_t4();
select py_spi_illegal_pexecute3();
select py_spi_illegal_pexecute4();
select py_spi_simple_t4();

CREATE OR REPLACE FUNCTION pyspi_illegal_sql() RETURNS integer AS $$
# container: plc_python_shared
plpy.execute("select datname from pg_database_invalid");
return 0
$$ LANGUAGE plcontainer;

CREATE OR REPLACE FUNCTION pyspi_illegal_sql_pexecute() RETURNS integer AS $$
# container: plc_python_shared
plan = plpy.prepare("select datname from pg_database_invalid");
plpy.execute(plan);
return 0
$$ LANGUAGE plcontainer;

CREATE OR REPLACE FUNCTION pyspi_bad_limit() RETURNS integer AS $$
# container: plc_python_shared
rv = plpy.execute("select datname from pg_database order by datname", -2);
for r in rv:
    plpy.notice(str(r))
return 0
$$ LANGUAGE plcontainer;

CREATE OR REPLACE FUNCTION pyspi_bad_limit_pexecute() RETURNS integer AS $$
# container: plc_python_shared
plan = plpy.prepare("select datname from pg_database order by datname", -2);
rv = plpy.execute(plan);
for r in rv:
    plpy.notice(str(r))
return 0
$$ LANGUAGE plcontainer;

CREATE OR REPLACE FUNCTION pyspi_bad_limit_stable() RETURNS integer AS $$
# container: plc_python_shared
rv = plpy.execute("select datname from pg_database order by datname", -2);
for r in rv:
    plpy.notice(str(r))
return 0
$$ LANGUAGE plcontainer STABLE;

CREATE OR REPLACE FUNCTION pyspi_bad_limit_immutable() RETURNS integer AS $$
# container: plc_python_shared
rv = plpy.execute("select datname from pg_database order by datname", -2);
for r in rv:
    plpy.notice(str(r))
return 0
$$ LANGUAGE plcontainer IMMUTABLE;

select py_spi_simple_t4();
select pyspi_illegal_sql();
select pyspi_illegal_sql_pexecute();
select pyspi_bad_limit();
select py_spi_simple_t4();
select pyspi_bad_limit_pexecute();
select pyspi_bad_limit_immutable();
select pyspi_bad_limit_stable();
select py_spi_simple_t4();

----- Now drop t4.
drop table t4;
