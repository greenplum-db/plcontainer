cmake_minimum_required(VERSION 3.18)
project(rclient)

find_package(PkgConfig REQUIRED)
pkg_check_modules(R REQUIRED libR)

# the same logic with entry.sh

set(target_os_info "$ENV{HOSTNAME}")

if(EXISTS "/usr/lib/os-release")
    set(os_release_file_path "/usr/lib/os-release")
elseif(EXISTS "/etc/os-release")
    set(os_release_file_path "/etc/os-release")
else()
    set(os_release_file_path "")
endif()

if(NOT "${os_release_file_path}" STREQUAL "")
  file(STRINGS "${os_release_file_path}" os_release REGEX "^(ID|BUILD_ID|VERSION_ID)=")
  foreach(_var ${os_release})
    if("${_var}" MATCHES "^(ID)=(.*)$")
      set(os_release_id "${CMAKE_MATCH_2}")
    elseif("${_var}" MATCHES "^(BUILD_ID)=(.*)$")
      set(os_release_build_id "${CMAKE_MATCH_2}")
    elseif("${_var}" MATCHES "^(VERSION_ID)=(.*)$")
      string(REGEX REPLACE "\"" "" CMAKE_MATCH_2 ${CMAKE_MATCH_2})
      set(os_release_version_id "${CMAKE_MATCH_2}")
    endif()
  endforeach(_var ${os_release})

  if(NOT "${os_release_id}" STREQUAL "")
    set(target_os_info "${os_release_id}")
  endif()
  if(NOT "${os_release_build_id}" STREQUAL "")
    set(target_os_info "${target_os_info}-${os_release_build_id}")
  elseif(NOT "${os_release_version_id}" STREQUAL "")
    set(target_os_info "${target_os_info}-${os_release_version_id}")
  else()
    set(target_os_info "${target_os_info}-$ENV{HOSTNAME}")
  endif()
endif()

exec_program(Rscript ARGS --vanilla -e \"strsplit\(version[['version.string']], ' '\)[[1]][3]\" OUTPUT_VARIABLE R_VERSION)
string(REGEX REPLACE "\\[1\\] " "" R_VERSION ${R_VERSION})
string(REGEX REPLACE "\"" "" R_VERSION ${R_VERSION})
string(REGEX REPLACE "\"" "" R_VERSION ${R_VERSION})
set(target_name "client_r${R_VERSION}_${target_os_info}")

file(GLOB client_SRC ./common/*.c client.c)
file(GLOB share_SRC rcall.c rconversions.c rlogging.c)

add_library(${target_name}_rcall SHARED ${share_SRC}) # this file will load by R runtime
add_executable(${target_name} ${client_SRC})

target_include_directories(${target_name} PRIVATE
    ${PG_INCLUDE_DIR_SERVER}
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${R_INCLUDE_DIRS})
target_include_directories(${target_name}_rcall PRIVATE
    ${PG_INCLUDE_DIR_SERVER}
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${R_INCLUDE_DIRS})
target_compile_definitions(${target_name} PRIVATE PLC_CLIENT)
target_compile_definitions(${target_name}_rcall PRIVATE PLC_CLIENT)
target_link_directories(${target_name} PRIVATE ${R_LIBRARY_DIRS})
target_link_libraries(${target_name} PRIVATE ${target_name}_rcall ${R_LIBRARIES}) # R is GPL. link to R dynamically
