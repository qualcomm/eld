cmake_minimum_required(VERSION 3.20)

# The helper selects the parser using CMAKE_HOST_WIN32. Exercise both
# command-line syntaxes on each host without needing Windows executables.
set(CMAKE_HOST_WIN32 "${TEST_WINDOWS}")
include("${ARGUMENT_MODULE}")
eld_standalone_test_arguments(_arguments "${TEST_FLAGS}")
message("${_arguments}")
