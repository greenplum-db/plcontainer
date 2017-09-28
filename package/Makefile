all: gppkg

PGXS := $(shell pg_config --pgxs)
include $(PGXS)

ifndef PLCONTAINER_VERSION
  PLCONTAINER_VERSION="0.0"
endif
ifndef PLCONTAINER_RELEASE
  PLCONTAINER_RELEASE="0"
endif
MAJOR_OS=$(shell cat /etc/redhat-release | sed s/.*release\ // | sed s/\ .*// | awk -F '.' '{print $$1}' )
ARCH=$(shell uname -p)

PLC_DIR=`cd .. && pwd`
PLC_GPPKG_VER=$(PLCONTAINER_VERSION).$(PLCONTAINER_RELEASE)
PLC_RPM_FLAGS="--define 'plc_dir $(PLC_DIR)' --define 'plc_ver $(PLCONTAINER_VERSION)' --define 'plc_rel $(PLCONTAINER_RELEASE)'"
PLC_RPM=plcontainer-$(PLCONTAINER_VERSION)-$(PLCONTAINER_RELEASE).$(ARCH).rpm
PLC_GPPKG=plcontainer-$(PLC_GPPKG_VER)-rhel$(MAJOR_OS)-$(ARCH).gppkg

TARGET_GPPKG=$(PLC_GPPKG)
EXTRA_CLEAN+=$(R_RPM) $(PLR_RPM) $(PLR_GPPKG)

#
# Generic rules to build gppkgs included here
#
include gppkg.mk

.PHONY: gppkg
gppkg:
	$(MAKE) $(PLC_RPM) RPM_FLAGS=$(PLC_RPM_FLAGS)
	$(MAKE) $(PLC_GPPKG) MAIN_RPM=$(PLC_RPM)

.PHONY: cleanall
cleanall: clean
	rm -rf BUILDROOT
	rm -rf SOURCES
	rm -rf SRPMS
	rm -rf $(PLC_RPM)
	rm -rf $(PLC_GPPKG)
