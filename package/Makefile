#------------------------------------------------------------------------------
#
# Copyright (c) 2016-Present Pivotal Software, Inc
#
#------------------------------------------------------------------------------

all: gppkg

PGXS := $(shell pg_config --pgxs)
include $(PGXS)

ifndef PLCONTAINER_VERSION
  PLCONTAINER_VERSION="1.0"
endif
ifndef PLCONTAINER_RELEASE
  PLCONTAINER_RELEASE="0"
endif
MAJOR_OS=$(shell cat /etc/redhat-release | sed s/.*release\ // | sed s/\ .*// | awk -F '.' '{print $$1}' )
ARCH=$(shell uname -p)

#json-c
JSON_VER=0.12
ifeq ($(JSON_VER),)
	  $(error can not determine the libjson-c version)
endif
JSON_REL=1
JSON_RPM_FLAGS="--define 'json_ver $(JSON_VER)' --define 'json_rel $(JSON_REL)'"
JSON_RPM=json-c-$(JSON_VER)-$(JSON_REL).$(ARCH).rpm

PLC_DIR=`cd .. && pwd`
PLC_GPPKG_VER=$(PLCONTAINER_VERSION).$(PLCONTAINER_RELEASE)
PLC_RPM_FLAGS="--define 'plc_dir $(PLC_DIR)' --define 'plc_ver $(PLCONTAINER_VERSION)' --define 'plc_rel $(PLCONTAINER_RELEASE)'"
PLC_RPM=plcontainer-$(PLCONTAINER_VERSION)-$(PLCONTAINER_RELEASE).$(ARCH).rpm
PLC_GPPKG=plcontainer-$(PLC_GPPKG_VER)-GP_VERSION-rhel$(MAJOR_OS)-$(ARCH).gppkg

TARGET_GPPKG=$(PLC_GPPKG)
EXTRA_CLEAN+=$(JSON_RPM) $(PLC_RPM) $(PLC_GPPKG)

#
# Generic rules to build gppkgs included here
#
include gppkg.mk

.PHONY: gppkg
gppkg:
	$(MAKE) $(JSON_RPM) RPM_FLAGS=$(JSON_RPM_FLAGS)
	$(MAKE) $(PLC_RPM) RPM_FLAGS=$(PLC_RPM_FLAGS)
	$(MAKE) $(PLC_GPPKG) MAIN_RPM=$(PLC_RPM) DEPENDENT_RPMS=$(JSON_RPM)

.PHONY: cleanall
cleanall: clean
	rm -rf BUILDROOT
	rm -rf SOURCES
	rm -rf SRPMS
	rm -rf $(PLC_RPM)
	rm -rf $(PLC_GPPKG)
