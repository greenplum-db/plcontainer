cmake_minimum_required(VERSION 3.18)

project(plcontainer)

include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/Gpdb.cmake)
include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/Distro.cmake)
include(${CMAKE_ROOT}/Modules/ExternalProject.cmake)
include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/Git.cmake)

# generate version
if(NOT DEFINED PLCONTAINER_VERSION)
  file(STRINGS VERSION PLCONTAINER_VERSION)
endif()
if(NOT DEFINED PYTHON_IMAGE_VERSION)
    set(PYTHON_IMAGE_VERSION ${PLCONTAINER_VERSION})
endif()
if(NOT DEFINED PYTHON2_IMAGE_VERSION)
    set(PYTHON2_IMAGE_VERSION ${PLCONTAINER_VERSION})
endif()
if(NOT DEFINED R_IMAGE_VERSION)
    set(R_IMAGE_VERSION ${PLCONTAINER_VERSION})
endif()

if(GP_MAJOR_VERSION VERSION_GREATER 6)
    set(LIBPQ ${PG_LIB_DIR}/libpq.so)
endif()

configure_file(
    "${PROJECT_SOURCE_DIR}/src/config.h.in"
    "${PROJECT_BINARY_DIR}/config.h"
)
configure_file(
    "${PROJECT_SOURCE_DIR}/src/config.h.in"
    "${PROJECT_SOURCE_DIR}/src/common/config.h"
)
execute_process(
    OUTPUT_VARIABLE my_UID
    COMMAND
    id -u $ENV{USER}
    OUTPUT_STRIP_TRAILING_WHITESPACE)

if(CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT)
    set(CMAKE_INSTALL_PREFIX
        "${PG_HOME}"
        CACHE PATH "default install prefix" FORCE)
endif()

set(VERSION ${PLCONTAINER_VERSION})
set(DOWNLOAD_DIR
    ${CMAKE_CURRENT_SOURCE_DIR}/downloads
    CACHE STRING "Directory for downloading external project")

# generate 'compile_commands.json'
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CONTAINER_NAME_SUFFIX_PYTHON "" CACHE STRING "The container python image env for plcontainer")
set(CONTAINER_NAME_SUFFIX_PYTHON2 "" CACHE STRING "The container python2 image env for plcontainer")
set(CONTAINER_NAME_SUFFIX_R "" CACHE STRING "The container r image env for plcontainer")

if("${CONTAINER_NAME_SUFFIX_PYTHON}" STREQUAL "")
    set(CONTAINER_NAME_SUFFIX_PYTHON "python39.alpine")
endif()

if("${CONTAINER_NAME_SUFFIX_PYTHON2}" STREQUAL "")
    set(CONTAINER_NAME_SUFFIX_PYTHON2 "python27.ubuntu")
endif()

if("${CONTAINER_NAME_SUFFIX_R}" STREQUAL "")
    set(CONTAINER_NAME_SUFFIX_R "r.alpine")
endif()

# Build dependencies
set(deps_INSTALL_DIR ${CMAKE_CURRENT_BINARY_DIR}/external_deps)
ExternalProject_Add(
    libxml
    URL
    https://github.com/GNOME/libxml2/archive/refs/tags/v2.9.14.tar.gz
    URL_MD5
    801d9101ff91e258c02c74d2166323c3
    DOWNLOAD_DIR
    ${DOWNLOAD_DIR}
    INSTALL_DIR
    ${deps_INSTALL_DIR}
    CMAKE_ARGS
    -D BUILD_SHARED_LIBS=OFF
    -D CMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE}
    -D CMAKE_INSTALL_PREFIX=${deps_INSTALL_DIR}
    -D LIBXML2_WITH_ICONV=OFF
    -D LIBXML2_WITH_LZMA=OFF
    -D LIBXML2_WITH_PYTHON=OFF
    -D LIBXML2_WITH_ZLIB=OFF
    -D CMAKE_INSTALL_LIBDIR=lib
)

ExternalProject_Add(
    json-c
    URL
    https://github.com/json-c/json-c/archive/refs/tags/json-c-0.16-20220414.tar.gz
    URL_MD5
    4f3288a5f14e0e6abe914213f41234e0
    DOWNLOAD_DIR
    ${DOWNLOAD_DIR}
    INSTALL_DIR
    ${deps_INSTALL_DIR}
    CMAKE_ARGS
    -D BUILD_SHARED_LIBS=OFF
    -D BUILD_STATIC_LIBS=ON
    -D CMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE}
    -D CMAKE_INSTALL_PREFIX=${deps_INSTALL_DIR}
    -D DISABLE_EXTRA_LIBS=ON
    -D CMAKE_INSTALL_LIBDIR=lib
)

# Build plcontainer extension
file(GLOB plcontainer_SRC src/*.c src/common/*.c)
add_library(plcontainer MODULE ${plcontainer_SRC})
set_target_properties(plcontainer
    PROPERTIES
    OUTPUT_NAME "plcontainer"
    PREFIX ""
    C_STANDARD 99)
target_include_directories(plcontainer PRIVATE
    ${PG_INCLUDE_DIR}
    ${PG_INCLUDE_DIR_SERVER}
    ${PROJECT_BINARY_DIR}
    ${deps_INSTALL_DIR}/include
    ${deps_INSTALL_DIR}/include/libxml2)
target_link_libraries(plcontainer PRIVATE
    ${deps_INSTALL_DIR}/lib/libxml2.a
    ${deps_INSTALL_DIR}/lib/libjson-c.a
    ${LIBPQ})
add_dependencies(
    plcontainer
    libxml
    json-c
)

# Build client
set(PY_DOCKERFILES_DIR ${CMAKE_CURRENT_SOURCE_DIR}/dockerfiles)
set(R_DOCKERFILES_DIR ${CMAKE_CURRENT_SOURCE_DIR}/dockerfiles)

if(CONTAINER_NAME_SUFFIX_PYTHON MATCHES "python.*_b")
    string(REGEX MATCH "python\([0-9]*\).*" _ ${CONTAINER_NAME_SUFFIX_PYTHON})
    set(_ver_PY ${CMAKE_MATCH_1})
    file(STRINGS dockerfiles/gp-data-science-bundle/dockerfiles/python/VERSION_py${_ver_PY}_image PYTHON_IMAGE_VERSION)
    string(REGEX MATCH "\(.*\)-.*_.*" _ ${PYTHON_IMAGE_VERSION})
    set(PYTHON_IMAGE_VERSION ${CMAKE_MATCH_1})
    set(PY_DOCKERFILES_DIR ${CMAKE_CURRENT_SOURCE_DIR}/dockerfiles/gp-data-science-bundle/dockerfiles/python)
endif()
if(CONTAINER_NAME_SUFFIX_R MATCHES "r.*_b")
    file(STRINGS dockerfiles/gp-data-science-bundle/dockerfiles/r/VERSION_r_image R_IMAGE_VERSION)
    string(REGEX MATCH "\(.*\)-.*_.*" _ ${R_IMAGE_VERSION})
    set(R_IMAGE_VERSION ${CMAKE_MATCH_1})
    set(R_DOCKERFILES_DIR ${CMAKE_CURRENT_SOURCE_DIR}/dockerfiles/gp-data-science-bundle/dockerfiles/r)
endif()
set(PYCLIENT_IMAGE ${CONTAINER_NAME_SUFFIX_PYTHON})
# python2.7
set(PY2CLIENT_IMAGE ${CONTAINER_NAME_SUFFIX_PYTHON2})
set(RCLIENT_IMAGE ${CONTAINER_NAME_SUFFIX_R})

# Conventionally, docker build command will send the context directory
# (typically the current one) to the docker daemon, but without dereferencing
# the symbolic links in the directory.
#
# To enable the context directory to contain symbolic links, we use tar -ch to
# create a tarball with symbolic links dereferenced. Afterwards, we pipe the 
# tarball to the docker build command. In this way, the docker daemon actually
# receives a set of files with links dereferenced.
add_custom_target(rclient_image
    COMMAND
    tar -ch . |
    docker build
    -t ${RCLIENT_IMAGE}
    -f Dockerfile.${RCLIENT_IMAGE}
    -
    WORKING_DIRECTORY
    ${R_DOCKERFILES_DIR})

add_custom_target(pyclient_image
    COMMAND
    tar -ch . |
    docker build
    -t ${PYCLIENT_IMAGE}
    -f Dockerfile.${PYCLIENT_IMAGE}
    -
    WORKING_DIRECTORY
    ${PY_DOCKERFILES_DIR})

# py2 client image docker file hard code for now.
add_custom_target(py2client_image
    COMMAND
    PYTHONPATH=
    docker build .
    -t ${PY2CLIENT_IMAGE}
    -f Dockerfile.${PY2CLIENT_IMAGE}
    WORKING_DIRECTORY
    ${CMAKE_CURRENT_SOURCE_DIR}/dockerfiles)

# tarball name
set(R_IMAGE_TARBALL_NAME
    plcontainer-r-image-${R_IMAGE_VERSION}-gp${GP_MAJOR_VERSION}.tar.gz)
set(PYTHON_IMAGE_TARBALL_NAME
    plcontainer-python3-image-${PYTHON_IMAGE_VERSION}-gp${GP_MAJOR_VERSION}.tar.gz)
set(PYTHON2_IMAGE_TARBALL_NAME
    plcontainer-python2-image-${PYTHON_IMAGE_VERSION}-gp${GP_MAJOR_VERSION}.tar.gz)

add_custom_target(rclient_image_tarball
    COMMAND
    PYTHONPATH=
    docker save ${RCLIENT_IMAGE} | gzip > ${R_IMAGE_TARBALL_NAME}
    DEPENDS rclient_image)

add_custom_target(pyclient_image_tarball
    COMMAND
    PYTHONPATH=
    docker save ${PYCLIENT_IMAGE} | gzip > ${PYTHON_IMAGE_TARBALL_NAME}
    DEPENDS pyclient_image)
add_custom_target(py2client_image_tarball
    COMMAND
    PYTHONPATH=
    docker save ${PY2CLIENT_IMAGE} | gzip > ${PYTHON2_IMAGE_TARBALL_NAME}
    DEPENDS py2client_image)

# for version-less tarball which will be uploaded to intermediate bucket
add_custom_target(rclient_image_artifact
    COMMAND
    ${CMAKE_COMMAND} -E tar czvf
    plcontainer-r-image-gp${GP_MAJOR_VERSION}.tgz
    ${R_IMAGE_TARBALL_NAME}
    DEPENDS rclient_image_tarball)

add_custom_target(pyclient_image_artifact
    COMMAND
    ${CMAKE_COMMAND} -E tar czvf
    plcontainer-python3-image-gp${GP_MAJOR_VERSION}.tgz
    ${PYTHON_IMAGE_TARBALL_NAME}
    DEPENDS pyclient_image_tarball)

add_custom_target(py2client_image_artifact
    COMMAND
    ${CMAKE_COMMAND} -E tar czvf
    plcontainer-python2-image-gp${GP_MAJOR_VERSION}.tgz
    ${PYTHON2_IMAGE_TARBALL_NAME}
    DEPENDS py2client_image_tarball)

add_custom_target(pyclient
    ${CMAKE_COMMAND} -E make_directory ${CMAKE_CURRENT_BINARY_DIR}/pyclient
    COMMAND
    PYTHONPATH=
    docker run
    --entrypoint=
    -v ${CMAKE_CURRENT_BINARY_DIR}/pyclient:/build
    -v ${CMAKE_CURRENT_SOURCE_DIR}:/src
    -w /build
    -u ${my_UID}
    -it ${PYCLIENT_IMAGE}
    sh -c 'cmake /src/src/pyclient && cmake --build .')
add_dependencies(pyclient pyclient_image)

# client for python2 will hardcode for the container becauesd python2 end of life
# we will not support it in the future
add_custom_target(py2client
    ${CMAKE_COMMAND} -E make_directory ${CMAKE_CURRENT_BINARY_DIR}/py2client
    COMMAND
    PYTHONPATH=
    docker run
    --entrypoint=
    -v ${CMAKE_CURRENT_BINARY_DIR}/py2client:/build
    -v ${CMAKE_CURRENT_SOURCE_DIR}:/src
    -w /build
    -u ${my_UID}
    -it ${PY2CLIENT_IMAGE}
    sh -c 'cmake /src/src/pyclient && cmake --build .')
add_dependencies(py2client py2client_image)

add_custom_target(rclient
    ${CMAKE_COMMAND} -E make_directory ${CMAKE_CURRENT_BINARY_DIR}/rclient
    COMMAND
    PYTHONPATH=
    docker run
    --entrypoint=
    -v ${CMAKE_CURRENT_BINARY_DIR}/rclient:/build
    -v ${CMAKE_CURRENT_SOURCE_DIR}:/src
    -w /build
    -u ${my_UID}
    -it ${RCLIENT_IMAGE}
    sh -c 'cmake /src/src/rclient && cmake --build .')

add_dependencies(rclient rclient_image)
add_custom_target(clients)
add_dependencies(clients pyclient py2client rclient)
add_custom_target(images_artifact)
add_dependencies(images_artifact
    pyclient_image_artifact
    py2client_image_artifact
    rclient_image_artifact)


# Install
install(
    TARGETS plcontainer
    DESTINATION "lib/postgresql")
install(
    FILES management/sql/plcontainer_gp--1.0.0.sql
    RENAME plcontainer--1.0.0.sql
    DESTINATION share/postgresql/extension)
install(
    FILES plcontainer.control
    DESTINATION share/postgresql/extension)
install(
    FILES management/config/plcontainer_configuration.xml
    DESTINATION share/postgresql/plcontainer)
install(
    PROGRAMS management/bin/plcontainer
    DESTINATION bin)

# Install clients
# user may only use pyclient or rclient so make the install optional
install(
    PROGRAMS ${CMAKE_CURRENT_BINARY_DIR}/pyclient/pyclient
    RENAME py3client
    DESTINATION bin/plcontainer_clients/ OPTIONAL)
# python2 client
install(
    PROGRAMS ${CMAKE_CURRENT_BINARY_DIR}/py2client/pyclient
    DESTINATION bin/plcontainer_clients/ OPTIONAL)
install(
    PROGRAMS src/pyclient/bin/py3client.sh
    DESTINATION bin/plcontainer_clients/ OPTIONAL)
install(
    PROGRAMS src/pyclient/bin/pyclient.sh
    DESTINATION bin/plcontainer_clients/ OPTIONAL)
install(
    PROGRAMS ${CMAKE_CURRENT_BINARY_DIR}/rclient/rclient
    DESTINATION bin/plcontainer_clients/ OPTIONAL)
install(
    PROGRAMS ${CMAKE_CURRENT_BINARY_DIR}/rclient/librcall.so
    DESTINATION bin/plcontainer_clients/ OPTIONAL)
install(
    PROGRAMS src/rclient/bin/rclient.sh
    DESTINATION bin/plcontainer_clients/ OPTIONAL)

# Add tests targets
add_subdirectory(tests)

# Packing
include(${CMAKE_CURRENT_SOURCE_DIR}/package/Pack.cmake)
