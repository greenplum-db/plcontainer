include(${CMAKE_SOURCE_DIR}/cmake/Regress.cmake)

RegressTarget_Add(testpy3
    INIT_FILE
    ${CMAKE_CURRENT_SOURCE_DIR}/init_file
    SQL_DIR ${CMAKE_CURRENT_SOURCE_DIR}/regress/sql
    EXPECTED_DIR ${CMAKE_CURRENT_SOURCE_DIR}/regress/expected
    RESULTS_DIR ${CMAKE_CURRENT_SOURCE_DIR}/regress/results
    DATA_DIR ${CMAKE_CURRENT_SOURCE_DIR}/data
    SCHEDULE_FILE ${CMAKE_CURRENT_SOURCE_DIR}/regress/pl_schedule_py3
    REGRESS_OPTS
    --dbname=contrib_regression)

RegressTarget_Add(testr
    INIT_FILE
    ${CMAKE_CURRENT_SOURCE_DIR}/init_file
    SQL_DIR ${CMAKE_CURRENT_SOURCE_DIR}/regress/sql
    EXPECTED_DIR ${CMAKE_CURRENT_SOURCE_DIR}/regress/expected
    RESULTS_DIR ${CMAKE_CURRENT_SOURCE_DIR}/regress/results
    DATA_DIR ${CMAKE_CURRENT_SOURCE_DIR}/data
    SCHEDULE_FILE ${CMAKE_CURRENT_SOURCE_DIR}/regress/pl_schedule_r
    REGRESS_OPTS
    --dbname=contrib_regression)

add_custom_target(installcheck)
add_dependencies(installcheck testpy3 testr)

if(CONTAINER_NAME_SUFFIX_PYTHON MATCHES ".*_b")
    RegressTarget_Add(testpy3_bundle
        INIT_FILE
        ${CMAKE_CURRENT_SOURCE_DIR}/init_file
        SQL_DIR ${CMAKE_CURRENT_SOURCE_DIR}/regress/sql
        EXPECTED_DIR ${CMAKE_CURRENT_SOURCE_DIR}/regress/expected
        RESULTS_DIR ${CMAKE_CURRENT_SOURCE_DIR}/regress/results
        DATA_DIR ${CMAKE_CURRENT_SOURCE_DIR}/data
        SCHEDULE_FILE ${CMAKE_CURRENT_SOURCE_DIR}/regress/pl_schedule_py3_bundle
        REGRESS_OPTS
        --dbname=contrib_regression)
    add_dependencies(installcheck testpy3_bundle)
endif()
