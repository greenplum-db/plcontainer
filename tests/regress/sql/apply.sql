drop type if exists add_one_input;
create type add_one_input as (
    i int
);

create or replace function add_one_for_apply(_ add_one_input[])
returns setof record as $$
# container: plc_python_shared
return [{"i": row["i"] + 1} for row in _]
$$ language plcontainer;

-- batch size == 0
select * 
from plcontainer_apply(
    table(select generate_series(1, 10)), 'add_one_for_apply', 0
) as (i int);

-- data size % batch size == 0
select * 
from plcontainer_apply(
    table(select generate_series(1, 10)), 'add_one_for_apply', 1
) as (i int);

-- data size % batch size == 1
select * 
from plcontainer_apply(
    table(select generate_series(1, 10)), 'add_one_for_apply', 3
) as (i int);

-- data size < batch size
select * 
from plcontainer_apply(
    table(select generate_series(1, 10)), 'add_one_for_apply', 100
) as (i int);

-- data size % batch size != 0 and != 1
select * 
from plcontainer_apply(
    table(select generate_series(1, 10)), 'add_one_for_apply', 4
) as (i int);

-- bulk transmitting >= 1 GB data
create or replace view "1gb_text" as (
    select repeat('a', 1 << 20) as a from generate_series(1, 1 << 10)
);

create or replace function echo_for_apply(arg_records "1gb_text"[])
returns setof record as $$
# container: plc_python_shared
return arg_records
$$ language plcontainer;

-- should throw an ERROR when using array_agg()
select echo_for_apply(array_agg("1gb_text")) from "1gb_text";

-- no ERROR when using plcontainer_apply()
select count(*)
from plcontainer_apply(
    table(table "1gb_text"), 'echo_for_apply', 100
) as (a text);

-- check if NULL in table is handled correctly
create or replace function check_none_for_apply(_ add_one_input[])
returns setof record as $$
# container: plc_python_shared
return [{"i": row["i"] is None} for row in _]
$$ language plcontainer;

select * 
from plcontainer_apply(
    table(select NULL FROM generate_series(1, 10)), 'check_none_for_apply', 3
) as (i bool);

-- empty table
select * 
from plcontainer_apply(
    table(select WHERE FALSE), 'check_none_for_apply', 3
) as (i bool);

-- check if we can return None
create or replace function return_none_for_apply(arg_records add_one_input[])
returns setof record as $$
# container: plc_python_shared
return [{"r": None}]
$$ language plcontainer;

select r IS NULL
from plcontainer_apply(
    table(SELECT 1), 'return_none_for_apply', 1
) as (r text);

-- Should throw an ERROR if UDF not exists
select *
from plcontainer_apply(
    table(SELECT 1), 'non_exist_udf', 1
) as (a text);
