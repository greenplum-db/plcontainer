PGXS := $(shell pg_config --pgxs)
include $(PGXS)

GP_VERSION_NUM := $(GP_MAJORVERSION)

MAJOR_OS=ubuntu18
ARCH=$(shell uname -p)

ifeq($(ARCH),x86_64)
ARCH=amd64
endif

DEB_ARGS=$(subst -, ,$*)
DEB_NAME=$(word 1,$(RPM_ARGS))
PWD=$(shell pwd)
CONTROL_NAME=plcontainer.control.in
%.deb: 
	rm -rf UBUNTU
	mkdir UBUNTU/DEBIAN -p
	cat $(PWD)/$(CONTROL_NAME) | sed -r "s|#version|$(PLC_GPPKG_VER)|" | sed -r "s|#arch|$(ARCH)|" > $(PWD)/UBUNTU/DEBIAN/control
	$(MAKE) -C $(PLC_DIR) install DESTDIR=$(PWD)/UBUNTU bindir=/bin libdir=/lib/postgresql pkglibdi=/lib/postgresql datadir=/share/postgresql
	dpkg-deb --build UBUNTU $@

%.gppkg: $(MAIN_PKG) $(DEPENDENT_PKGS)
	echo "SHELL $(SHELL), pwd=`pwd`, ls=`ls`"
	mkdir -p gppkg/deps 
	cp gppkg_spec.yml gppkg/
	cp $(MAIN_PKG) gppkg/ 
ifdef DEPENDENT_PKGS
	for dep_rpm in $(DEPENDENT_PKGS); do \
		cp $${dep_pkg} gppkg/deps; \
	done
endif
	source $(GPHOME)/greenplum_path.sh && gppkg --build gppkg 
	rm -rf gppkg

clean:
	rm -rf UBUNTU
	rm -rf gppkg
	rm -f gppkg_spec.yml
ifdef EXTRA_CLEAN
	rm -f $(EXTRA_CLEAN)
endif

install: $(TARGET_GPPKG)
	source $(INSTLOC)/greenplum_path.sh && gppkg -i $(TARGET_GPPKG)

.PHONY: install clean
