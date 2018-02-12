#------------------------------------------------------------------------------
# 
# Copyright (c) 2016-Present Pivotal Software, Inc
#
#------------------------------------------------------------------------------

PGXS := $(shell pg_config --pgxs)
include $(PGXS)

REGRESS_OPTS = --dbname=$(PL_TESTDB) --init-file=./init_file --schedule=pl_schedule

PSQLDIR = $(bindir)

.PHONY: default
default: all

.PHONY: all
all: tests


.PHONY: tests
tests:
	@echo "Tests require three runtime configurations. Will check the three"
	@echo "runtime configurations soon. If there is configuration missing,"
	@echo "please refer plcontainer script concourse/scripts/run_plcontainer_tests.sh"
	@echo "as example to learn how to set up those configurations"
	plcontainer runtime-show -r plc_python_shared
	plcontainer runtime-show -r plc_r_shared
	plcontainer runtime-show -r plc_python_network
	# Might use branch to handle the difference later.
	pg_config --version
	if [ "`pg_config --version`" == "PostgreSQL 8.3.23" ]; then    \
		cp expected/test_python.out.gp5 expected/test_python.out; \
		cp expected/uda_python.out.gp5 expected/uda_python.out;  \
	else                                                         \
		cp expected/test_python.out.gp6 expected/test_python.out; \
		cp expected/uda_python.out.gp6 expected/uda_python.out;  \
	fi
	PL_TESTDB=$(PL_TESTDB) $(top_builddir)/src/test/regress/pg_regress \
				--psqldir=$(PSQLDIR) $(REGRESS_OPTS) || if [[ -f regression.diffs ]]; then cat regression.diffs; exit 1; fi

.PHONY: submake
submake:
	$(MAKE) -C $(top_builddir)/src/test/regress pg_regress$(X)
