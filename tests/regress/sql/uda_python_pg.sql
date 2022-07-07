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
-- Test GD(global dictionary)
CREATE or replace FUNCTION pystack(state text, groupid text, vec text)
RETURNS text AS
$$
# container: plc_python_shared
    import ast

    if not (GD.has_key(groupid)):
        GD[groupid] = [ast.literal_eval('[{vecstring}]'.format(vecstring=vec))]
        return groupid
    else:
        GD[groupid].append(ast.literal_eval('[{vecstring}]'.format(vecstring=vec)))
        return groupid

$$ language plcontainer;

CREATE FUNCTION pystack_ffunc(groupid text)
RETURNS text AS
$$
# container: plc_python_shared
   import cPickle
   serialized_array = cPickle.dumps(GD[groupid])
   return serialized_array
$$ language plcontainer;

CREATE AGGREGATE stack_array_agg (text,text) (
	   SFUNC = pystack,
	   FINALFUNC = pystack_ffunc,
	   STYPE = text
);

create table a(i int, col text);
insert into a values(1,'2,3,4'),(2,'4,5,6'),(3,'5,6,8');

select stack_array_agg('1', col) from a;
drop table a;
