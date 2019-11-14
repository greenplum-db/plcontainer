# plcontainer/src/Makefile

MODULE_big = plc_coordinator
DATA = plc_coordinator--1.0.0.sql

EXTENSION = plc_coordinator
SRCDIR = ./
FILES = plc_coordinator.c containers.c message_fns.c plc_configuration.c \
        plc_docker_api.c plcontainer_udfs.c function_cache.c \
        plc_typeio.c subtransaction_handler.c \
        common/base_network.c common/comm_channel.c common/comm_connectivity.c \
        common/comm_dummy_plc.c common/comm_messages.c
OBJS = $(foreach src,$(FILES),$(subst .c,.o,$(src)))

CXX_FILES=proto/async_server.cc proto/client.cc proto/proto_utils.cc proto/plcontainer.pb.cc proto/plcontainer.grpc.pb.cc
OBJS += $(foreach src,$(CXX_FILES),$(subst .cc,.o,$(src)))

override SHLIB_LINK += $(shell pkg-config --libs json-c)
override SHLIB_LINK += $(shell xml2-config --libs)
override SHLIB_LINK += $(shell curl-config --libs)

override CFLAGS += $(shell xml2-config --cflags)

HOST_SYSTEM = $(shell uname | cut -f 1 -d_)
SYSTEM ?= $(HOST_SYSTEM)
CXX = g++
CXXFLAGS += -std=c++11
ifeq ($(SYSTEM),Darwin)
override SHLIB_LINK += -L/usr/local/lib -lprotobuf -lgrpc++\
           -lgrpc++_reflection\
           -ldl
else
override SHLIB_LINK += -L/usr/local/lib -lprotobuf -lgrpc++\
           -Wl,--no-as-needed -lgrpc++_reflection -Wl,--as-needed\
           -ldl
endif

INCLUDE_DIR = include
override CFLAGS += -std=gnu99 -I$(INCLUDE_DIR) -I$(INCLUDE_DIR)/proto -Wall -Wextra -g3 -O0 -ggdb
override CXXFLAGS += $(subst -fexcess-precision=standard,,$(subst -Wmissing-prototypes,,$(subst -std=gnu99,,$(CFLAGS)))) -I$(INCLUDE_DIR) -I$(INCLUDE_DIR)/proto

PGXS := $(shell pg_config --pgxs)
include $(PGXS)
