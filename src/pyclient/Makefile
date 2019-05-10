#------------------------------------------------------------------------------
#
#  
# Copyright (c) 2016-Present Pivotal Software, Inc
# 
#------------------------------------------------------------------------------

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

override CFLAGS += $(CLIENT_CFLAGS) -I$(PLCONTAINER_DIR)/ -DPLC_CLIENT -Wall -Wextra -Werror -Wno-unused-result
override LDFLAGS += $(CLIENT_LDFLAGS)

CLIENT = pyclient
common_src = $(shell find $(PLCONTAINER_DIR)/common -name "*.c")
common_objs = $(foreach src,$(common_src),$(subst .c,.$(CLIENT).o,$(src)))
client_src = $(shell find . -name "*.c")
client_objs = $(foreach src,$(client_src),$(subst .c,.o,$(src)))

.PHONY: default
default: all

# add auto dependency for common src used by pyclient. Refer to the following link:
# https://www.gnu.org/software/make/manual/html_node/Automatic-Prerequisites.html
common_dep = $(foreach src,$(common_src),$(subst .c,.$(CLIENT).d,$(src)))
$(common_dep): $(common_src)
	@set -e; rm -f $@; $(CC) -M $(CFLAGS) $< > $@.$$$$; sed 's,\($*\)\.o[ :]*,\1.o $@ : ,g' < $@.$$$$ > $@; rm -f $@.$$$$

$(common_objs): %.$(CLIENT).o: %.c $(common_dep)
	$(CC)  $(CFLAGS) -c -o $@ $<

# add auto dependency for pyclient Makefile. Refer to the following link:
# http://make.mad-scientist.net/papers/advanced-auto-dependency-generation/#combine
DEPDIR := .d
$(shell mkdir -p $(DEPDIR) >/dev/null)
DEPFLAGS = -MT $@ -MMD -MP -MF $(DEPDIR)/$*.Td
POSTCOMPILE = @mv -f $(DEPDIR)/$*.Td $(DEPDIR)/$*.d && touch $@
$(client_objs): %.o: %.c $(DEPDIR)/%.d
	$(CC) $(DEPFLAGS) $(CFLAGS) -c -o $@ $<
	$(POSTCOMPILE)

.PHONY: all
all: $(client_objs) $(common_objs)
	LIBRARY_PATH=$(LD_LIBRARY_PATH) $(CC) -o $(CLIENT) $^ $(LDFLAGS) $(LIBS)
	cp $(CLIENT) bin

.PHONY: clean
clean:
	rm -f $(common_objs)
	rm -f *.o
	rm -f $(CLIENT)
	rm -f bin/$(CLIENT)
	rm -f $(common_dep)
	rm -rf $(DEPDIR)

$(DEPDIR)/%.d: ;
.PRECIOUS: $(DEPDIR)/%.d

include $(wildcard $(patsubst %,$(DEPDIR)/%.d,$(basename $(client_src))))
