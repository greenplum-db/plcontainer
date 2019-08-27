-- should error out but should not crash PG
CREATE OR REPLACE
FUNCTION r_bad_fun()
RETURNS int4 AS
$$
# container: plc_r_shared
  deadbeef <- function(,bad) {}
  42
$$
LANGUAGE plcontainer;
select r_bad_fun();
