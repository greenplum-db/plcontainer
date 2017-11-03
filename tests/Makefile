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
	PL_TESTDB=$(PL_TESTDB) $(top_builddir)/src/test/regress/pg_regress \
				--psqldir=$(PSQLDIR) $(REGRESS_OPTS) || if [[ -f regression.diffs ]]; then cat regression.diffs; exit 1; fi

.PHONY: submake
submake:
	$(MAKE) -C $(top_builddir)/src/test/regress pg_regress$(X)
