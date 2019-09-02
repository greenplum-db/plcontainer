# plcontainer/src/Makefile

MODULE_big = plc_coordinator
DATA = plc_coordinator--1.0.0.sql

EXTENSION = plc_coordinator
SRCDIR = ./
FILES = plc_coordinator.c containers.c message_fns.c plc_configuration.c \
        plc_docker_api.c plcontainer_udfs.c sqlhandler.c function_cache.c \
        plc_backend_api.c plc_typeio.c subtransaction_handler.c \
        common/base_network.c common/comm_channel.c common/comm_connectivity.c \
        common/comm_dummy_plc.c common/comm_messages.c
OBJS = $(foreach src,$(FILES),$(subst .c,.o,$(src)))

override SHLIB_LINK += $(shell pkg-config --libs json-c)
override SHLIB_LINK += $(shell xml2-config --libs)
override SHLIB_LINK += $(shell curl-config --libs)

INCLUDE_DIR = include
override CFLAGS += -I$(INCLUDE_DIR) -Wall -Wextra -g -O3

PGXS := $(shell pg_config --pgxs)
include $(PGXS)
