cmake_minimum_required(VERSION 3.18)
project(pyclient)

find_package(Python COMPONENTS Interpreter Development)

file(GLOB pyclient_SRC ../common/*.c *.c)

add_executable(pyclient ${pyclient_SRC})

target_include_directories(pyclient PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${Python_INCLUDE_DIRS})
target_link_libraries(pyclient PRIVATE ${Python_LIBRARIES})
target_compile_definitions(pyclient PRIVATE PLC_CLIENT)
