include(${CMAKE_SOURCE_DIR}/cmake/Regress.cmake)

if("$ENV{ARTIFACT_TYPE}" STREQUAL "Release")
    file(READ ${CMAKE_CURRENT_SOURCE_DIR}/tests_using_faultinjector.txt EXCLUDE_TESTS)
    string(STRIP ${EXCLUDE_TESTS} EXCLUDE_TESTS)
    list(APPEND REGRESS_OPTS "--exclude-tests=${EXCLUDE_TESTS}")
endif()

RegressTarget_Add(testpy
    INIT_FILE
    ${CMAKE_CURRENT_SOURCE_DIR}/init_file
    SQL_DIR ${CMAKE_CURRENT_SOURCE_DIR}/regress/sql
    EXPECTED_DIR ${CMAKE_CURRENT_SOURCE_DIR}/regress/expected
    RESULTS_DIR ${CMAKE_CURRENT_SOURCE_DIR}/regress/results
    DATA_DIR ${CMAKE_CURRENT_SOURCE_DIR}/regress/data
    SCHEDULE_FILE ${CMAKE_CURRENT_SOURCE_DIR}/regress/pl_schedule_py
    REGRESS_OPTS ${REGRESS_OPTS} --dbname=testpy
    REGRESS_ENV PL_TESTDB=testpy)

RegressTarget_Add(repeatpy
    RUN_TIMES -1
    INIT_FILE
    ${CMAKE_CURRENT_SOURCE_DIR}/init_file
    SQL_DIR ${CMAKE_CURRENT_SOURCE_DIR}/regress/sql
    EXPECTED_DIR ${CMAKE_CURRENT_SOURCE_DIR}/regress/expected
    RESULTS_DIR ${CMAKE_CURRENT_SOURCE_DIR}/regress/results
    DATA_DIR ${CMAKE_CURRENT_SOURCE_DIR}/regress/data
    SCHEDULE_FILE ${CMAKE_CURRENT_SOURCE_DIR}/regress/pl_schedule_py
    REGRESS_OPTS ${REGRESS_OPTS} --dbname=testpy
    REGRESS_ENV PL_TESTDB=testpy)

RegressTarget_Add(testpy2
    INIT_FILE
    ${CMAKE_CURRENT_SOURCE_DIR}/init_file
    SQL_DIR ${CMAKE_CURRENT_SOURCE_DIR}/regress/sql
    EXPECTED_DIR ${CMAKE_CURRENT_SOURCE_DIR}/regress/expected
    RESULTS_DIR ${CMAKE_CURRENT_SOURCE_DIR}/regress/results
    DATA_DIR ${CMAKE_CURRENT_SOURCE_DIR}/regress/data
    SCHEDULE_FILE ${CMAKE_CURRENT_SOURCE_DIR}/regress/pl_schedule_py2
    REGRESS_OPTS ${REGRESS_OPTS} --dbname=testpy2
    REGRESS_ENV PL_TESTDB=testpy2)

RegressTarget_Add(testr
    INIT_FILE
    ${CMAKE_CURRENT_SOURCE_DIR}/init_file
    SQL_DIR ${CMAKE_CURRENT_SOURCE_DIR}/regress/sql
    EXPECTED_DIR ${CMAKE_CURRENT_SOURCE_DIR}/regress/expected
    RESULTS_DIR ${CMAKE_CURRENT_SOURCE_DIR}/regress/results
    DATA_DIR ${CMAKE_CURRENT_SOURCE_DIR}/regress/data
    SCHEDULE_FILE ${CMAKE_CURRENT_SOURCE_DIR}/regress/pl_schedule_r
    REGRESS_OPTS ${REGRESS_OPTS} --dbname=testr
    REGRESS_ENV PL_TESTDB=testr)

add_custom_target(installcheck)

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
        REGRESS_OPTS ${REGRESS_OPTS} --dbname=testpy_bundle
        REGRESS_ENV PL_TESTDB=testpy_bundle)
    add_dependencies(installcheck testpy_bundle)
else()
    add_dependencies(installcheck testpy2 testpy testr)
endif()

add_custom_target(prepare_runtime
    COMMAND
    CONTAINER_NAME_SUFFIX_PYTHON=${CONTAINER_NAME_SUFFIX_PYTHON}
    CONTAINER_NAME_SUFFIX_PYTHON2=${CONTAINER_NAME_SUFFIX_PYTHON2}
    CONTAINER_NAME_SUFFIX_R=${CONTAINER_NAME_SUFFIX_R}
    ${CMAKE_CURRENT_SOURCE_DIR}/prepare_runtime.sh)
