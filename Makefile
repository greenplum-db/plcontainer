#------------------------------------------------------------------------------
# 
# Copyright (c) 2016-Present Pivotal Software, Inc
#
#------------------------------------------------------------------------------
MODULE_big = plcontainer
# for extension
EXTENSION = plcontainer

# Directories
SRCDIR = ./src
MGMTDIR = ./management
RCLIENTDIR = src/rclient/bin

# the DATA is including files for extension use
DATA = $(MGMTDIR)/sql/plcontainer--1.0.0.sql $(MGMTDIR)/sql/plcontainer--unpackaged--1.0.0.sql plcontainer.control

ifeq ($(PLC_PG),yes)
  $(shell cd $(MGMTDIR)/sql/ && ln -sf plcontainer_pg--1.0.0.sql plcontainer_install.sql.in )
  $(shell cd $(MGMTDIR)/sql/ && cp -f plcontainer_pg--1.0.0.sql plcontainer--1.0.0.sql )
else
  $(shell cd $(MGMTDIR)/sql/ && ln -sf plcontainer_gp--1.0.0.sql plcontainer_install.sql.in )
  $(shell cd $(MGMTDIR)/sql/ && cp -f plcontainer_gp--1.0.0.sql plcontainer--1.0.0.sql )
endif

# DATA_built will built sql from .in file
DATA_built = $(MGMTDIR)/sql/plcontainer_install.sql $(MGMTDIR)/sql/plcontainer_uninstall.sql

# Files to build
# TODO: clean docker out from plcontainer
FILES = src/function_cache.c src/plcontainer.c  \
        src/containers.c src/message_fns.c src/plc_configuration.c src/plc_docker_api.c \
        src/plc_typeio.c src/subtransaction_handler.c \
        src/common/base_network.c src/common/comm_channel.c src/common/comm_connectivity.c src/common/comm_messages.c src/common/comm_dummy_plc.c
OBJS = $(foreach FILE,$(FILES),$(subst .c,.o,$(FILE)))
		
CXX_FILES=src/proto/client.cc src/proto/proto_utils.cc src/proto/plcontainer.pb.cc src/proto/plcontainer.grpc.pb.cc
OBJS += $(foreach FILE,$(CXX_FILES),$(subst .cc,.o,$(FILE)))

PROTO_PREFIX=plcontainer
PROTO_FILE=$(PROTO_PREFIX).proto
PROTO_CXX_FILES=src/proto/$(PROTO_PREFIX).pb.cc src/proto/$(PROTO_PREFIX).grpc.pb.cc

PROTOC=protoc
GRPC_CPP_PLUGIN=grpc_cpp_plugin
GRPC_CPP_PLUGIN_PATH ?= `which $(GRPC_CPP_PLUGIN)`

PGXS := $(shell pg_config --pgxs)
include $(PGXS)

ifeq ($(PLC_PG),yes)
	override CFLAGS += -DPLC_PG
#	override CFLAGS += -DPLC_PG  -Wno-sign-compare
endif

# FIXME: We might need a configure script to handle below checks later.
# See https://github.com/greenplum-db/plcontainer/issues/322

# Plcontainer version
PLCONTAINER_VERSION = $(shell git describe)
ifeq ($(PLCONTAINER_VERSION),)
  $(error can not determine the plcontainer version)
else
  ver = \#define PLCONTAINER_VERSION \"$(PLCONTAINER_VERSION)\"
  $(shell if [ ! -f src/common/config.h ] || [ "$(ver)" != "`cat src/common/config.h`" ]; then echo "$(ver)" > src/common/config.h; fi)
endif

PLCONTAINERDIR = $(DESTDIR)$(datadir)/plcontainer
INCLUDE_DIR = $(SRCDIR)/include

override CFLAGS += -Werror -Wextra -Wall -Wno-sign-compare -I$(INCLUDE_DIR) -I$(INCLUDE_DIR)/proto
override CFLAGS += $(shell xml2-config --cflags)

CXXFLAGS += $(subst -fexcess-precision=standard,,$(subst -Wmissing-prototypes,,$(subst -std=gnu99,,$(CFLAGS)))) -I$(INCLUDE_DIR) -I$(INCLUDE_DIR)/proto

override SHLIB_LINK += $(shell pkg-config --libs json-c)
override SHLIB_LINK += $(shell xml2-config --libs)
override SHLIB_LINK += $(shell curl-config --libs)

HOST_SYSTEM = $(shell uname | cut -f 1 -d_)
SYSTEM ?= $(HOST_SYSTEM)
CXX = g++
#CPPFLAGS += `pkg-config --cflags protobuf grpc`
CXXFLAGS += -std=c++11
ifeq ($(SYSTEM),Darwin)
#LDFLAGS += -L/usr/local/lib `pkg-config --libs protobuf grpc++`
override SHLIB_LINK += -L/usr/local/lib -lprotobuf -lgrpc++\
           -lgrpc++_reflection\
           -ldl
else
#LDFLAGS += -L/usr/local/lib `pkg-config --libs protobuf grpc++`
override SHLIB_LINK += -L/usr/local/lib -lprotobuf -lgrpc++\
           -Wl,--no-as-needed -lgrpc++_reflection -Wl,--as-needed\
           -ldl
endif


ifeq ($(ENABLE_COVERAGE),yes)
  override CFLAGS += -coverage
  override SHLIB_LINK += -lgcov --coverage
endif

all: proto coordinator all-lib
	@echo "Build PL/Container Done."


install: all install-coordinator installdirs install-lib install-extra install-clients

clean: clean-clients clean-coverage clean-proto
distclean: distclean-config

.PHONY: coordinator
coordinator:
	$(MAKE) -C $(SRCDIR)/

.PHONY: install-coordinator
install-coordinator:
	$(MAKE) -C $(SRCDIR)/ install

installdirs: installdirs-lib
	$(MKDIR_P) '$(DESTDIR)$(bindir)'
	$(MKDIR_P) '$(PLCONTAINERDIR)'

uninstall: uninstall-lib
	rm -f '$(DESTDIR)$(bindir)/plcontainer'
	rm -rf '$(PLCONTAINERDIR)'

.PHONY: proto
proto:
	@echo "Generating protobuf and grpc files."
	$(PROTOC) -I src --grpc_out=src/proto --plugin=protoc-gen-grpc=$(GRPC_CPP_PLUGIN_PATH) $(PROTO_FILE)
	$(PROTOC) -I src --cpp_out=src/proto $(PROTO_FILE)
	mv src/proto/$(PROTO_PREFIX).pb.h src/include/proto/	
	mv src/proto/$(PROTO_PREFIX).grpc.pb.h src/include/proto/	

.PHONY: clean-proto
clean-proto:
	rm -f src/include/proto/$(PROTO_PREFIX).pb.h 
	rm -f src/include/proto/$(PROTO_PREFIX).grpc.pb.h 
	rm -f src/proto/$(PROTO_PREFIX).pb.cc 
	rm -f src/proto/$(PROTO_PREFIX).grpc.pb.cc 

.PHONY: install-extra
install-extra: installdirs
	$(INSTALL_PROGRAM) '$(MGMTDIR)/bin/plcontainer'                         '$(DESTDIR)$(bindir)/plcontainer'
	$(INSTALL_DATA)    '$(MGMTDIR)/config/plcontainer_configuration.xml'    '$(PLCONTAINERDIR)/'
	$(INSTALL_DATA)    '$(MGMTDIR)/sql/plcontainer_install.sql.in'             '$(PLCONTAINERDIR)/plcontainer_install.sql'
	$(INSTALL_DATA)    '$(MGMTDIR)/sql/plcontainer_uninstall.sql.in'           '$(PLCONTAINERDIR)/plcontainer_uninstall.sql'

.PHONY: install-clients
install-clients:
	$(MKDIR_P) '$(DESTDIR)$(bindir)/plcontainer_clients'
	cp $(RCLIENTDIR)/*  $(DESTDIR)$(bindir)/plcontainer_clients/

.PHONY: installcheck
installcheck:
	$(MAKE) -C tests tests

.PHONY: build-clients
build-clients:
	$(MAKE) -C $(SRCDIR)/rclient all

.PHONY: clean-clients
clean-clients:
	$(MAKE) -C $(SRCDIR)/rclient clean

.PHONY: clean-coverage
clean-coverage:
	rm -f `find . -name '*.gcda' -print`
	rm -f `find . -name '*.gcno' -print`
	rm -rf coverage.info coverage_result

.PHONY: distclean-config
distclean-config:
	rm -f src/common/config.h

.PHONY: coverage-report
coverage-report:
	lcov -c -o coverage.info -d .
	genhtml coverage.info -o coverage_result
