# CMakeLists.txt for ELD Vim syntax-highlighting files and tests.

option(ELD_VIM_TESTS "Enable Vim syntax-highlighting tests (requires vim > 9)" OFF)

if(ELD_VIM_TESTS)
  add_subdirectory(test)
endif()
