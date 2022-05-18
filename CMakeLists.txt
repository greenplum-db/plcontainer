cmake_minimum_required(VERSION 3.18)

project(plcontainer)

set(DOWNLOAD_DIR
    ${CMAKE_CURRENT_SOURCE_DIR}/downloads
    CACHE STRING "Directory for downloading external project")

# generate 'compile_commands.json'
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(deps_INSTALL_DIR ${CMAKE_CURRENT_BINARY_DIR}/external_deps)

include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/Gpdb.cmake)

include(${CMAKE_ROOT}/Modules/ExternalProject.cmake)
ExternalProject_Add(
    libxml
    URL
    https://github.com/GNOME/libxml2/archive/refs/tags/v2.9.14.tar.gz
    URL_MD5
    801d9101ff91e258c02c74d2166323c3
    INSTALL_DIR
    ${deps_INSTALL_DIR}
    CMAKE_ARGS
    -D BUILD_SHARED_LIBS=OFF
    -D CMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE}
    -D CMAKE_INSTALL_PREFIX=${deps_INSTALL_DIR}
    -D LIBXML2_WITH_ICONV=OFF
    -D LIBXML2_WITH_LZMA=OFF
    -D LIBXML2_WITH_PYTHON=OFF
    -D LIBXML2_WITH_ZLIB=OFF)

file(GLOB plcontainer_SRC src/*.c src/common/*.c)

add_library(plcontainer MODULE ${plcontainer_SRC})
set_target_properties(plcontainer
    PROPERTIES
    OUTPUT_NAME "plcontainer"
    PREFIX ""
    C_STANDARD 99)
target_include_directories(plcontainer PRIVATE
    ${PG_INCLUDE_DIR_SERVER}
    ${deps_INSTALL_DIR}/include/libxml2)
