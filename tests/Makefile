#------------------------------------------------------------------------------
#
# 
# Copyright (c) 2016, Pivotal.
#
#------------------------------------------------------------------------------
 

PGXS := $(shell pg_config --pgxs)
include $(PGXS)

REGRESS_OPTS = --dbname=$(PL_TESTDB) --init-file=./init_file

# R Regression Tests
REGRESS_R = plcontainer_function_r plcontainer_function_r_gpdb5 \
		    plcontainer_test_r plcontainer_test_r_gpdb5
REGRESS_R_GPDB4 = plcontainer_function_r plcontainer_test_r

# Python Regression Tests
REGRESS_PYTHON = plcontainer_function_python plcontainer_function_python_gpdb5 \
				 plcontainer_test_python plcontainer_test_python_gpdb5 \
				 plcontainer_test_anaconda \
				 plcontainer_function_python3 plcontainer_function_python3_gpdb5 \
 				 plcontainer_test_python3 plcontainer_test_python3_gpdb5 \
 				 plcontainer_test_anaconda3
REGRESS_PYTHON_GPDB4 = plcontainer_function_python \
 				 plcontainer_test_python \
 				 plcontainer_test_anaconda \
 				 plcontainer_function_python3 \
  				 plcontainer_test_python3 \
  				 plcontainer_test_anaconda3

# Regression Tests for GPDB5
REGRESS_GPDB5_OLD = plcontainer_install plcontainer_schema $(REGRESS_PYTHON) $(REGRESS_R)
REGRESS_GPDB5 = plcontainer_install plcontainer_schema plcontainer_function_python plcontainer_function_python_gpdb5 plcontainer_function_r plcontainer_function_r_gpdb5
# Regression Tests for GPDB4
REGRESS_GPDB4 = plcontainer_install plcontainer_schema $(REGRESS_PYTHON_GPDB4) $(REGRESS_R_GPDB4)

PSQLDIR = $(bindir)

.PHONY: default
default: all

.PHONY: all
all: tests

# Python3 and Python share the same tests, but container is different, so we have
# to alter function definitions to test python3
.PHONY: all
prepare:
	# First Python Functions
	sed 's/plc_python$$/plc_python3/g' sql/plcontainer_function_python.sql | \
		sed 's/plc_anaconda/plc_anaconda3/g' > sql/plcontainer_function_python3.sql
	sed 's/plc_python$$/plc_python3/g' sql/plcontainer_function_python_gpdb5.sql | \
		sed 's/plc_anaconda/plc_anaconda3/g' > sql/plcontainer_function_python3_gpdb5.sql
	sed 's/plc_python$$/plc_python3/g' expected/plcontainer_function_python.out | \
		sed 's/plc_anaconda/plc_anaconda3/g' > expected/plcontainer_function_python3.out
	sed 's/plc_python$$/plc_python3/g' expected/plcontainer_function_python_gpdb5.out | \
		sed 's/plc_anaconda/plc_anaconda3/g' > expected/plcontainer_function_python3_gpdb5.out
	# Then Python Tests
	sed 's/plc_python$$/plc_python3/g' sql/plcontainer_test_python.sql | \
		sed 's/plc_anaconda/plc_anaconda3/g' > sql/plcontainer_test_python3.sql
	sed 's/plc_python$$/plc_python3/g' sql/plcontainer_test_python_gpdb5.sql | \
		sed 's/plc_anaconda/plc_anaconda3/g' > sql/plcontainer_test_python3_gpdb5.sql
	cp expected/plcontainer_test_python.out expected/plcontainer_test_python3.out
	cp expected/plcontainer_test_python_gpdb5.out expected/plcontainer_test_python3_gpdb5.out
	# Then Anaconda
	cp sql/plcontainer_test_anaconda.sql sql/plcontainer_test_anaconda3.sql
	cp expected/plcontainer_test_anaconda.out expected/plcontainer_test_anaconda3.out

.PHONY: tests
tests: prepare
	$(top_builddir)/src/test/regress/pg_regress \
				--psqldir=$(PSQLDIR) $(REGRESS_OPTS) $(REGRESS_GPDB5)

.PHONY: tests4
tests4: prepare
	$(top_builddir)/src/test/regress/pg_regress \
				--psqldir=$(PSQLDIR) $(REGRESS_OPTS) $(REGRESS_GPDB4)

.PHONY: submake
submake:
	$(MAKE) -C $(top_builddir)/src/test/regress pg_regress$(X)
