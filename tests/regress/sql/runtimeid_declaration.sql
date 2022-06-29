CREATE OR REPLACE FUNCTION runtime_id_cr() RETURNS int8 AS $$
# container:
plc_python_shared
return 1
$$ LANGUAGE plcontainer;

CREATE OR REPLACE FUNCTION runtime_id_no_name() RETURNS int8 AS $$
# container:
return 1
$$ LANGUAGE plcontainer;

CREATE OR REPLACE FUNCTION runtime_id_no_hash() RETURNS int8 AS $$
container:
return 1
$$ LANGUAGE plcontainer;

CREATE OR REPLACE FUNCTION runtime_id_lots_space() RETURNS int8 AS $$
	  # container 	:  	 plc_python_shared  
return 1
$$ LANGUAGE plcontainer;

CREATE OR REPLACE FUNCTION runtime_id_too_long() RETURNS int8 AS $$
          #container: plc_python_shared_toooooooooooooooooooooooooooooooooooooooooooooooooooooooo_long
return 1
$$ LANGUAGE plcontainer;

CREATE OR REPLACE FUNCTION runtime_id_blank_line() RETURNS int8 AS $$

return 1
$$ LANGUAGE plcontainer;

CREATE OR REPLACE FUNCTION runtime_id_wrong_start() RETURNS int8 AS $$
# extra_container: plc_python_shared
return 1
$$ LANGUAGE plcontainer;

CREATE OR REPLACE FUNCTION runtime_id_extra_char() RETURNS int8 AS $$
# container_extra: plc_python_shared
return 1
$$ LANGUAGE plcontainer;

CREATE OR REPLACE FUNCTION runtime_id_space_inner() RETURNS int8 AS $$
# container: plc_python_  shared
return 1
$$ LANGUAGE plcontainer;

CREATE OR REPLACE FUNCTION runtime_id_space_in_container() RETURNS int8 AS $$
# cont  ainer: plc_python_shared
return 1
$$ LANGUAGE plcontainer;

CREATE OR REPLACE FUNCTION runtime_id_wrong_char() RETURNS int8 AS $$
# container: plc_#python#_shared
return 1
$$ LANGUAGE plcontainer;

CREATE OR REPLACE FUNCTION runtime_id_no_id() RETURNS int8 AS $$
return 1
$$ LANGUAGE plcontainer;

CREATE OR REPLACE FUNCTION runtime_id_no_colon() RETURNS int8 AS $$
# container plc_python_shared
return 1
$$ LANGUAGE plcontainer;

CREATE OR REPLACE FUNCTION runtime_id_no_space() RETURNS int8 AS $$
#container:plc_python_shared
return 1
$$ LANGUAGE plcontainer;

CREATE OR REPLACE FUNCTION runtime_id_no_cr() RETURNS int8 AS $$
#container:plc_python_shared $$ LANGUAGE plcontainer;

CREATE OR REPLACE FUNCTION runtime_id_blank() RETURNS int8 AS $$
#container:  	$$ LANGUAGE plcontainer;

SELECT runtime_id_cr();
SELECT runtime_id_no_name();
SELECT runtime_id_no_hash();
SELECT runtime_id_lots_space();
SELECT runtime_id_too_long();
SELECT runtime_id_blank_line();
SELECT runtime_id_wrong_start();
SELECT runtime_id_extra_char();
SELECT runtime_id_space_inner();
SELECT runtime_id_space_in_container();
SELECT runtime_id_wrong_char();
SELECT runtime_id_no_id();
SELECT runtime_id_no_colon();
SELECT runtime_id_no_space();
SELECT runtime_id_no_cr();
SELECT runtime_id_blank();

DROP FUNCTION runtime_id_cr();
DROP FUNCTION runtime_id_no_name();
DROP FUNCTION runtime_id_no_hash();
DROP FUNCTION runtime_id_lots_space();
DROP FUNCTION runtime_id_too_long();
DROP FUNCTION runtime_id_blank_line();
DROP FUNCTION runtime_id_wrong_start();
DROP FUNCTION runtime_id_extra_char();
DROP FUNCTION runtime_id_space_inner();
DROP FUNCTION runtime_id_space_in_container();
DROP FUNCTION runtime_id_wrong_char();
DROP FUNCTION runtime_id_no_id();
DROP FUNCTION runtime_id_no_colon();
DROP FUNCTION runtime_id_no_space();
DROP FUNCTION runtime_id_no_cr();
DROP FUNCTION runtime_id_blank();
