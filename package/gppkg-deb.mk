PGXS := $(shell pg_config --pgxs)
include $(PGXS)

GP_VERSION_NUM := $(GP_MAJORVERSION)

MAJOR_OS=ubuntu18
ARCH=$(shell uname -p)

ifeq ($(ARCH), x86_64)
ARCH=amd64
endif

DEB_ARGS=$(subst -, ,$*)
DEB_NAME=$(word 1,$(RPM_ARGS))
PWD=$(shell pwd)
TARGET_GPPKG=$(PLC_GPPKG)
CONTROL_NAME=plcontainer.control.in

.PHONY: pkg
pkg: $(TARGET_GPPKG)

%.deb:
	rm -rf UBUNTU
	mkdir UBUNTU/DEBIAN -p
	mkdir UBUNTU/share/postgresql/extension/ -p
	cat gppkg_spec.yml.in | sed "s/#arch/$(ARCH)/g" | sed "s/#os/$(MAJOR_OS)/g" | sed "s/#gpver/$(GP_VERSION_NUM)/g" | sed "s/#plcver/$(PLC_GPPKG_VER)/g" > gppkg_spec.yml
	cat $(PWD)/$(CONTROL_NAME) | sed -r "s|#version|$(PLC_GPPKG_VER)|" | sed -r "s|#arch|$(ARCH)|" > $(PWD)/UBUNTU/DEBIAN/control
	$(MAKE) -C $(PLC_DIR) install DESTDIR=$(PWD)/UBUNTU bindir=/bin libdir=/lib/postgresql pkglibdir=/lib/postgresql datadir=/share/postgresql
	dpkg-deb --build UBUNTU $@

%.gppkg: $(PLC_DEB) $(DEPENDENT_DEBS)
	echo "SHELL $(SHELL), pwd=`pwd`, ls=`ls`"
	mkdir -p gppkg/deps
	cp gppkg_spec.yml gppkg/
	cp $(PLC_DEB) gppkg/
ifdef DEPENDENT_PKGS
	for dep_rpm in $(DEPENDENT_DEBS); do \
		cp $${dep_pkg} gppkg/deps; \
	done
endif
	/bin/bash -c "source $(GPHOME)/greenplum_path.sh && gppkg --build gppkg"
	rm -rf gppkg

clean:
	rm -rf UBUNTU
	rm -rf gppkg
	rm -f gppkg_spec.yml
ifdef EXTRA_CLEAN
	rm -f $(EXTRA_CLEAN)
endif

install: $(TARGET_GPPKG)
	/bin/bash source -c "$(INSTLOC)/greenplum_path.sh && gppkg -i $(TARGET_GPPKG)"

.PHONY: install clean
