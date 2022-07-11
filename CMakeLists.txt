cmake_minimum_required(VERSION 3.18)

project(plcontainer)

include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/Gpdb.cmake)
include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/Distro.cmake)
include(${CMAKE_ROOT}/Modules/ExternalProject.cmake)
include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/Git.cmake)

GitAbbRev_Get(PLCONTAINER_GIT_VERSION)
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
# for gppkg
set(VERSION 0.0.0)
set(DOWNLOAD_DIR
    ${CMAKE_CURRENT_SOURCE_DIR}/downloads
    CACHE STRING "Directory for downloading external project")

# generate 'compile_commands.json'
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

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
    ${deps_INSTALL_DIR}/lib/libjson-c.a)
add_dependencies(
    plcontainer
    libxml
    json-c
)

# Build client
set(PYCLIENT_IMAGE python39.alpine)
add_custom_target(pyclient_image
    COMMAND
    PYTHONPATH=
    docker build .
    -t ${PYCLIENT_IMAGE}
    -f Dockerfile.${PYCLIENT_IMAGE}
    WORKING_DIRECTORY
    ${CMAKE_CURRENT_SOURCE_DIR}/dockerfiles)

set(RCLIENT_IMAGE r.alpine)
add_custom_target(rclient_image
    COMMAND
    PYTHONPATH=
    docker build .
    -t ${RCLIENT_IMAGE}
    -f Dockerfile.${RCLIENT_IMAGE}
    WORKING_DIRECTORY
    ${CMAKE_CURRENT_SOURCE_DIR}/dockerfiles)

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
    sh -c 'cmake /src/src/rclient && cmake --build . -v')

add_dependencies(rclient rclient_image)
add_custom_target(clients)
add_dependencies(clients pyclient rclient)

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
install(
    PROGRAMS src/pyclient/bin/py3client.sh
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

# # Packing
include(${CMAKE_CURRENT_SOURCE_DIR}/Pack.cmake)
