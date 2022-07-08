set log_min_messages='DEBUG1';

CREATE FUNCTION transfer_funds_exception() RETURNS void AS $$
#container: plc_python_shared
try:
    with plpy.subtransaction():
        plpy.execute("UPDATE accounts SET balance = balance - 100 WHERE account_name = 'joe'")
        raise Exception('Here is the exception')
        plpy.execute("UPDATE accounts SET balance = balance + 100 WHERE account_name = 'mary'")
except:
    result = "error transferring funds"
else:
    result = "funds transferred correctly"
plan = plpy.prepare("INSERT INTO operations (result) VALUES ($1)", ["text"])
plpy.execute(plan, [result])
$$ LANGUAGE plcontainer;

CREATE FUNCTION transfer_funds_normal() RETURNS void AS $$
#container: plc_python_shared
try:
    with plpy.subtransaction():
        plpy.execute("UPDATE accounts SET balance = balance - 100 WHERE account_name = 'joe'")
        plpy.execute("UPDATE accounts SET balance = balance + 100 WHERE account_name = 'mary'")
except:
    result = "error transferring funds"
else:
    result = "funds transferred correctly"
plan = plpy.prepare("INSERT INTO operations (result) VALUES ($1)", ["text"])
plpy.execute(plan, [result])
$$ LANGUAGE plcontainer;


CREATE FUNCTION transfer_funds_without_subtrans() RETURNS void AS $$
#container: plc_python_shared
try:
        plpy.execute("UPDATE accounts SET balance = balance - 100 WHERE account_name = 'joe'")
        raise Exception('I know Python!')
        plpy.execute("UPDATE accounts SET balance = balance + 100 WHERE account_name = 'mary'")
except:
    result = "error transferring funds"
else:
    result = "funds transferred correctly"
plan = plpy.prepare("INSERT INTO operations (result) VALUES ($1)", ["text"])
plpy.execute(plan, [result])
$$ LANGUAGE plcontainer;

CREATE FUNCTION transfer_funds_oldapi() RETURNS void AS $$
#container: plc_python_shared
subxact = plpy.subtransaction()
subxact.enter()
try:
        plpy.execute("UPDATE accounts SET balance = balance - 100 WHERE account_name = 'joe'")
        plpy.execute("UPDATE accounts SET balance = balance + 100 WHERE account_name = 'mary'")
except:
    result = "error transferring funds"
    import sys
    subxact.exit(*sys.exc_info())
    raise
else:
    result = "funds transferred correctly"
    subxact.exit(None, None, None)
plan = plpy.prepare("INSERT INTO operations (result) VALUES ($1)", ["text"])
plpy.execute(plan, [result])
$$ LANGUAGE plcontainer;

create table accounts(account_name text, balance int);
insert into accounts values('joe',100),('mary',100);
create table operations(result text);

select transfer_funds_exception();
select account_name, balance from accounts order by account_name;
select * from operations order by result;

select transfer_funds_normal();
select account_name, balance from accounts order by account_name;
select * from operations order by result;

select transfer_funds_oldapi();
select account_name, balance from accounts order by account_name;
select * from operations order by result;

select transfer_funds_without_subtrans();
select account_name, balance from accounts order by account_name;
select * from operations order by result;

