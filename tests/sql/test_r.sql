set datestyle='ISO,MDY';
select rshouldnotparse();
select rlog100();
select rbool('t');
select rbool('f');
select rint(NULL::int2);
select rint(123::int2);
select rint(234::int4);
select rint(345::int8);
select rfloat(3.1415926535897932384626433832::float4);
select rfloat(3.1415926535897932384626433832::float8);
select rnumeric(3.1415926535897932384626433832::numeric);
select rtimestamp('2012-01-02 12:34:56.789012'::timestamp);
select rtimestamptz('2012-01-02 12:34:56.789012 UTC+4'::timestamptz);
select rtext('123');
select rbyteain(rbyteaout(array[123,1,7]::int[]));
select rbyteain(rbyteaout(array[123,null,7]::int[]));
select rtest_mia();
select vec('{1.23, 1.32}'::float8[]);
select vec('{1, 5,10, 100, 7}'::int8[]);
select vec('{1.23, 1.32}'::float4[]);
select vec('{1, 5,10, 100, 7}'::int4[]);
select vec('{1, 5,10, 100, 7}'::numeric[]);

select rintarr('{1,2,3,4}'::int2[]);
select rintarr('{1,2,3,null}'::int2[]);
select rintarr('{null}'::int2[]);
select rintarr('{{1,2,3,4},{5,6,7,8}}'::int2[]);
select rdimarr('{{1,2,3,4},{5,6,7,8}}'::int2[]);

select rintarr('{1,2,3,4,5}'::int4[]);
select rintarr('{1,2,3,4,null}'::int4[]);
select rintarr('{null}'::int4[]);
select rintarr('{{1,2,3,4},{5,6,7,8}}'::int4[]);
select rdimarr('{{1,2,3,4},{5,6,7,8}}'::int4[]);

select rintarr('{1,2,3,4,6}'::int8[]);
select rintarr('{1,2,3,6,null}'::int8[]);
select rintarr('{null}'::int8[]);
select rintarr('{{1,2,3,4},{5,6,7,8}}'::int8[]);
select rdimarr('{{1,2,3,4},{5,6,7,8}}'::int8[]);

select rfloatarr('{1.2,2.3,3.4,5.6}'::float8[]);
select rfloatarr('{1.2,2.3,3.4,null}'::float8[]);
select rfloatarr('{null}'::float8[]);
select rfloatarr('{{1.2,2.3,3.4,5.6,6.7},{1.2,2.3,3.4,5.6,6.7}}'::float8[]);
select rdimarr('{{1.2,2.3,3.4,5.6,6.7},{1.2,2.3,3.4,5.6,6.7}}'::float8[]);

select rfloatarr('{1.2,2.3,3.4,5.6,6.7}'::float4[]);
select rfloatarr('{1.2,2.3,3.4,5.6,null}'::float4[]);
select rfloatarr('{null}'::float4[]);
select rfloatarr('{{1.2,2.3,3.4,5.6,6.7},{1.2,2.3,3.4,5.6,6.7}}'::float4[]);
select rdimarr('{{1.2,2.3,3.4,5.6,6.7},{1.2,2.3,3.4,5.6,6.7}}'::float4[]);

select rfloatarr('{1.2,2.3,3.4,5.6,6.7}'::numeric[]);
select rfloatarr('{1.2,2.3,3.4,5.6,null}'::numeric[]);
select rfloatarr('{null}'::numeric[]);
select rfloatarr('{{1.2,2.3,3.4,5.6,6.7},{1.2,2.3,3.4,5.6,6.7}}'::numeric[]);
select rdimarr('{{1.2,2.3,3.4,5.6,6.7},{1.2,2.3,3.4,5.6,6.7}}'::numeric[]);


select rboolarr('{1,1,0}'::bool[]);
select rboolarr('{1,1,0,NULL}'::bool[]);
select rboolarr('{NULL}'::bool[]);
select rboolarr('{{1,1,0},{1,0,0}}'::bool[]);
select rdimarr('{{1,1,0},{1,0,0}}'::bool[]);

select rtimestamparr($${'2012-01-02 12:34:56.789012','2012-01-03 12:34:56.789012'}$$::timestamp[]);

--select rlog100_shared();
select rpg_spi_exec('select 1');

select rlogging();
select rlogging2();
\! psql -d ${PL_TESTDB} -c "select rlogging_fatal();"

select rsetofint4();
select rsetofint8();
select rsetofint2();
select rsetoffloat4();
select rsetoffloat8();
select rsetoftext();

select rsetoffloat8array();
select rsetofint4array();
select rsetofint8array();
select rsetoftextarray();

select runargs1('foo');
select runargs2(123, 'foo');
select runargs3(123, 'foo', 'bar');
select runargs4(1,null,null,1);

select rnested_call_three('foo');
select rnested_call_two('foobar');
select rnested_call_one('foo1');
select rtestudt1( ('t', 1, 2, 3, 4, 5, 6, 'foobar')::test_type );
select rtestudt2( (
        array[true,false,true]::bool[],
        array[1,2,3]::smallint[],
        array[2,3,4]::int[],
        array[3,4,5]::int8[],
        array[4.5,5.5,6.5]::float4[],
        array[5.5,6.5,7.5]::float8[],
        array[6.5,7.5,8.5]::numeric[],
        array['a','b','c']::varchar[])::test_type2 );

select rtestudt6a();
select rtestudt6b();
select rtestudt8();
select rtestudt11();
select * from rtestudt13( (1,2,'a')::test_type3 );

--select paster('{hello, happy}','{world, birthday}',' ');
--select rtest_spi_tup('select fname, lname,username from users order by 1,2,3');
-- This function is of "return setof record" type which is not supported yet
-- select rtest_spi_ta('select oid, typname from pg_type where typname = ''oid'' or typname = ''text''');
