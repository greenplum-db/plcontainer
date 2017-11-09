#------------------------------------------------------------------------------
#
#  
# Copyright (c) 2016-Present Pivotal Software, Inc
# 
# 

# Global build Directories

PLCONTAINER_DIR = ..

ifeq ($(PYTHON),)
  PYTHON = python
endif

# Python build flags
#PYTHON_CFLAGS = $(shell pkg-config --cflags python-2.7)
#PYTHON_LDFLAGS = $(shell pkg-config --libs python-2.7)
PYTHON_CFLAGS = $(shell $(PYTHON) -c "from distutils import sysconfig; import sys; sys.stdout.write('-I' + sysconfig.get_python_inc())")
PYTHON_LDFLAGS = $(shell $(PYTHON) -c "from distutils import sysconfig; import sys; sys.stdout.write(sysconfig.get_config_var('BLDLIBRARY'))")

override CFLAGS += $(CUSTOMFLAGS) -I$(PLCONTAINER_DIR)/ -DCOMM_STANDALONE -Wall -Wextra -Werror

common_src = $(shell find $(PLCONTAINER_DIR)/common -name "*.c")
common_objs = $(foreach src,$(common_src),$(subst .c,.o,$(src)))
python_src = $(shell find . -name "*.c")
python_objs = $(foreach src,$(python_src),$(subst .c,.o,$(src)))

PYTHONCLIENT=pyclient

.PHONY: default
default: all

%.o: %.c
	$(CC) $(PYTHON_CFLAGS) $(CFLAGS) -c -o $@ $^

build: $(python_objs) $(common_objs)
	LIBRARY_PATH=$(LD_LIBRARY_PATH) $(CC) -o $(PYTHONCLIENT) $^ $(PYTHON_LDFLAGS) $(LIBS)
	cp $(PYTHONCLIENT) bin

PHONY: clean_common
clean_common:
	rm -f $(common_objs)

.PHONY: debug
debug:
	$(MAKE) clean_common
	CUSTOMFLAGS='-D_DEBUG_CLIENT -g -O0' $(MAKE) build
	$(MAKE) clean_common

.PHONY: all
all:
	$(MAKE) clean_common
	CUSTOMFLAGS='-O3 -g' $(MAKE) build
	$(MAKE) clean_common

distclean clean: clean_common
	rm -f *.o
	rm -f $(PYTHONCLIENT)
	rm -f bin/$(PYTHONCLIENT)
