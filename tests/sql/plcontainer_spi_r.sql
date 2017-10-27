CREATE OR REPLACE FUNCTION load_r_typenames() RETURNS text AS $$
# container: plc_r_shared
sql <- "select upper(typname::text) || 'OID' as typename, oid from pg_catalog.pg_type where typtype = 'b' order by typname"
rs <- pg.spi.exec(sql)
for(i in 1:nrow(rs))
{
  typobj <- rs[i,1]
  typval <- rs[i,2]
  if (substr(typobj,1,1) == "_")
        typobj <- paste("ARRAYOF", substr(typobj,2,nchar(typobj)), sep="")
          assign(typobj, typval, .GlobalEnv)
}
return("OK")
$$ language plcontainer;

SELECT load_r_typenames();

----- Test text, bool, char, int4
DROP TABLE if exists t2r;
create TABLE t2r (name text, online bool, sex char, id int4);
INSERT INTO t2r values('bob1', true, 'm', 7000);
INSERT INTO t2r values('bob1', true, 'm', 8000);
INSERT INTO t2r values('bob1', true, 'm', 9000);
INSERT INTO t2r values('bob2', true, 'f', 9001);
INSERT INTO t2r values('bob3', false, 'm', 9002);
INSERT INTO t2r values('alice1', false, 'm', 9003);
INSERT INTO t2r values('alice2', false, 'f', 9004);
INSERT INTO t2r values('alice3', true, 'm', 9005);

CREATE OR REPLACE FUNCTION r_spi_pexecute1() RETURNS void AS $$
# container: plc_r_shared
plr.notice("Test query execution")
rv <- pg.spi.exec("select * from t2r where name='bob1' and online=True and sex='m' order by id limit 2")
for(i in 1:nrow(rv)){
    plr.notice(rv[i,1])
}

plr.notice("Test text")
plan = pg.spi.prepare("select * from t2r where name=$1 order by id", c(TEXTOID))
rv = pg.spi.execp(plan, list("bob1"))
for(i in 1:nrow(rv)){
    plr.notice(rv[i,1])
}

plr.notice("Test bool")
plan <- pg.spi.prepare("select * from t2r where online=$1 order by id", c(BOOLOID))
rv <- pg.spi.execp(plan, list(TRUE))
for(i in 1:nrow(rv)){
	plr.notice(rv[i,1])
}

plan1 <- pg.spi.prepare("select * from t2r where sex=$1 order by id", c(CHAROID))
plan2 <- pg.spi.prepare("select * from t2r where id>$1 order by id", c(INT4OID))
plr.notice("Test int4")
rv <- pg.spi.execp(plan2, list(9001))

for(i in 1:nrow(rv)){
    plr.notice(rv[i,1])
}

plr.notice("Test char")
rv <- pg.spi.execp(plan1, list('m'))

for(i in 1:nrow(rv)){
    plr.notice(rv[i,1])
}

plr.notice("Test text+bool+char+int4")
plan <- pg.spi.prepare("select * from t2r where name=$1 and online=$2 and sex=$3 order by id limit $4", c(TEXTOID, BOOLOID, CHAROID, INT4OID))
rv <- pg.spi.execp(plan, list("bob1", TRUE, 'm', 2));

for(i in 1:nrow(rv)){
    plr.notice(rv[i,1])
}

$$ LANGUAGE plcontainer;

select r_spi_pexecute1();
drop table if exists t2r;

----- Test text, int2, int8
drop table if exists t3r;
create table t3r (name text, age int2, id int8);
insert into t3r values('bob0', 20, 9000);
insert into t3r values('bob1', 20, 8000);
insert into t3r values('bob2', 30, 7000);
insert into t3r values('alice1', 40, 6000);
insert into t3r values('alice2', 50, 6001);

CREATE OR REPLACE FUNCTION r_spi_pexecute2() RETURNS void AS $$
# container: plc_r_shared

plr.notice("Test int2")
plan <- pg.spi.prepare("select * from t3r where age=$1 order by id", c(INT2OID))
rv <- pg.spi.execp(plan, list(20));

for(i in 1:nrow(rv)){
    plr.notice(rv[i,1])
}

plr.notice("Test int8")
plan <- pg.spi.prepare("select * from t3r where id<$1 order by id", c(INT8OID))
rv <- pg.spi.execp(plan, list(6002));

for(i in 1:nrow(rv)){
    plr.notice(rv[i,1])
}

plr.notice("Test text + int8")
plan <- pg.spi.prepare("select * from t3r where name=$1 and age=$2 order by id", c(TEXTOID, INT8OID))
rv <- pg.spi.execp(plan, list("bob0", 20));

for(i in 1:nrow(rv)){
    plr.notice(rv[i,1])
}

$$ LANGUAGE plcontainer;

select r_spi_pexecute2();
drop table if exists t3r;


----- Test bytea, float4, float8
drop table if exists t4r;
create table t4r (name bytea, score1 float4, score2 float8);
insert into t4r values('bob0', 8.5, 16.5);
insert into t4r values('bob1', 8.75, 16.75);
insert into t4r values('bob2', 8.875, 16.875);
insert into t4r values('bob3', 9.125, 16.125);
insert into t4r values('bob4', 9.25, 16.25);

CREATE OR REPLACE FUNCTION r_spi_pexecute3() RETURNS void AS $$
# container: plc_r_shared

plan1 <- pg.spi.prepare("select * from t4r where name=$1 order by score1", c(BYTEAOID))
plan2 <- pg.spi.prepare("select * from t4r where score1<$1 order by name", c(FLOAT4OID))
plan3 <- pg.spi.prepare("select * from t4r where score2<$1 order by name", c(FLOAT8OID))
plan4 <- pg.spi.prepare("select * from t4r where score1<$1 and score2<$2 order by name", c(FLOAT4OID, FLOAT8OID))

plr.notice("Test bytea");
rv <- pg.spi.execp(plan1, list('bob0'));

plr.notice("Test float4");
rv <- pg.spi.execp(plan2, list(8.9));

for(i in 1:nrow(rv)){
    plr.notice(rv[i,1])
}

plr.notice("Test float8");
rv <- pg.spi.execp(plan3, list(16.8));

for(i in 1:nrow(rv)){
    plr.notice(rv[i,1])
}

plr.notice("Test float4+float8");
rv <- pg.spi.execp(plan4, list(9.18, 16.80));

for(i in 1:nrow(rv)){
    plr.notice(rv[i,1])
}

$$ LANGUAGE plcontainer;

select r_spi_pexecute3();
----- Do not drop table t4 here since it is used below.

----- negative tests
CREATE OR REPLACE FUNCTION r_spi_illegal_pexecute1() RETURNS void AS $$
# container: plc_r_shared

plan1 <- pg.spi.prepare("select * from t4r where name=$1 order by score1", c(INT4OID))
rv <- pg.spi.execp(plan1, list("bob0"));

for(i in 1:nrow(rv)){
    plr.notice(rv[i,1])
}

$$ LANGUAGE plcontainer;

CREATE OR REPLACE FUNCTION r_spi_illegal_pexecute2() RETURNS void AS $$
# container: plc_r_shared

plan1 <- pg.spi.prepare("select * from t4r where score1<$1 order by score1", c(BYTEAOID))
rv <- pg.spi.execp(plan1, list("bob0"));

for(i in 1:nrow(rv)){
    plr.notice(rv[i,1])
}

$$ LANGUAGE plcontainer;

CREATE OR REPLACE FUNCTION r_spi_illegal_pexecute3() RETURNS void AS $$
# container: plc_r_shared

plan1 <- pg.spi.prepare("select * from t4r where score1<$1 order by score1", c(FLOAT4OID))
rv <- pg.spi.execp(plan1, list("bob0"));

for(i in 1:nrow(rv)){
    plr.notice(rv[i,1])
}

$$ LANGUAGE plcontainer;

CREATE OR REPLACE FUNCTION r_spi_illegal_pexecute4() RETURNS void AS $$
# container: plc_r_shared

plan1 <- pg.spi.prepare("select * from t4r where score1<$1 and score2<$2 order by score1", c(FLOAT4OID, FLOAT8OID))
rv <- pg.spi.execp(plan1, list(9.5, "bob0"));

for(i in 1:nrow(rv)){
    plr.notice(rv[i,1])
}

$$ LANGUAGE plcontainer;


CREATE OR REPLACE FUNCTION r_spi_simple_t4() RETURNS void AS $$
# container: plc_r_shared

plan1 <- pg.spi.prepare("select * from t4r where name='bob0' order by score1")
rv <- pg.spi.execp(plan1);

for(i in 1:nrow(rv)){
    plr.notice(rv[i,1])
}

$$ LANGUAGE plcontainer;

select r_spi_simple_t4();
select r_spi_illegal_pexecute1();
select r_spi_illegal_pexecute2();
select r_spi_simple_t4();
select r_spi_illegal_pexecute3();
select r_spi_illegal_pexecute4();
select r_spi_simple_t4();

CREATE OR REPLACE FUNCTION rspi_illegal_sql() RETURNS integer AS $$
# container: plc_r_shared
rs <- pg.spi.exec("select datname from pg_database_invalid");
return (0)
$$ LANGUAGE plcontainer;

CREATE OR REPLACE FUNCTION rspi_illegal_sql_pexecute() RETURNS integer AS $$
# container: plc_r_shared
plan <- pg.spi.prepare("select datname from pg_database_invalid");
rv <- pg.spi.execp(plan);
return (0)
$$ LANGUAGE plcontainer;


select r_spi_simple_t4();
select rspi_illegal_sql();
select rspi_illegal_sql_pexecute();
select r_spi_simple_t4();


----- Now drop t4.
drop table t4r;

-- test SPI update, insert and delete

drop table if exists t5r;
create table t5r (name text, score1 float4, score2 float8);
insert into t5r values('bob0', 8.5, 16.5);
insert into t5r values('bob1', 8.75, 16.75);

CREATE OR REPLACE FUNCTION rspi_insert_exec() RETURNS integer AS $$
# container: plc_r_shared
rv <- pg.spi.exec("insert into t5r values('bob2', 8.5, 16.5)");
plr.notice(rv)
return (0)
$$ LANGUAGE plcontainer;

CREATE OR REPLACE FUNCTION rspi_update_exec() RETURNS integer AS $$
# container: plc_r_shared
rv <- pg.spi.exec("update t5r set score1=11 where name='bob2'");
plr.notice(rv)
return (0)
$$ LANGUAGE plcontainer;

CREATE OR REPLACE FUNCTION rspi_delete_exec() RETURNS integer AS $$
# container: plc_r_shared
rv <- pg.spi.exec("delete from t5r where name='bob2'");
plr.notice(rv)
return (0)
$$ LANGUAGE plcontainer;

SELECT rspi_insert_exec();
SELECT * FROM t5r order by name;
SELECT rspi_update_exec();
SELECT * FROM t5r order by name;
SELECT rspi_delete_exec();
SELECT * FROM t5r order by name;

CREATE OR REPLACE FUNCTION rspi_insert_execp() RETURNS integer AS $$
# container: plc_r_shared
plan <- pg.spi.prepare("insert into t5r values('bob3', 8.5, 16.5)");
rv <- pg.spi.execp(plan);
plr.notice(rv)
return (0)
$$ LANGUAGE plcontainer;

CREATE OR REPLACE FUNCTION rspi_update_execp() RETURNS integer AS $$
# container: plc_r_shared
plan <- pg.spi.prepare("update t5r set score1=12 where name='bob3'");
rv <- pg.spi.execp(plan);
plr.notice(rv)
return (0)
$$ LANGUAGE plcontainer;

CREATE OR REPLACE FUNCTION rspi_delete_execp() RETURNS integer AS $$
# container: plc_r_shared
plan <- pg.spi.prepare("delete from t5r where name='bob3'");
rv <- pg.spi.execp(plan);
plr.notice(rv)
return (0)
$$ LANGUAGE plcontainer;


SELECT rspi_insert_execp();
SELECT * FROM t5r order by name;
SELECT rspi_update_execp();
SELECT * FROM t5r order by name;
SELECT rspi_delete_execp();
SELECT * FROM t5r order by name;

insert into t5r values(null, 18.75, 26.75);
insert into t5r values(null, 28.75, 26.75);
insert into t5r values(null, 38.75, 26.75);

SELECT load_r_typenames();
CREATE OR REPLACE FUNCTION rspi_select_null_execp() RETURNS integer AS $$
# container: plc_r_shared
plan <- pg.spi.prepare("select * from t5r order by name");
rv <- pg.spi.execp(plan);
for(i in 1:nrow(rv)){
    plr.notice(rv[i,1])
}
plan1 <- pg.spi.prepare("insert into t5r values($1, 48.75, 26.75)", c(TEXTOID));
rv <- pg.spi.execp(plan1, list(NULL));
plan2 <- pg.spi.prepare("select * from t5r order by name");
rv <- pg.spi.execp(plan2);
for(i in 1:nrow(rv)){
    plr.notice(rv[i,1])
}
return (0)
$$ LANGUAGE plcontainer;

SELECT rspi_select_null_execp();
SELECT * FROM t5r order by score1;
DROP TABLE t5r;
