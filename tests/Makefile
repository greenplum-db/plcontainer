#------------------------------------------------------------------------------
# 
# Copyright (c) 2016-Present Pivotal Software, Inc
#
#------------------------------------------------------------------------------

PGXS := $(shell pg_config --pgxs)
include $(PGXS)

REGRESS_OPTS = --dbname=$(PL_TESTDB) --init-file=./init_file

REGRESS_BASIC = plcontainer_install \
	plcontainer_schema

# R Regression Tests
REGRESS_R = plcontainer_function_r \
	plcontainer_function_r_gpdb5
REGRESS_R_TESTS = plcontainer_test_r \
	plcontainer_test_r_gpdb5

# Python Regression Tests
REGRESS_PYTHON = plcontainer_function_python \
	plcontainer_function_python_gpdb5
REGRESS_PYTHON_TESTS = plcontainer_test_python \
	plcontainer_test_python_gpdb5 \
	plcontainer_exception_python \
	plcontainer_faultinject_python \
	plcontainer_function_python_network

# Regression Tests for GPDB5
REGRESS_GPDB5 = $(REGRESS_BASIC) $(REGRESS_R) $(REGRESS_PYTHON) $(REGRESS_PYTHON_TESTS)

PSQLDIR = $(bindir)

.PHONY: default
default: all

.PHONY: all
all: tests


.PHONY: tests
tests:
	$(top_builddir)/src/test/regress/pg_regress \
				--psqldir=$(PSQLDIR) $(REGRESS_OPTS) $(REGRESS_GPDB5)

.PHONY: submake
submake:
	$(MAKE) -C $(top_builddir)/src/test/regress pg_regress$(X)
