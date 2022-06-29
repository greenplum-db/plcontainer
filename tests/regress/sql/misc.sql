-- Test function cache change. See function_cache_put()

CREATE OR REPLACE FUNCTION pydef1(i integer) RETURNS integer AS $$
# container: plc_python_shared
return i+1
$$ LANGUAGE plcontainer;

select pydef1(1);
select pydef1(2);

CREATE OR REPLACE FUNCTION pydef1(i integer) RETURNS integer AS $$
# container: plc_python_shared
return i+2
$$ LANGUAGE plcontainer;

select pydef1(1);
select pydef1(2);

DROP FUNCTION pydef1(i integer);
