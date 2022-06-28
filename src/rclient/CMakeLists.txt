cmake_minimum_required(VERSION 3.18)
project(rclient)

find_package(PkgConfig REQUIRED)
pkg_check_modules(R REQUIRED libR)

file(GLOB client_SRC
    ../common/*.c
    client.c)
file(GLOB share_SRC
    rcall.c
    rconversions.c
    rlogging.c)

add_library(rcall SHARED ${share_SRC})
add_executable(rclient ${client_SRC})

target_include_directories(rcall PRIVATE
    ${PG_INCLUDE_DIR_SERVER}
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${R_INCLUDE_DIRS})
target_compile_definitions(rcall PRIVATE PLC_CLIENT)

target_include_directories(rclient PRIVATE
    ${PG_INCLUDE_DIR_SERVER}
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${R_INCLUDE_DIRS})
target_link_directories(rclient PRIVATE ${R_LIBRARY_DIRS})
target_link_libraries(rclient PRIVATE rcall ${R_LIBRARIES})
target_compile_definitions(rclient PRIVATE PLC_CLIENT)
