CREATE OR REPLACE FUNCTION pyerror_invalid_program() RETURNS integer AS $$
# container: plc_python_shared
this is invalid python program
$$ LANGUAGE plcontainer;

CREATE OR REPLACE FUNCTION pyerror_invalid_import() RETURNS integer AS $$
# container: plc_python_shared
import invalid_package
$$ LANGUAGE plcontainer;

CREATE OR REPLACE FUNCTION pyerror_invalid_num() RETURNS integer AS $$
# container: plc_python_shared
return (float)3.4
$$ LANGUAGE plcontainer;

CREATE OR REPLACE FUNCTION pyerror_invalid_division() RETURNS integer AS $$
# container: plc_python_shared
return 1/0
$$ LANGUAGE plcontainer;

CREATE OR REPLACE FUNCTION pyerror_raise() RETURNS integer AS $$
# container: plc_python_shared
raise OverflowError
return 3
$$ LANGUAGE plcontainer;

CREATE OR REPLACE FUNCTION pyerror_argument(i integer, j integer) RETURNS integer AS $$
# container: plc_python_shared
return i+j+k
$$ LANGUAGE plcontainer;

select pyerror_invalid_program();

select pyerror_invalid_import();

select pyerror_invalid_num();

select pyerror_invalid_division();

select pyerror_raise();

select pyerror_argument(3,4);
