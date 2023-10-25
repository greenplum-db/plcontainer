cmake_minimum_required(VERSION 3.18)
project(pyclient)
find_package(Python COMPONENTS Interpreter Development)

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

file(GLOB pyclient_SRC ./common/*.c *.c)

set(target_name "client_python${Python_VERSION_MAJOR}${Python_VERSION_MINOR}_${target_os_info}")

add_executable(${target_name} ${pyclient_SRC})

target_include_directories(${target_name} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${Python_INCLUDE_DIRS})
target_link_libraries(${target_name} PRIVATE ${Python_LIBRARIES})
target_compile_definitions(${target_name} PRIVATE PLC_CLIENT)
