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
#CLIENT_CFLAGS = $(shell pkg-config --cflags python-2.7)
#CLIENT_LDFLAGS = $(shell pkg-config --libs python-2.7)
CLIENT_CFLAGS = $(shell $(PYTHON) -c "from distutils import sysconfig; import sys; sys.stdout.write('-I' + sysconfig.get_python_inc())")
CLIENT_LDFLAGS = $(shell $(PYTHON) -c "from distutils import sysconfig; import sys; sys.stdout.write(sysconfig.get_config_var('BLDLIBRARY'))")

override CFLAGS += $(CUSTOMFLAGS) -I$(PLCONTAINER_DIR)/ -DCOMM_STANDALONE -Wall -Wextra -Werror

CLIENT = pyclient
common_src = $(shell find $(PLCONTAINER_DIR)/common -name "*.c")
common_objs = $(foreach src,$(common_src),$(subst .c,.$(CLIENT).o,$(src)))
client_src = $(shell find . -name "*.c")
client_objs = $(foreach src,$(client_src),$(subst .c,.o,$(src)))

.PHONY: default
default: all

$(common_objs): %.$(CLIENT).o: %.c
	$(CC) $(CLIENT_CFLAGS) $(CFLAGS) -c -o $@ $^

$(client_objs): %.o: %.c
	$(CC) $(CLIENT_CFLAGS) $(CFLAGS) -c -o $@ $^

build: $(client_objs) $(common_objs)
	LIBRARY_PATH=$(LD_LIBRARY_PATH) $(CC) -o $(CLIENT) $^ $(CLIENT_LDFLAGS) $(LIBS)
	cp $(CLIENT) bin

.PHONY: debug
debug: CUSTOMFLAGS = -D_DEBUG_CLIENT -g -O0
debug: build

.PHONY: all
all: CUSTOMFLAGS = -O3 -g
all: build

distclean clean:
	rm -f $(common_objs)
	rm -f *.o
	rm -f $(CLIENT)
	rm -f bin/$(CLIENT)
