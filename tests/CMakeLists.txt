include(${CMAKE_SOURCE_DIR}/cmake/Regress.cmake)

RegressTarget_Add(testpy
    INIT_FILE
    ${CMAKE_CURRENT_SOURCE_DIR}/init_file
    SQL_DIR ${CMAKE_CURRENT_SOURCE_DIR}/regress/sql
    EXPECTED_DIR ${CMAKE_CURRENT_SOURCE_DIR}/regress/expected
    RESULTS_DIR ${CMAKE_CURRENT_SOURCE_DIR}/regress/results
    DATA_DIR ${CMAKE_CURRENT_SOURCE_DIR}/regress/data
    SCHEDULE_FILE ${CMAKE_CURRENT_SOURCE_DIR}/regress/pl_schedule_py
    REGRESS_OPTS
    --dbname=${TEST_DB_NAME}
    REGRESS_ENV PL_TESTDB=${TEST_DB_NAME})

RegressTarget_Add(testpy2
    INIT_FILE
    ${CMAKE_CURRENT_SOURCE_DIR}/init_file
    SQL_DIR ${CMAKE_CURRENT_SOURCE_DIR}/regress/sql
    EXPECTED_DIR ${CMAKE_CURRENT_SOURCE_DIR}/regress/expected
    RESULTS_DIR ${CMAKE_CURRENT_SOURCE_DIR}/regress/results
    DATA_DIR ${CMAKE_CURRENT_SOURCE_DIR}/regress/data
    SCHEDULE_FILE ${CMAKE_CURRENT_SOURCE_DIR}/regress/pl_schedule_py2
    REGRESS_OPTS
    --dbname=${TEST_DB_NAME}
    REGRESS_ENV PL_TESTDB=${TEST_DB_NAME})

RegressTarget_Add(testr
    INIT_FILE
    ${CMAKE_CURRENT_SOURCE_DIR}/init_file
    SQL_DIR ${CMAKE_CURRENT_SOURCE_DIR}/regress/sql
    EXPECTED_DIR ${CMAKE_CURRENT_SOURCE_DIR}/regress/expected
    RESULTS_DIR ${CMAKE_CURRENT_SOURCE_DIR}/regress/results
    DATA_DIR ${CMAKE_CURRENT_SOURCE_DIR}/regress/data
    SCHEDULE_FILE ${CMAKE_CURRENT_SOURCE_DIR}/regress/pl_schedule_r
    REGRESS_OPTS
    --dbname=${TEST_DB_NAME}
    REGRESS_ENV PL_TESTDB=${TEST_DB_NAME})

add_custom_target(installcheck)
add_dependencies(installcheck testpy2 testpy testr)

# for _b means for data-science-bundle include the packages and import tests
if(CONTAINER_NAME_SUFFIX_PYTHON MATCHES ".*_b")
    RegressTarget_Add(testpy_bundle
        INIT_FILE
        ${CMAKE_CURRENT_SOURCE_DIR}/init_file
        SQL_DIR ${CMAKE_CURRENT_SOURCE_DIR}/regress/sql
        EXPECTED_DIR ${CMAKE_CURRENT_SOURCE_DIR}/regress/expected
        RESULTS_DIR ${CMAKE_CURRENT_SOURCE_DIR}/regress/results
        DATA_DIR ${CMAKE_CURRENT_SOURCE_DIR}/regress/data
        SCHEDULE_FILE ${CMAKE_CURRENT_SOURCE_DIR}/regress/pl_schedule_py_bundle
        REGRESS_OPTS
        --dbname=${TEST_DB_NAME}
        REGRESS_ENV PL_TESTDB=${TEST_DB_NAME})
    add_dependencies(installcheck testpy_bundle)
endif()

add_custom_target(prepare_runtime
    COMMAND
    CONTAINER_NAME_SUFFIX_PYTHON=${CONTAINER_NAME_SUFFIX_PYTHON}
    CONTAINER_NAME_SUFFIX_PYTHON2=${CONTAINER_NAME_SUFFIX_PYTHON2}
    CONTAINER_NAME_SUFFIX_R=${CONTAINER_NAME_SUFFIX_R}
    PL_TESTDB=${TEST_DB_NAME}
    ${CMAKE_CURRENT_SOURCE_DIR}/prepare_runtime.sh)
