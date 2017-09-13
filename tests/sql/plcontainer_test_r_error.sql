CREATE OR REPLACE FUNCTION rerror_invalid_program() RETURNS integer AS $$
# container: plc_r_shared
this is invalid r program
$$ LANGUAGE plcontainer;

CREATE OR REPLACE FUNCTION rerror_invalid_import() RETURNS integer AS $$
# container: plc_r_shared
library(invalid_lib)
$$ LANGUAGE plcontainer;

CREATE OR REPLACE FUNCTION rerror_timestamp(t timestamp) RETURNS timestamp AS $$
# container: plc_r_shared
options(digits.secs = 6)
tmp <- strptime(t,'%Y-%m-%d %H:%M:%OS')
return (as.character(tmp +i))
$$ LANGUAGE plcontainer;

CREATE OR REPLACE FUNCTION rerror_non_exist_function() RETURNS int[] as $$
#container:plc_r_shared
as.mx(array(1:10,c(2,5)))
$$ language plcontainer;

-- spi query a non-exist table
CREATE OR REPLACE FUNCTION rerror_spi_queryfail() RETURNS text AS $$
# container: plc_r_shared
res = pg.spi.exec('select fname from non_exist_table order by 1')
paste(res, sep=',', collapse=',')
$$ LANGUAGE plcontainer;

CREATE OR REPLACE FUNCTION rerror_setof() RETURNS setof int4 AS $$
# container: plc_r_shared
as.vector(array(1:3,c(-1,2)))
$$ LANGUAGE plcontainer ;

CREATE OR REPLACE FUNCTION rerror_datatype() RETURNS setof float8 AS $$
# container: plc_r_shared
as.vector(array(4:15,c(5,3)))
$$ LANGUAGE plcontainer ;

CREATE OR REPLACE FUNCTION rerror_dataframe() RETURNS setof test_type AS $$
# container: plc_r_shared
a=c(TRUE,FALSE,TRUE)
b=as.integer(c(1,2,3))
c=as.integer(c(5,6,7))
d=c(8,9,10)
e=c(1.0, 2.0, 3.0)
f=c(4.0, 5.0, 6.0)
g=c(7,8,9)
h=c('foo','bar')
x<-data.frame(a,b,c,d,e,f,g,h)
return(x)
$$ LANGUAGE plcontainer ;

select rerror_invalid_program();

select rerror_invalid_import();

select rerror_timestamp('2017-09-10 12:10:09');

select rerror_non_exist_function();

select rerror_spi_queryfail(); 

select rerror_setof();

select rerror_datatype();

select rerror_dataframe();
