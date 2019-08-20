# plcontainer/src/Makefile

MODULE_big = plc_coordinator
DATA = plc_coordinator-1.0.0.sql

EXTENSION = plc_coordinator
SRCDIR = ./
FILES = plc_coordinator.c
OBJS = plc_coordinator.o
SHLIB_LINK = $(libpq)

PGXS := $(shell pg_config --pgxs)
include $(PGXS)
