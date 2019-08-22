# plcontainer/src/Makefile

MODULE_big = plc_coordinator
DATA = plc_coordinator--1.0.0.sql

EXTENSION = plc_coordinator
SRCDIR = ./
FILES = plc_coordinator.c plc_configuration.c ../common/base_network.c \
	../common/comm_dummy_plc.c 
OBJS = $(foreach src,$(FILES),$(subst .c,.o,$(src)))
SHLIB_LINK = $(libpq) $(shell pkg-config --libs json-c) \
	$(shell xml2-config --libs) \
	$(shell curl-config --libs)

INCLUDE_DIR = ../include
override CFLAGS += -std=gnu99 -I$(INCLUDE_DIR) -Wall -Wextra -Werror

PGXS := $(shell pg_config --pgxs)
include $(PGXS)
