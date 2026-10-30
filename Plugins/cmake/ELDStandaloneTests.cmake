# Test dependencies are separate from package discovery and library creation.
# This module is included only for standalone builds with BUILD_TESTING=ON.

find_package(Python3 REQUIRED COMPONENTS Interpreter)

if(CMAKE_HOST_SYSTEM_NAME STREQUAL "Linux")
  add_test(NAME ELDStandalone.RuntimeConfiguration
    COMMAND "${CMAKE_COMMAND}"
      "-DRUNTIME_MODULE=${CMAKE_CURRENT_LIST_DIR}/ELDStandalone.cmake"
      "-DTEST_ROOT=${CMAKE_CURRENT_BINARY_DIR}/runtime-config-test"
      -P "${CMAKE_CURRENT_LIST_DIR}/tests/PackageRuntime.cmake")
endif()

find_program(ELD_TARGET_CXX
  NAMES clang++ hexagon-clang++ clang hexagon-clang
  PATHS "${ELD_TOOLS_DIR}/bin" NO_DEFAULT_PATH
  DOC "Target compiler used by standalone plugin tests")
find_program(ELD_TARGET_LD NAMES ld.eld
  PATHS "${ELD_TOOLS_DIR}/bin" NO_DEFAULT_PATH
  DOC "Target ELD linker used by standalone plugin tests")

# Prefer the C driver adjacent to the selected C++ compiler, including .exe
# installations. An explicit ELD_TARGET_CC always takes precedence.
if(ELD_TARGET_CXX)
  get_filename_component(_eld_cxx_dir "${ELD_TARGET_CXX}" DIRECTORY)
  get_filename_component(_eld_cxx_name "${ELD_TARGET_CXX}" NAME)
  string(REGEX REPLACE "[+][+][.]exe$" ".exe"
    _eld_cc_name "${_eld_cxx_name}")
  string(REGEX REPLACE "[+][+]$" "" _eld_cc_name "${_eld_cc_name}")
  find_program(ELD_TARGET_CC NAMES "${_eld_cc_name}" clang hexagon-clang
    PATHS "${_eld_cxx_dir}" NO_DEFAULT_PATH
    DOC "Target C compiler used by standalone plugin tests")
  if(NOT ELD_TARGET_CC)
    set(ELD_TARGET_CC "${ELD_TARGET_CXX}" CACHE FILEPATH
      "Target C compiler used by standalone plugin tests" FORCE)
  endif()
elseif(ELD_TARGET_CC)
  set(ELD_TARGET_CXX "${ELD_TARGET_CC}" CACHE FILEPATH
    "Target compiler used by standalone plugin tests" FORCE)
endif()
foreach(_var ELD_TARGET_CC ELD_TARGET_CXX ELD_TARGET_LD)
  if(NOT ${_var} OR NOT EXISTS "${${_var}}" OR IS_DIRECTORY "${${_var}}")
    message(FATAL_ERROR
      "BUILD_TESTING requires ${_var}; set it to a target tool executable "
      "or use BUILD_TESTING=OFF to build libraries only")
  endif()
endforeach()

set(ELD_TARGET_TRIPLE "" CACHE STRING
  "Target triple passed to the standalone test compiler")
set(ELD_TARGET_CXX_FLAGS "" CACHE STRING
  "Additional flags for both target compiler drivers")
set(ELD_TARGET_LD_FLAGS "" CACHE STRING "Additional target linker flags")
set(ELD_TEST_TARGET "" CACHE STRING
  "Override the lit architecture derived from ELD_TARGET_TRIPLE")

set(ELD_RUNTIME_DIR "${ELD_LIBRARY_DIR}")
if(ELD_PACKAGE_CXX_RUNTIME_DIR)
  set(ELD_RUNTIME_DIR "${ELD_PACKAGE_CXX_RUNTIME_DIR}")
endif()
if(WIN32)
  get_filename_component(ELD_RUNTIME_DIR "${ELD_RUNTIME_LIBRARY}" DIRECTORY)
endif()

if(ELD_ENABLE_LIT_TESTS)
  if(NOT ELD_TARGET_TRIPLE AND NOT ELD_TEST_TARGET)
    message(FATAL_ERROR
      "ELD_ENABLE_LIT_TESTS requires ELD_TARGET_TRIPLE or ELD_TEST_TARGET")
  endif()
  find_program(ELD_LLVM_LIT NAMES llvm-lit llvm-lit.py lit
    HINTS "${ELD_TOOLS_DIR}/bin"
    DOC "External lit runner for standalone plugin tests")
  if(NOT ELD_LLVM_LIT OR NOT EXISTS "${ELD_LLVM_LIT}")
    message(FATAL_ERROR "ELD_ENABLE_LIT_TESTS requires ELD_LLVM_LIT")
  endif()
  set(ELD_LIT_COMMAND "${Python3_EXECUTABLE}" "${ELD_LLVM_LIT}")
  if(WIN32 AND ELD_LLVM_LIT MATCHES "[.](exe|cmd|bat)$")
    set(ELD_LIT_COMMAND "${ELD_LLVM_LIT}")
  endif()
  get_filename_component(_eld_lit_dir "${ELD_LLVM_LIT}" DIRECTORY)
  set(ELD_LLVM_TOOLS_DIR "${_eld_lit_dir}" CACHE PATH
    "LLVM tools directory used by standalone plugin lit tests")
  get_filename_component(_eld_llvm_root "${ELD_LLVM_TOOLS_DIR}" DIRECTORY)
  set(ELD_LLVM_LIBS_DIR "${_eld_llvm_root}/lib" CACHE PATH
    "LLVM libraries directory used by standalone plugin lit tests")
  foreach(_tool FileCheck not)
    find_program(_eld_lit_tool NAMES "${_tool}"
      PATHS "${ELD_LLVM_TOOLS_DIR}" NO_DEFAULT_PATH)
    if(NOT _eld_lit_tool)
      message(FATAL_ERROR
        "ELD_ENABLE_LIT_TESTS requires '${_tool}' in "
        "ELD_LLVM_TOOLS_DIR=${ELD_LLVM_TOOLS_DIR}")
    endif()
    unset(_eld_lit_tool)
    unset(_eld_lit_tool CACHE)
  endforeach()
endif()

function(eld_add_standalone_test)
  cmake_parse_arguments(TEST "" "NAME;TARGET;SCRIPT" "" ${ARGN})
  if(TEST_UNPARSED_ARGUMENTS OR NOT TEST_NAME OR NOT TEST_SCRIPT
      OR NOT TARGET "${TEST_TARGET}")
    message(FATAL_ERROR
      "eld_add_standalone_test requires NAME, TARGET, and SCRIPT")
  endif()
  if(CMAKE_SYSTEM_NAME STREQUAL "Linux" AND CMAKE_READELF)
    add_test(NAME "ELDStandaloneRuntime.${TEST_NAME}"
      COMMAND "${CMAKE_COMMAND}"
        "-DPLUGIN_LIBRARY=$<TARGET_FILE:${TEST_TARGET}>"
        "-DREADELF=${CMAKE_READELF}"
        -P "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/tests/NoRuntimePath.cmake")
  endif()
  add_test(NAME "ELDStandalone.${TEST_NAME}"
    COMMAND "${Python3_EXECUTABLE}" "${TEST_SCRIPT}"
      --plugin-dir "$<TARGET_FILE_DIR:${TEST_TARGET}>"
      --tools-dir "${ELD_TOOLS_DIR}"
      --library-dir "${ELD_LIBRARY_DIR}"
      --runtime-dir "${ELD_RUNTIME_DIR}"
      --target-cc "${ELD_TARGET_CC}"
      --target-cxx "${ELD_TARGET_CXX}"
      --target-ld "${ELD_TARGET_LD}"
      --target-triple "${ELD_TARGET_TRIPLE}"
      # = keeps dash-leading flags from being parsed as Python options.
      "--target-cxx-flags=${ELD_TARGET_CXX_FLAGS}"
      "--target-ld-flags=${ELD_TARGET_LD_FLAGS}")
endfunction()

function(eld_add_standalone_lit_test)
  if(NOT ELD_ENABLE_LIT_TESTS)
    return()
  endif()
  cmake_parse_arguments(TEST "" "NAME;TARGET;TEST_DIR" "" ${ARGN})
  if(TEST_UNPARSED_ARGUMENTS OR NOT TEST_NAME OR NOT TEST_TEST_DIR
      OR NOT TARGET "${TEST_TARGET}")
    message(FATAL_ERROR
      "eld_add_standalone_lit_test requires NAME, TARGET, and TEST_DIR")
  endif()
  # lit.cfg prepares the host runtime search paths, including custom library
  # locations. Use the target directory for multi-configuration generators.
  add_test(NAME "ELDStandaloneLit.${TEST_NAME}"
    COMMAND ${ELD_LIT_COMMAND} -v
      "-Deld_obj_root=${CMAKE_BINARY_DIR}"
      "-Deld_tools_dir=${ELD_TOOLS_DIR}"
      "-Deld_library_dir=${ELD_LIBRARY_DIR}"
      "-Deld_runtime_dir=${ELD_RUNTIME_DIR}"
      "-Dplugin_library_dir=$<TARGET_FILE_DIR:${TEST_TARGET}>"
      "-Dllvm_tools_dir=${ELD_LLVM_TOOLS_DIR}"
      "-Dllvm_libs_dir=${ELD_LLVM_LIBS_DIR}"
      "-Deld_test_target=${ELD_TEST_TARGET}"
      "-Dtarget_triple=${ELD_TARGET_TRIPLE}"
      "-Dtarget_cc=${ELD_TARGET_CC}"
      "-Dtarget_cxx=${ELD_TARGET_CXX}"
      "-Dtarget_ld=${ELD_TARGET_LD}"
      "-Dtarget_cxx_flags=${ELD_TARGET_CXX_FLAGS}"
      "-Dtarget_ld_flags=${ELD_TARGET_LD_FLAGS}"
      "-Dpython_executable=${Python3_EXECUTABLE}"
      "${TEST_TEST_DIR}")
endfunction()
