-- Test UDA with prefunc
CREATE FUNCTION sfunc_sum_cols(state numeric, col_a numeric, col_b numeric) 
RETURNS numeric AS 
$$
# container: plc_python_shared
   return state + col_a + col_b
$$ language plcontainer;


CREATE FUNCTION prefunc_sum_cols(state_a numeric, state_b numeric) 
RETURNS numeric AS 
$$
# container: plc_python_shared
   return state_a + state_b
$$ language plcontainer;

CREATE FUNCTION final_func_sum_cols(state_a numeric)
RETURNS numeric AS
$$
# container: plc_python_shared
   return state_a*10
$$ language plcontainer;

CREATE AGGREGATE sum_cols(numeric, numeric) (
	   SFUNC = sfunc_sum_cols,
	   PREFUNC = prefunc_sum_cols,
	   FINALFUNC = final_func_sum_cols,
	   STYPE = numeric,
	   INITCOND = 0 
);

create table t (i int, j int);
insert into t values(2,1),(4,8),(9,10),(10,24),(44,11);
select sum_cols(i,j) FROM t;
drop table t;
