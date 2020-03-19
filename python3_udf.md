# Write UDF in PLContainer python3

After installation of plcontainer extension and python3 docker image settings, we can now start to write python3 UDF in GPDB.

If you are familiar with PLContainer Python2 UDF, the differences in Python3 are changing the "container" name to Python3 runtime and using Python3 syntax.

[What' new in Python3](https://docs.python.org/3/whatsnew/3.0.html)

Syntax of UDF:
```sql
CREATE FUNCTION funcname (argument-list)
  RETURNS return-type
AS $$
# conatiner: CONTAINERNAME
function body
$$ LANGUAGE plcontainer;
```
The body of a function is simply a Python script. When the function is called, its arguments are passed as elements of the list args; named arguments are also passed as ordinary variables to the Python script. Use of named arguments is usually more readable. The result is returned from the Python code in the usual way, with return or yield (in case of a result-set statement). PL/Python translates Python's None into the SQL null value.

Example UDF:

```sql
CREATE OR REPLACE FUNCTION pyexample_func(r test_type[]) RETURNS int AS $$
# container: plc_python3
if r is None:
    return 1
for el in r:
    if el is None:
        return 2
return 3
$$ LANGUAGE plcontainer;
```
**\# container: plc_python3** must appear in the first line of function body, replace "plc_python3" with the python3 runtime name you defined in plcontainer command line tool.

If you want to run the same function body in both python2 and python3, you need to define 2 different UDF, using python2 and python3 respectively.

Some features in python3 are not supported in PLContainer yet. Please refer to our [wiki](https://github.com/greenplum-db/plcontainer/wiki/PLContainer-Unsupported-Features) for details.

## Data type mapping
* Greenplum boolean is converted to Python bool. When the UDF return type is boolean, the return value will be evaluated for truth according to the Python rules. That is, 0 and empty string are false, but notably 'f' is true.

Example:
```sql
CREATE OR REPLACE FUNCTION pybool_func(a int) RETURNS boolean AS $$
# container: plc_python3_shared
    if (a > 0):
        return True
    else:
        return False
$$ LANGUAGE plcontainer;
```

```
test=# select pybool_func(-1);

 pybool_func
-------------
 f
(1 row)
```
* Greenplum bytea is converted to bytes.
* Greenplum smallint, int bigint and oid are converted to Python int.
* Greenplum real and double are converted to Python float.
* Greenplum numeric is converted to Python Decimal.
* All other data types will be converted to string.
* None
If an SQL null value is passed to a function, the argument value will appear as None in Python. For example,
```sql
CREATE OR REPLACE FUNCTION pymax(a int, b int) RETURNS integer AS $$
# container: plc_python3_shared
  if (a is None) or (b is None):
    return None
  if a > b:
    return a
  return b
$$ LANGUAGE plcontainer;
```

```
test=# select pymax(null, 2);
 pymax
-------

(1 row)
```
* SQL array values are passed into PLContainer as a Python list. To return an SQL array value out of a PLContainer python function, return a Python list:

```sql
CREATE OR REPLACE FUNCTION pyreturn_arr() RETURNS int[]
AS $$
# container: plc_python3_shared
return [1, 2, 3, 4, 5]
$$ LANGUAGE plcontainer;
```

```
test=# select pyreturn_arr();
 pyreturn_arr
--------------
 {1,2,3,4,5}
(1 row)
```

* self defined type

We have a table named employee.
```sql
CREATE TABLE employee (
  name text,
  salary integer,
  age integer
);
```
Now we defined a function 
```sql
CREATE OR REPLACE FUNCTION overpaid (e employee)
  RETURNS boolean
AS $$
# container: plc_python3_shared
  if e["salary"] > 200000:
    return True
  if (e["age"] < 30) and (e["salary"] > 100000):
    return True
  return False
$$ LANGUAGE plcontainer;
```
Apply this function to employee table:
```
test=# select * from employee;
 name | salary | age
------+--------+-----
 jack | 105000 |  35
 mary | 205000 |  35
(2 rows)

test=# select overpaid(emp) from employee emp;
 overpaid
----------
 f
 t
(2 rows)
```
If we want return row or composite types from a Python function, we can create a self defined type:

```sql
CREATE TYPE named_value AS (
  name   text,
  value  integer
);


# Now redefine overpaid function
CREATE OR REPLACE FUNCTION overpaid (e employee)
  RETURNS named_value
AS $$
# container: plc_python3_shared
  if e["salary"] > 200000:
    return {"name": e["name"], "value": e["salary"]}
  if (e["age"] < 30) and (e["salary"] > 100000):
    return {"name": e["name"], "value": e["salary"]}
  return None
$$ LANGUAGE plcontainer;
```

```bash
test=# select overpaid(emp) from employee emp;
   overpaid
---------------

 (mary,205000)
(2 rows)
```
* setof type
A PLContainer python function can also return sets of scalar or composite types. The following examples assume we have composite type:
```sql
CREATE TYPE greeting AS (
  how text,
  who text
);
```

A set result can be returned from a:

Sequence type (tuple, list, set)
```sql
CREATE FUNCTION greet (how text)
  RETURNS SETOF greeting
AS $$
# container: plc_python3_shared
  # return tuple containing lists as composite types
  # all other combinations work also
  return ( {"how": how, "who": "World"}, {"how": how, "who": "Greenplum"} )
$$ LANGUAGE plcontainer;
```

```
test=# select greet('hello');
       greet
-------------------
 (hello,World)
 (hello,Greenplum)
(2 rows)
```