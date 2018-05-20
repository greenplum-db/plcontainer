-- Test UDA with prefunc
CREATE FUNCTION rsfunc_sum_cols(state numeric, col_a numeric, col_b numeric) 
RETURNS numeric AS 
$$
# container: plc_r_shared
   return (state + col_a + col_b)
$$ language plcontainer;


CREATE FUNCTION prefunc_rsum_cols(state_a numeric, state_b numeric) 
RETURNS numeric AS 
$$
# container: plc_r_shared
   return (state_a + state_b)
$$ language plcontainer;

CREATE AGGREGATE rsum_cols(numeric, numeric) (
	   SFUNC = rsfunc_sum_cols,
	   PREFUNC = prefunc_rsum_cols,
	   STYPE = numeric,
	   INITCOND = 0 
);

create table tr (i int, j int);
insert into tr values(2,1),(4,8),(9,10),(10,24),(44,11);
select rsum_cols(i,j) FROM tr;
drop table tr;

