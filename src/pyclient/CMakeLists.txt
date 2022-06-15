cmake_minimum_required(VERSION 3.18)
project(pyclient)

include(${CMAKE_CURRENT_SOURCE_DIR}/../../cmake/Gpdb.cmake)
find_package(Python COMPONENTS Interpreter Development)

file(GLOB pyclient_SRC ../common/*.c *.c)

add_executable(pyclient ${pyclient_SRC})

target_include_directories(pyclient PRIVATE
    ${PG_INCLUDE_DIR_SERVER}
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${Python_INCLUDE_DIRS})
