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

.PHONY: default
default: clean all

.PHONY: clean_common
clean_common:
	rm -f $(common_objs)

%.o: %.c
	$(CC) $(PYTHON_CFLAGS) $(CFLAGS) -c -o $@ $^

client: $(python_objs) $(common_objs)
	LIBRARY_PATH=$(LD_LIBRARY_PATH) $(CC) -o $@ $^ $(PYTHON_LDFLAGS) $(LIBS)
	cp client bin

.PHONY: debug
debug: CUSTOMFLAGS = -D_DEBUG_CLIENT -g -O0
debug: client

.PHONY: all
all: CUSTOMFLAGS = -O3 -g
all: client

clean: clean_common
	rm -f *.o
	rm -f client
	rm -f bin/client
