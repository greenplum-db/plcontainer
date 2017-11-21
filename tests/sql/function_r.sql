CREATE OR REPLACE FUNCTION rshouldnotparse() returns text as $$
# container: plc_r_shared
return 'hello'
$$ LANGUAGE plcontainer;

CREATE OR REPLACE FUNCTION rlog100() RETURNS text AS $$
# container: plc_r_shared
return(log10(100))
$$ LANGUAGE plcontainer;

CREATE OR REPLACE FUNCTION rbool(b bool) RETURNS bool AS $$
# container: plc_r_shared
return (b)
$$ LANGUAGE plcontainer;

CREATE OR REPLACE FUNCTION rint(i int2) RETURNS int2 AS $$
# container: plc_r_shared
return (i+1)
$$ LANGUAGE plcontainer;

CREATE OR REPLACE FUNCTION rint(i int4) RETURNS int4 AS $$
# container: plc_r_shared
return (i+2)
$$ LANGUAGE plcontainer;

CREATE OR REPLACE FUNCTION rint(i int8) RETURNS int8 AS $$
# container: plc_r_shared
return (i+3)
$$ LANGUAGE plcontainer;

CREATE OR REPLACE FUNCTION rfloat(f float4) RETURNS float4 AS $$
# container: plc_r_shared
return (f+1)
$$ LANGUAGE plcontainer;

CREATE OR REPLACE FUNCTION rfloat(f float8) RETURNS float8 AS $$
# container: plc_r_shared
return (f+2)
$$ LANGUAGE plcontainer;

CREATE OR REPLACE FUNCTION rnumeric(n numeric) RETURNS numeric AS $$
# container: plc_r_shared
return (n+3.0)
$$ LANGUAGE plcontainer;

CREATE OR REPLACE FUNCTION rtimestamp(t timestamp) RETURNS timestamp AS $$
# container: plc_r_shared
options(digits.secs = 6)
tmp <- strptime(t,'%Y-%m-%d %H:%M:%OS')
return (as.character(tmp + 3600))
$$ LANGUAGE plcontainer;

CREATE OR REPLACE FUNCTION rtimestamptz(t timestamptz) RETURNS timestamptz AS $$
# container: plc_r_shared
t
$$ LANGUAGE plcontainer;

CREATE OR REPLACE FUNCTION rtext(arg varchar) RETURNS varchar AS $$
# container: plc_r_shared
return(paste(arg,'foo',sep=''))
$$ LANGUAGE plcontainer;

create or replace function rbyteaout(arg int[]) returns bytea as $$
# container: plc_r_shared
return (arg)
$$ language plcontainer;

create or replace function rbyteain(arg bytea) returns int[] as $$
# container: plc_r_shared
return (arg)
$$ language plcontainer;

create or replace function rtest_mia() returns int[] as $$
#container:plc_r_shared
as.matrix(array(1:10,c(2,5)))
$$ language plcontainer;

create or replace function vec(arg1 float8[]) returns float8[] as
$$
# container: plc_r_shared
arg1+1
$$ language 'plcontainer';

create or replace function vec(arg1 float4[]) returns float4[] as
$$
# container: plc_r_shared
arg1+1
$$ language 'plcontainer';

create or replace function vec(arg1 int8[]) returns int8[] as
$$
# container: plc_r_shared
arg1+1
$$ language 'plcontainer';

create or replace function vec(arg1 int4[]) returns int4[] as
$$
# container: plc_r_shared
as.integer(arg1+1)
$$ language 'plcontainer';

create or replace function vec(arg1 numeric[]) returns numeric[] as
$$
# container: plc_r_shared
arg1+1
$$ language 'plcontainer';

CREATE OR REPLACE FUNCTION rintarr(arr int8[]) RETURNS int8 AS $$
# container: plc_r_shared
return (sum(arr, na.rm=TRUE))
$$ LANGUAGE plcontainer;

CREATE OR REPLACE FUNCTION rintarr(arr int4[]) RETURNS int4 AS $$
# container: plc_r_shared
return (sum(arr, na.rm=TRUE))
$$ LANGUAGE plcontainer;

CREATE OR REPLACE FUNCTION rintarr(arr int2[]) RETURNS int2 AS $$
# container: plc_r_shared
return (sum(arr, na.rm=TRUE))
$$ LANGUAGE plcontainer;

CREATE OR REPLACE FUNCTION rfloatarr(arr float8[]) RETURNS float8 AS $$
# container: plc_r_shared
return (sum(arr, na.rm=TRUE))
$$ LANGUAGE plcontainer;

CREATE OR REPLACE FUNCTION rfloatarr(arr float4[]) RETURNS float4 AS $$
# container: plc_r_shared
return (sum(arr, na.rm=TRUE))
$$ LANGUAGE plcontainer;

CREATE OR REPLACE FUNCTION rfloatarr(arr numeric[]) RETURNS numeric AS $$
# container: plc_r_shared
return (sum(arr, na.rm=TRUE))
$$ LANGUAGE plcontainer;

CREATE OR REPLACE FUNCTION rboolarr(arr boolean[]) RETURNS int AS $$
# container: plc_r_shared
return (sum(arr, na.rm=TRUE))
$$ LANGUAGE plcontainer;

CREATE OR REPLACE FUNCTION rtimestamparr(arr timestamp[]) RETURNS timestamp[] AS $$
# container: plc_r_shared
options(digits.secs = 6)
tmp <- strptime(arr,'%Y-%m-%d %H:%M:%OS')
return (as.character(tmp+3600))
$$
language plcontainer;

CREATE OR REPLACE FUNCTION rdimarr(arr int2[]) RETURNS int[] AS $$
# container: plc_r_shared
return (dim(arr))
$$ LANGUAGE plcontainer;

CREATE OR REPLACE FUNCTION rdimarr(arr int4[]) RETURNS int[] AS $$
# container: plc_r_shared
return (dim(arr))
$$ LANGUAGE plcontainer;

CREATE OR REPLACE FUNCTION rdimarr(arr int8[]) RETURNS int[] AS $$
# container: plc_r_shared
return (dim(arr))
$$ LANGUAGE plcontainer;
CREATE OR REPLACE FUNCTION rdimarr(arr float4[]) RETURNS int[] AS $$
# container: plc_r_shared
return (dim(arr))
$$ LANGUAGE plcontainer;
CREATE OR REPLACE FUNCTION rdimarr(arr float8[]) RETURNS int[] AS $$
# container: plc_r_shared
return (dim(arr))
$$ LANGUAGE plcontainer;
CREATE OR REPLACE FUNCTION rdimarr(arr numeric[]) RETURNS int[] AS $$
# container: plc_r_shared
return (dim(arr))
$$ LANGUAGE plcontainer;

CREATE OR REPLACE FUNCTION rdimarr(arr boolean[]) RETURNS int[] AS $$
# container: plc_r_shared
return (dim(arr))
$$ LANGUAGE plcontainer;

create or replace function paster(arg1 text[], arg2 text[], arg3 text) returns text[] as
$$
#container: plc_r_shared
paste(arg1, arg2, sep = arg3)
$$
language plcontainer;

CREATE OR REPLACE FUNCTION rlog100_shared() RETURNS text AS $$
# container: plc_r_shared
return(log10(100))
$$ LANGUAGE plcontainer;

create or replace function rpg_spi_exec(arg1 text) returns text as $$
#container: plc_r_shared
(pg.spi.exec(arg1))[[1]]
$$ language plcontainer;

CREATE OR REPLACE FUNCTION rconcatall() RETURNS text AS $$
# container: plc_r_shared
res = pg.spi.exec('select fname from users order by 1')
paste(res, sep=',', collapse=',')
$$ LANGUAGE plcontainer;

CREATE OR REPLACE FUNCTION rnested_call_one(a text) RETURNS text AS $$
# container: plc_r_shared
q = "SELECT rnested_call_two('%s')"
q = gsub("%s",a,q)
r = pg.spi.exec(q)
return (r[1,1])
$$ LANGUAGE plcontainer ;

CREATE OR REPLACE FUNCTION rnested_call_two(a text) RETURNS text AS $$
# container: plc_r_shared
q = "SELECT rnested_call_three('%s') "
q = gsub("%s",a,q)
r = pg.spi.exec(q)
return (r[1,1])
$$ LANGUAGE plcontainer ;

CREATE OR REPLACE FUNCTION rnested_call_three(a text) RETURNS text AS $$
# container: plc_r_shared
return (a)
$$ LANGUAGE plcontainer ;

CREATE OR REPLACE FUNCTION rlogging() RETURNS void AS $$
# container: plc_r_shared
plr.debug('this is the debug message')
plr.log('this is the log message')
plr.info('this is the info message')
plr.notice('this is the notice message')
plr.warning('this is the warning message')
plr.error('this is the error message')
$$ LANGUAGE plcontainer;

CREATE OR REPLACE FUNCTION rlogging2() RETURNS void AS $$
# container: plc_r_shared
pg.spi.exec('select rlogging()');
$$ LANGUAGE plcontainer;

CREATE OR REPLACE FUNCTION runargs1(varchar) RETURNS text AS $$
# container: plc_r_shared
return (args[0])
$$ LANGUAGE plcontainer;

-- create type for next function
create type user_type as (fname text, lname text, username text);

create or replace function rtest_spi_tup(arg1 text) returns setof user_type as $$
#container: plc_r_shared
pg.spi.exec(arg1)
$$ language plcontainer;

create or replace function rtest_spi_ta(arg1 text) returns setof record as $$
#container: plc_r_shared
pg.spi.exec(arg1)
$$ language plcontainer;

CREATE OR REPLACE FUNCTION rsetofint4() RETURNS setof int4 AS $$
# container: plc_r_shared
as.vector(array(1:15,c(5,3)))
$$ LANGUAGE plcontainer ;

CREATE OR REPLACE FUNCTION rsetofbool() RETURNS setof boolean AS $$
# container: plc_r_shared
as.vector(c(TRUE,FALSE,TRUE,TRUE,FALSE,FALSE))
$$ LANGUAGE plcontainer ;

CREATE OR REPLACE FUNCTION rsetofint8() RETURNS setof int8 AS $$
# container: plc_r_shared
as.vector(array(2:16,c(5,3)))
$$ LANGUAGE plcontainer ;

CREATE OR REPLACE FUNCTION rsetofint2() RETURNS setof int2 AS $$
# container: plc_r_shared
as.vector(array(3:17,c(5,3)))
$$ LANGUAGE plcontainer ;
CREATE OR REPLACE FUNCTION rsetoffloat4() RETURNS setof float4 AS $$
# container: plc_r_shared
as.vector(array(2.5:15,c(5,3)))
$$ LANGUAGE plcontainer ;

CREATE OR REPLACE FUNCTION rsetoffloat8() RETURNS setof float8 AS $$
# container: plc_r_shared
as.vector(array(4.5:15,c(5,3)))
$$ LANGUAGE plcontainer ;

CREATE OR REPLACE FUNCTION rsetoffloat8array() RETURNS setof float8[] AS $$
# container: plc_r_shared
matrix(c(array(5.5:15),array(3.5:15)),2)
$$ LANGUAGE plcontainer ;

CREATE OR REPLACE FUNCTION rsetofint8array() RETURNS setof int8[] AS $$
# container: plc_r_shared
matrix(c(array(5:15),array(3:15)),2)
$$ LANGUAGE plcontainer ;

CREATE OR REPLACE FUNCTION rsetofint4array() RETURNS setof int4[] AS $$
# container: plc_r_shared
matrix(c(array(5:15),array(3:15)),2)
$$ LANGUAGE plcontainer ;

CREATE OR REPLACE FUNCTION rsetoftext() RETURNS setof text AS $$
# container: plc_r_shared
as.vector(c("like", "dislike", "hate", "like", "don't know", "like", "dislike"))
$$ LANGUAGE plcontainer ;

CREATE OR REPLACE FUNCTION rsetoftextarray() RETURNS setof text[] AS $$
# container: plc_r_shared
as.matrix(c("like", "dislike", "hate", "like","don't know", "like", "dislike","seven"),2)
$$ LANGUAGE plcontainer ;

CREATE OR REPLACE FUNCTION runargs1(varchar) RETURNS text AS $$
# container: plc_r_shared
return(args[[1]])
$$ LANGUAGE plcontainer;

CREATE OR REPLACE FUNCTION runargs2(a int, varchar) RETURNS text AS $$
# container: plc_r_shared
return(paste(a, args[[2]], sep=""))
$$ LANGUAGE plcontainer;

CREATE OR REPLACE FUNCTION runargs3(a int, varchar, c varchar) RETURNS text AS $$
# container: plc_r_shared
return(paste(a, args[[2]], c, sep=""))
$$ LANGUAGE plcontainer;

CREATE OR REPLACE FUNCTION runargs4(int, int, int, int) RETURNS int AS $$
# container: plc_r_shared
return(length(args))
$$ LANGUAGE plcontainer;

CREATE OR REPLACE FUNCTION rtestudt1(r test_type) RETURNS int AS $$
# container: plc_r_shared
if ( (r[1,1] != TRUE) || (typeof(r[1,1]) != 'logical') ) return (2)
if ( (r['1','a'] != TRUE) || (typeof(r['1','a']) != 'logical') ) return (2)

if ( (r[1,2] != 1) || (typeof(r[1,2]) != 'integer') ) return (3)
if ( (r['1','b'] != 1) || (typeof(r['1','b']) != 'integer') ) return (3)

if ( (r[1,3] != 2) || (typeof(r[1,3]) != 'integer') ) return (4)
if ( (r['1','c'] != 2) || (typeof(r['1','c']) != 'integer') ) return (4)

if ( (r[1,4] != 3) || (typeof(r[1,4]) != 'double') ) return (5)
if ( (r['1','d'] != 3) || (typeof(r['1','d']) != 'double') ) return (5)

if ( (r[1,5] != 4.0) || (typeof(r[1,5]) != 'double') ) return (6)
if ( (r['1','e'] != 4.0) || (typeof(r['1','e']) != 'double') ) return (6)

if ( (r[1,6] != 5.0) || (typeof(r[1,6]) != 'double') ) return (7)
if ( (r['1','f'] != 5.0) || (typeof(r['1','f']) != 'double') ) return (7)

if ( (r[1,7] != 6.0) || (typeof(r[1,7]) != 'double') ) return (8)
if ( (r['1','g'] != 6.0) || (typeof(r['1','g']) != 'double') ) return (8)
if ( (r[1,8] != 'foobar') || (typeof(r[1,8]) != 'character') ) return (9)
if ( (r['1','h'] != 'foobar') || (typeof(r['1','h']) != 'character') ) return (9)
return (10)
$$ LANGUAGE plcontainer;

CREATE OR REPLACE FUNCTION rtestudt2(r test_type2) RETURNS int AS $$
# container: plc_r_shared
if ( (length(r[,1]) != 3) || (r[,1] != c(1,0,1)) || (typeof(r[1,1]) != 'logical') ) return(2) 
if ( (length(r[,2]) != 3) || (r[,2] != c(1,2,3)) || (typeof(r[1,2]) != 'integer') ) return(3)
if ( (length(r[,3]) != 3) || (r[,3] != c(2,3,4)) || (typeof(r[1,3]) != 'integer') ) return(4)
if ( (length(r[,4]) != 3) || (r[,4] != c(3,4,5)) || (typeof(r[1,4]) != 'double') ) return(5)
if ( (length(r[,5]) != 3) || (r[,5] != c(4.5,5.5,6.5)) || (typeof(r[1,5]) != 'double') ) return(6)
if ( (length(r[,6]) != 3) || (r[,6] != c(5.5,6.5,7.5)) || (typeof(r[1,6]) != 'double') ) return(7)
if ( (length(r[,7]) != 3) || (r[,7] != c(6.5,7.5,8.5)) || (typeof(r[1,7]) != 'double') ) return(8)
if ( (length(r[,8]) != 3) || (r[,8] != c('a','b','c')) || (typeof(r[1,8]) != 'character') ) return(9)
return(10)
$$ LANGUAGE plcontainer;

CREATE OR REPLACE FUNCTION rtestudt6a() RETURNS test_type AS $$
# container: plc_r_shared
a=TRUE
b=as.integer(1)
c=as.integer(2)
d=3
e=4
f=4
g=6
h='foo'
x<-data.frame(a,b,c,d,e,f,g,h)
$$ LANGUAGE plcontainer;

CREATE OR REPLACE FUNCTION rtestudt6b() RETURNS setof test_type AS $$
# container: plc_r_shared
a=c(TRUE,FALSE,TRUE)
b=as.integer(c(1,2,3))
c=as.integer(c(5,6,7))
d=c(8,9,10)
e=c(1.0, 2.0, 3.0)
f=c(4.0, 5.0, 6.0)
g=c(7,8,9)
h=c('foo','bar','zzz')
x<-data.frame(a,b,c,d,e,f,g,h)
return(x)
$$ LANGUAGE plcontainer;

CREATE OR REPLACE FUNCTION rtestudt8() RETURNS setof test_type3 AS $$
# container: plc_r_shared
a=1
b=2
c='foo'
a1=data.frame(a,b,c)
a=3
b=4
c='bar'
a2=data.frame(a,b,c)
v=list()
v[[1]]=a1
v[[2]]=a2
return (v)
$$ LANGUAGE plcontainer;

CREATE OR REPLACE FUNCTION rtestudt11() RETURNS setof test_type4 AS $$
# container: plc_r_shared
v=list()
v[[1]] = data.frame(1,c(2,22),c('foo','foo2'))
v[[2]] = data.frame(3,c(4,44),c('bar','bar2'))
return(v)
$$ LANGUAGE plcontainer;

CREATE OR REPLACE FUNCTION rtestudt13(r test_type3) RETURNS test_type3 AS $$
# container: plc_r_shared
return(r)
$$ LANGUAGE plcontainer;

CREATE OR REPLACE FUNCTION rtestudtrecord1() RETURNS record AS $$
# container: plc_r_shared
data.frame(1,  2,  'foo' )
$$ LANGUAGE plcontainer;

CREATE OR REPLACE FUNCTION rtestudtrecord2() RETURNS SETOF record AS $$
# container: plc_r_shared
l=list()
l[[1]] = data.frame(1,  2,  'foo' )
l[[2]] = data.frame(3,  4,  'bar' )
return (l)
$$ LANGUAGE plcontainer;

CREATE OR REPLACE FUNCTION rversion() RETURNS varchar AS $$
# container : plc_r_shared
return(paste("R", getRversion()))
$$ LANGUAGE plcontainer;

CREATE OR REPLACE FUNCTION r_shared_path_perm() RETURNS setof bool[] AS $$
# container : plc_r_shared
v=list()
v[[1]] = file.create("/tmp/plcontainer/test_file")
v[[2]] = file.create("/tmp/test_file")
return(v)
$$ LANGUAGE plcontainer;
