drop type if exists add_one_input;
create type add_one_input as (
    i int
);

create or replace function add_one_for_apply(_ add_one_input[])
returns setof record as $$
# container: plc_python_user
for row in _:
    yield {"i": row["i"] + 1}
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
# container: plc_python_user
for row in arg_records:
    yield row
$$ language plcontainer;

-- should throw an ERROR when using array_agg()
select echo_for_apply(array_agg("1gb_text")) from "1gb_text";

-- no ERROR when using plcontainer_apply()
select count(*)
from plcontainer_apply(
    table(table "1gb_text"), 'echo_for_apply', 10
) as (a text);
