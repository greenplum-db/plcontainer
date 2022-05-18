find_package(Git)

macro(GitHash_Get _git_hash)
  execute_process(
    COMMAND ${GIT_EXECUTABLE} log -1 --pretty=format:%h
    OUTPUT_VARIABLE ${_git_hash}
    OUTPUT_STRIP_TRAILING_WHITESPACE ERROR_QUIET
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})
endmacro()

macro(GitAbbRev_Get _git_abbrev)
  execute_process(
    COMMAND ${GIT_EXECUTABLE} describe --abbrev=0
    OUTPUT_VARIABLE ${_git_abbrev}
    OUTPUT_STRIP_TRAILING_WHITESPACE ERROR_QUIET
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})
endmacro()
