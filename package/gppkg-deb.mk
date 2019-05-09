PGXS := $(shell pg_config --pgxs)
include $(PGXS)

GP_VERSION_NUM := $(GP_MAJORVERSION)

MAJOR_OS=ubuntu18
ARCH=$(shell uname -p)

DEB_ARGS=$(subst -, ,$*)
DEB_NAME=$(word 1,$(RPM_ARGS))
PWD=$(shell pwd)
%.deb: 
	rm -rf UBUNTU
	mkdir UBUNTU/DEBIAN -p
	mkdir UBUNTU/$(PKG_PREFIX) -p
	cp "$(CONTROL_FILE)" UBUNTU/DEBIAN/control"
	tar xzf "$(TARGZ) -C UBUNTU/$(PKG_PREFIX)
	dpkg-deb --build UBUNTU $@

gppkg_spec.yml: gppkg_spec.yml.in
	cat $< | sed "s/#arch/$(ARCH)/g" | sed "s/#os/rhel$(MAJOR_OS)/g" | sed 's/#gpver/$(GP_VERSION_NUM)/g' | sed "s/#plcver/$(PLC_GPPKG_VER)/g"> $@

%.gppkg: gppkg_spec.yml $(MAIN_PKG) $(DEPENDENT_PKGS)
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
