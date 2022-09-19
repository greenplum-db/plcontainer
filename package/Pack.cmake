# Set package information
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Greenplum-plcontainer")
set(CPACK_PACKAGE_DESCRIPTION "Provides PL/Container procedural language implementation for the Greenplum Database.")
set(CPACK_RPM_PACKAGE_LICENSE "VMware Software EULA")
set(CPACK_PACKAGE_CONTACT "VMware")

#set(CPACK_RPM_GPSSPKG_PACKAGE_AUTOREQPROV FALSE)
# cpack BUG! Below is not working
# See https://gitlab.kitware.com/cmake/cmake/-/issues/19244
# Workaround
set(CPACK_RPM_PACKAGE_AUTOREQPROV " no")
# %define _build_id_links none
# Don't add 'build-id' into the rpm.
# %global _python_bytecompile_errors_terminate_build 0
# %global __python %{_bindir}/python${PYTHON3_VERSION_SHORT}
# Solve rpm's quirk -- always want to build pyc when it see a python file.
# See https://git.centos.org/rpms/python3/blob/c7/f/SPECS/python3.spec#_155
set(CPACK_RPM_SPEC_MORE_DEFINE
"%define _build_id_links none
%global _python_bytecompile_errors_terminate_build 0
%global __python %{_bindir}/python${PYTHON3_VERSION_SHORT}"
)

include(CPack)

set(PACK_NAME ${CMAKE_PROJECT_NAME}-${VERSION}-${DISTRO_NAME}-${CMAKE_SYSTEM_PROCESSOR})
# expecting filename of form %{name}-%{version}-%{release}.%{arch}.rpm
# See gppkg's package.py
set(RPM_NAME ${CMAKE_PROJECT_NAME}-${VERSION}.${CMAKE_SYSTEM_PROCESSOR})
set(DEB_NAME ${CMAKE_PROJECT_NAME}-${VERSION}-${CMAKE_SYSTEM_PROCESSOR})
set(PACK_GPPKG_FILE_NAME gppython3)

configure_file(${CMAKE_CURRENT_SOURCE_DIR}/package/gppkg_spec.yml.in gppkg_spec.yml @ONLY)

add_custom_target(gppkg_rpm
    COMMAND
    cpack -G RPM
    -D CPACK_RPM_PACKAGE_VERSION=${VERSION}
    -D CPACK_RPM_FILE_NAME=${RPM_NAME}.rpm
    -D CPACK_RPM_PACKAGE_PROVIDES="/bin/sh" &&
    ${CMAKE_COMMAND} -E rm -rf gppkg_rpm &&
    ${CMAKE_COMMAND} -E make_directory gppkg_rpm &&
    ${CMAKE_COMMAND} -E copy gppkg_spec.yml ${RPM_NAME}.rpm gppkg_rpm &&
    ${PG_BIN_DIR}/gppkg --build gppkg_rpm)

add_custom_target(gppkg_deb
    COMMAND
    cpack -G DEB
    -D CPACK_PACKAGING_INSTALL_PREFIX="./"
    -D CPACK_DEB_PACKAGE_VERSION=${VERSION}
    -D CPACK_DEBIAN_FILE_NAME=${DEB_NAME}.deb
    ${CMAKE_COMMAND} -E rm -rf gppkg_deb &&
    ${CMAKE_COMMAND} -E make_directory gppkg_deb &&
    ${CMAKE_COMMAND} -E copy gppkg_spec.yml ${DEB_NAME}.deb gppkg_deb &&
    ${PG_BIN_DIR}/gppkg --build gppkg_deb)

# Build the specific package only for our supported platform
if(${DISTRO_NAME} MATCHES "rhel.*")
    add_custom_target(gppkg DEPENDS gppkg_rpm)
elseif(${DISTRO_NAME} MATCHES "ubuntu.*")
    add_custom_target(gppkg DEPENDS gppkg_deb)
else()
    add_custom_target(gppkg
        COMMAND
        ${CMAKE_COMMAND} -E echo "Cannot identify the gppkg type (rpm/deb). Please build the specific target (gppkg_rpm/gppkg_deb) directly." &&
        ${CMAKE_COMMAND} -E false
        VERBATIM)
endif()
add_custom_target(gppkg_artifact
    COMMAND
    ${CMAKE_COMMAND} -E tar czvf
    plcontainer-gppkg-${DISTRO_NAME}-gp${GP_MAJOR_VERSION}.tar.gz
    ${PACK_NAME}.gppkg
DEPENDS gppkg)
