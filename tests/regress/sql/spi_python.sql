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
insert into t2 values(null, true, 'm', 1000);
insert into t2 values(null, true, 'm', 1001);

CREATE OR REPLACE FUNCTION py_spi_pexecute1() RETURNS void AS $$
# container: plc_python_shared

plpy.notice("Test query execution")
rv = plpy.execute("select * from t2 where name='bob1' and online=True and sex='m' order by id limit 2")
for r in rv:
    plpy.notice(str(r))

plpy.notice("Test query targetlist containing NULL")
rv = plpy.execute("select * from t2 where id = 1000")
for r in rv:
    plpy.notice(str(r))

plpy.notice("Test text")
plan = plpy.prepare("select * from t2 where name=$1 order by id", ["text"])
plpy.notice(plan.status())
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

plpy.notice("Test results containing NULL")
plan = plpy.prepare("select * from t2 where name is null limit $1", ["int4"])
rv = plpy.execute(plan, [2]);
for r in rv:
    plpy.notice(str(r))

plpy.notice("Test insert NULL")
plan = plpy.prepare("insert into t2 values($1, true, 'm', 1002)", ["text"])
rv = plpy.execute(plan, [None]);
for r in rv:
    plpy.notice(str(r))
rv = plpy.execute("select * from t2 order by id");
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

CREATE OR REPLACE FUNCTION py_spi_simple_num() RETURNS void AS $$
# container: plc_python_shared

plan1 = plpy.prepare("select * from t4 where score1<$1 order by score1", ["numeric"])
rv = plpy.execute(plan1, [10.5]);
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
SELECT py_spi_simple_num();

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

-- test SPI update, insert and delete

drop table if exists t5;
create table t5 (name text, score1 float4, score2 float8);
insert into t5 values('bob0', 8.5, 16.5);
insert into t5 values('bob1', 8.75, 16.75);

-- execute
CREATE OR REPLACE FUNCTION pyspi_insert_exec() RETURNS integer AS $$
# container: plc_python_shared
rv = plpy.execute("insert into t5 values('bob2', 8.5, 16.5)");
for r in rv:
	plpy.notice(str(r))
return 0
$$ LANGUAGE plcontainer;

CREATE OR REPLACE FUNCTION pyspi_update_exec() RETURNS integer AS $$
# container: plc_python_shared
rv = plpy.execute("update t5 set score1=11 where name='bob2'");
for r in rv:
	plpy.notice(str(r))
return 0
$$ LANGUAGE plcontainer;

CREATE OR REPLACE FUNCTION pyspi_delete_exec() RETURNS integer AS $$
# container: plc_python_shared
rv = plpy.execute("delete from t5 where name='bob2'");
for r in rv:
	plpy.notice(str(r))
return 0
$$ LANGUAGE plcontainer;

SELECT pyspi_insert_exec();
SELECT * FROM t5 order by name;
SELECT pyspi_update_exec();
SELECT * FROM t5 order by name;
SELECT pyspi_delete_exec();
SELECT * FROM t5 order by name;

-- executep
CREATE OR REPLACE FUNCTION pyspi_insert_execp() RETURNS integer AS $$
# container: plc_python_shared
plan = plpy.prepare("insert into t5 values('bob3', 8.5, 16.5)");
rv = plpy.execute(plan);
for r in rv:
	plpy.notice(str(r))
return 0
$$ LANGUAGE plcontainer;

CREATE OR REPLACE FUNCTION pyspi_update_execp() RETURNS integer AS $$
# container: plc_python_shared
plan = plpy.prepare("update t5 set score1=12 where name='bob3'");
rv = plpy.execute(plan);
for r in rv:
	plpy.notice(str(r))
return 0
$$ LANGUAGE plcontainer;

CREATE OR REPLACE FUNCTION pyspi_delete_execp() RETURNS integer AS $$
# container: plc_python_shared
plan = plpy.prepare("delete from t5 where name='bob3'");
rv = plpy.execute(plan);
for r in rv:
	plpy.notice(str(r))
return 0
$$ LANGUAGE plcontainer;

CREATE OR REPLACE FUNCTION pyspi_select_notexist_execp() RETURNS integer AS $$
# container: plc_python_shared
plan = plpy.prepare("select * from t5 where name='bob_notexist'");
rv = plpy.execute(plan);
for r in rv:
	plpy.notice(str(r))
return 0
$$ LANGUAGE plcontainer;

CREATE OR REPLACE FUNCTION pyspi_delete_notexist_execp() RETURNS integer AS $$
# container: plc_python_shared
plan = plpy.prepare("delete from t5 where name='bob3_notexist'");
rv = plpy.execute(plan);
for r in rv:
	plpy.notice(str(r))
return 0
$$ LANGUAGE plcontainer;


SELECT pyspi_insert_execp();
SELECT * FROM t5 order by name;
SELECT pyspi_update_execp();
SELECT * FROM t5 order by name;
SELECT pyspi_delete_execp();
SELECT * FROM t5 order by name;

-- Test returns 0 row X N cols.
select pyspi_select_notexist_execp();
-- Test returns 0 row X 0 col.
select pyspi_delete_notexist_execp();
-- insert returns N rows * 0 col, so there is no need of additional case.

DROP TABLE t5;

-- Test for Exception
CREATE OR REPLACE FUNCTION pyspi_exec_exception() RETURNS integer AS $$
# container: plc_python_shared
plan = list();
try:
	rv = plpy.execute(plan);
except Exception as e:
	plpy.notice(type(e))
return 0
$$ LANGUAGE plcontainer;

SELECT pyspi_exec_exception();

-- FIXME: type conversion could not caught by expection yet
--        due to the PG_CATCH() is not passed to client.

-- We will update on fname, so we can't have it as a distribution key,
-- which in turn means it can't be part of a primary key

CREATE FUNCTION invalid_type_uncaught(a text) RETURNS text AS $$
# container: plc_python_shared
if not SD.has_key("plan"):
	q = "SELECT fname FROM users WHERE lname = $1"
	SD["plan"] = plpy.prepare(q, [ "test" ])
rv = plpy.execute(SD["plan"], [ a ])
if len(rv):
	return rv[0]["fname"]
return None
$$ LANGUAGE plcontainer;

-- for what it's worth catch the exception generated by
-- the typo, and return None
CREATE FUNCTION invalid_type_caught(a text) RETURNS text AS $$
# container: plc_python_shared
if not SD.has_key("plan"):
	q = "SELECT fname FROM users WHERE lname = $1"
	try:
		SD["plan"] = plpy.prepare(q, [ "test" ])
	except plpy.SPIError, ex:
		plpy.notice(str(ex))
		return None
rv = plpy.execute(SD["plan"], [ a ])
if len(rv):
	return rv[0]["fname"]
return None
$$ LANGUAGE plcontainer;

-- for what it's worth catch the exception generated by
-- the typo, and reraise it as a plain error

CREATE FUNCTION invalid_type_reraised(a text) RETURNS text AS $$
# container: plc_python_shared
if not SD.has_key("plan"):
	q = "SELECT fname FROM users WHERE lname = $1"
	try:
		SD["plan"] = plpy.prepare(q, [ "test" ])
	except plpy.SPIError, ex:
		plpy.error(str(ex))
rv = plpy.execute(SD["plan"], [ a ])
if len(rv):
	return rv[0]["fname"]
return None
$$ LANGUAGE plcontainer;


-- no typo no messing about

CREATE FUNCTION valid_type(a text) RETURNS text AS $$
# container: plc_python_shared
if not SD.has_key("plan"):
	SD["plan"] = plpy.prepare("SELECT fname FROM users WHERE lname = $1", [ "text" ])
rv = plpy.execute(SD["plan"], [ a ])
if len(rv):
	return rv[0]["fname"]
return None
$$ LANGUAGE plcontainer;

CREATE FUNCTION nested_call_one(a text) RETURNS text AS $$
# container: plc_python_shared
q = "SELECT nested_call_two('%s')" % a
r = plpy.execute(q)
return r[0]
$$ LANGUAGE plcontainer ;

CREATE FUNCTION nested_call_two(a text) RETURNS text AS $$
# container: plc_python_shared
q = "SELECT nested_call_three('%s')" % a
r = plpy.execute(q)
return r[0]
$$ LANGUAGE plcontainer ;

CREATE FUNCTION nested_call_three(a text) RETURNS text AS $$
# container: plc_python_shared
return a
$$ LANGUAGE plcontainer ;

-- some spi stuff

CREATE FUNCTION spi_prepared_plan_test_one(a text) RETURNS text AS $$
# container: plc_python_shared
if not SD.has_key("myplan"):
	q = "SELECT count(*) FROM users WHERE lname = $1"
	SD["myplan"] = plpy.prepare(q, [ "text" ])
try:
	rv = plpy.execute(SD["myplan"], [a])
	return "there are " + str(rv[0]["count"]) + " " + str(a) + "s"
except Exception, ex:
	plpy.error(str(ex))
return None
$$ LANGUAGE plcontainer;

CREATE FUNCTION spi_prepared_plan_test_nested(a text) RETURNS text AS $$
# container: plc_python_shared
if not SD.has_key("myplan"):
	q = "SELECT spi_prepared_plan_test_one('%s') as count" % a
	SD["myplan"] = plpy.prepare(q)
try:
	rv = plpy.execute(SD["myplan"])
	if len(rv):
		return rv[0]["count"]
except Exception, ex:
	plpy.error(str(ex))
return None
$$ LANGUAGE plcontainer;

SELECT invalid_type_uncaught('rick');
SELECT invalid_type_caught('rick');
SELECT invalid_type_reraised('rick');
SELECT valid_type('rick');

select nested_call_one('pass this along');
select spi_prepared_plan_test_one('doe');
select spi_prepared_plan_test_one('smith');
select spi_prepared_plan_test_nested('smith');

CREATE FUNCTION result_nrows_test() RETURNS int
AS $$
# container: plc_python_shared
plan = plpy.prepare("SELECT 1 UNION SELECT 2")
plpy.info(plan.status())
result = plpy.execute(plan)
if result.status() > 0:
   return result.nrows()
else:
   return None
$$ LANGUAGE plcontainer;

select result_nrows_test();
