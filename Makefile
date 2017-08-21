#------------------------------------------------------------------------------
#
# 
# Copyright (c) 2016, Pivotal.
#
#------------------------------------------------------------------------------
MODULE_big = plcontainer

# Release versions
include release.mk

# Directories
SRCDIR = ./src
COMMONDIR = ./src/common
MGMTDIR = ./management
PYCLIENTDIR = src/pyclient/bin
RCLIENTDIR = src/rclient/bin
PYCLIENTINSTALL = pyclient/pyclient
RCLIENTINSTALL = rclient/rclient

# Files to build
FILES = $(shell find $(SRCDIR) -not -path "*client*" -type f -name "*.c")
OBJS = $(foreach FILE,$(FILES),$(subst .c,.o,$(FILE)))

PGXS := $(shell pg_config --pgxs)
include $(PGXS)

# Curl
CURL_CONFIG = $(shell type -p curl-config || echo no)
ifneq ($(CURL_CONFIG),no)
  CURL_VERSION  = $(shell $(CURL_CONFIG) --version | cut -d" " -f2)
  VERSION_CHECK = $(shell expr $(CURL_VERSION) \>\= 7.40.0)
ifeq ($(VERSION_CHECK),1)
  override CFLAGS += -DCURL_DOCKER_API $(shell $(CURL_CONFIG) --cflags)
  SHLIB_LINK = $(shell $(CURL_CONFIG) --libs)
  $(info curl version is >= 7.40, building with Curl Docker API interface)
else
  $(info curl version is < 7.40, falling back to default Docker API interface)
endif
else
  $(info curl-config is not found, building with default Docker API interface)
endif

#libxml
LIBXML_CONFIG = $(shell which xml2-config || echo no)
ifneq ($(LIBXML_CONFIG),no)
  override CFLAGS += $(shell xml2-config --cflags)
  override SHLIB_LINK += $(shell xml2-config --libs)
else
  $(error xml2-config is missing. Have you installed libxml?)
endif

PLCONTAINERDIR = $(DESTDIR)$(datadir)/plcontainer

override CFLAGS += -Werror -Wextra -Wall

# detected the docker API version, only for centos 6
RHEL_MAJOR_OS=$(shell cat /etc/redhat-release | sed s/.*release\ // | sed s/\ .*// | awk -F '.' '{print $$1}' )
ifeq ($(RHEL_MAJOR_OS), 6)
  override CFLAGS +=  -DDOCKER_API_LOW
endif

all: all-lib

install: installdirs install-lib install-extra install-clients

clean: clean-clients

installdirs: installdirs-lib
	$(MKDIR_P) '$(DESTDIR)$(bindir)'
	$(MKDIR_P) '$(PLCONTAINERDIR)'

.PHONY: uninstall
uninstall: uninstall-lib
	rm -f '$(DESTDIR)$(bindir)/plcontainer-config'
	rm -rf '$(PLCONTAINERDIR)'

.PHONY: install-extra
install-extra:
	# Management
	$(INSTALL_PROGRAM) '$(MGMTDIR)/bin/plcontainer-config'               '$(DESTDIR)$(bindir)/plcontainer-config'
	$(INSTALL_PROGRAM) '$(MGMTDIR)/bin/plcontainer'                      '$(DESTDIR)$(bindir)/plcontainer'
	$(INSTALL_DATA)    '$(MGMTDIR)/config/plcontainer_configuration.xml' '$(PLCONTAINERDIR)'
	$(INSTALL_DATA)    '$(MGMTDIR)/sql/plcontainer_install.sql'          '$(PLCONTAINERDIR)'

.PHONY: install-clients
install-clients: clients
	$(MKDIR_P) '$(DESTDIR)$(bindir)/pyclient'
	$(MKDIR_P) '$(DESTDIR)$(bindir)/rclient'
	$(INSTALL_PROGRAM) '$(PYCLIENTDIR)/client'        '$(DESTDIR)$(bindir)/$(PYCLIENTINSTALL)'
	$(INSTALL_PROGRAM) '$(RCLIENTDIR)/client'         '$(DESTDIR)$(bindir)/$(RCLIENTINSTALL)'

.PHONY: installcheck
installcheck:
	$(MAKE) -C tests tests

.PHONY: clients
clients:
	$(MAKE) -C $(SRCDIR)/pyclient
	$(MAKE) -C $(SRCDIR)/rclient
	touch $(COMMONDIR)/clients_timestamp

all-lib: check-clients-make
	@echo "Build PL/Container Done."

.PHONY: check-clients-make
check-clients-make:
	if [ -f $(COMMONDIR)/clients_timestamp ]; then \
	    rm $(COMMONDIR)/clients_timestamp && $(MAKE) clean ; \
	fi

.PHONY: clean-clients
clean-clients:
	$(MAKE) -C $(SRCDIR)/pyclient clean
	$(MAKE) -C $(SRCDIR)/rclient clean
