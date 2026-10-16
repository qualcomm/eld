# Common setup for building ELD plugins against an installed ELD package.
# This file is included only by the standalone Plugins/CMakeLists.txt path.

function(eld_standalone_prepare_compiler)
  if(NOT DEFINED ELD_PACKAGE_ROOT OR ELD_PACKAGE_ROOT STREQUAL "")
    message(FATAL_ERROR
      "ELD_PACKAGE_ROOT must point at an installed ELD package")
  endif()

  # This function runs before project(). CMake performs compiler
  # identification and ABI/link tests inside project(), so the libc++ option
  # must be placed in CMAKE_CXX_FLAGS before project() rather than added later
  # by eld_standalone_setup().
  option(ELD_PLUGIN_USE_LIBCXX
    "Link standalone plugins with package libc++" ON)
  if(ELD_PLUGIN_USE_LIBCXX
      AND NOT CMAKE_CXX_FLAGS MATCHES
        "(^|[ ]+)-stdlib=libc\\+\\+([ ]|$)")
    if(CMAKE_CXX_FLAGS)
      set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -stdlib=libc++"
        CACHE STRING "CXX compiler flags" FORCE)
    else()
      set(CMAKE_CXX_FLAGS_INIT "${CMAKE_CXX_FLAGS_INIT} -stdlib=libc++"
        PARENT_SCOPE)
    endif()
  endif()

  if(NOT DEFINED ELD_PLUGIN_CXX OR ELD_PLUGIN_CXX STREQUAL "")
    set(ELD_PLUGIN_CXX "clang++" CACHE FILEPATH
      "Host C++ compiler used to build plugin shared libraries")
  endif()

  # CMake selects the compiler during project(). Set it before project() so
  # callers can use the convenient -DELD_PLUGIN_CXX=... form.
  if(NOT DEFINED CMAKE_CXX_COMPILER)
    set(CMAKE_CXX_COMPILER "${ELD_PLUGIN_CXX}" CACHE FILEPATH
      "Host C++ compiler used to build plugin shared libraries" FORCE)
  endif()
endfunction()

function(eld_standalone_setup)
  set(ELD_PACKAGE_ROOT "${ELD_PACKAGE_ROOT}" CACHE PATH
    "Root of the installed ELD package" FORCE)
  set(ELD_PLUGIN_TOOLS_DIR "${ELD_PACKAGE_ROOT}/Tools" CACHE PATH
    "Tools directory in the installed ELD package" FORCE)
  set(ELD_PLUGIN_INCLUDE_DIR
    "${ELD_PLUGIN_TOOLS_DIR}/include/ELD/PluginAPI" CACHE PATH
    "ELD plugin API include directory" FORCE)
  set(ELD_PLUGIN_INCLUDE_ROOT "${ELD_PLUGIN_TOOLS_DIR}/include" CACHE PATH
    "ELD package include directory" FORCE)
  set(ELD_PLUGIN_LIBRARY_DIR "${ELD_PLUGIN_TOOLS_DIR}/lib" CACHE PATH
    "ELD package library directory" FORCE)

  set(ELD_TARGET_CXX "" CACHE FILEPATH
    "Target C/C++ compiler used by standalone plugin tests")
  if(ELD_TARGET_CXX STREQUAL "")
    find_program(_eld_target_cxx
      NAMES clang++ hexagon-clang++ hexagon-clang clang
      PATHS "${ELD_PLUGIN_TOOLS_DIR}/bin" NO_DEFAULT_PATH)
    if(_eld_target_cxx)
      set(ELD_TARGET_CXX "${_eld_target_cxx}" CACHE FILEPATH
        "Target C/C++ compiler used by standalone plugin tests" FORCE)
    endif()
  endif()
  set(ELD_TARGET_LD "${ELD_PLUGIN_TOOLS_DIR}/bin/ld.eld" CACHE FILEPATH
    "Target ELD linker used by standalone plugin tests")
  set(ELD_TARGET_TRIPLE "" CACHE STRING
    "Optional target triple passed to the standalone test compiler")
  set(ELD_TARGET_CXX_FLAGS "" CACHE STRING
    "Additional target compiler flags used by standalone tests")
  set(ELD_TARGET_LD_FLAGS "" CACHE STRING
    "Additional target linker flags used by standalone tests")
  set(ELD_TARGET_CC "" CACHE FILEPATH
    "Target C compiler used by standalone plugin lit tests")
  if(ELD_TARGET_CC STREQUAL "" AND ELD_TARGET_CXX)
    get_filename_component(_eld_target_cxx_dir "${ELD_TARGET_CXX}" DIRECTORY)
    get_filename_component(_eld_target_cxx_name "${ELD_TARGET_CXX}" NAME)
    string(REGEX REPLACE "\\+\\+$" "" _eld_target_cc_name
      "${_eld_target_cxx_name}")
    if(NOT _eld_target_cc_name STREQUAL _eld_target_cxx_name)
      find_program(_eld_target_cc
        NAMES "${_eld_target_cc_name}"
        PATHS "${_eld_target_cxx_dir}" NO_DEFAULT_PATH)
      if(_eld_target_cc)
        set(ELD_TARGET_CC "${_eld_target_cc}" CACHE FILEPATH
          "Target C compiler used by standalone plugin lit tests" FORCE)
      endif()
    endif()
  endif()
  set(ELD_TEST_TARGET "" CACHE STRING
    "Override the architecture name used by standalone lit tests")
  option(ELD_ENABLE_LIT_TESTS
    "Register plugin lit tests in standalone builds" OFF)
  set(ELD_LLVM_LIT "" CACHE FILEPATH
    "External llvm-lit executable used by standalone plugin tests")
  set(ELD_LLVM_TOOLS_DIR "" CACHE PATH
    "LLVM tools directory used by standalone plugin lit tests")
  set(ELD_LLVM_LIBS_DIR "" CACHE PATH
    "LLVM libraries directory used by standalone plugin lit tests")
  foreach(_required
      "${ELD_PLUGIN_INCLUDE_DIR}/LinkerPlugin.h"
      "${ELD_PLUGIN_INCLUDE_DIR}/OutputSectionIteratorPlugin.h"
      "${ELD_PLUGIN_INCLUDE_DIR}/PluginADT.h"
      "${ELD_PLUGIN_INCLUDE_DIR}/Defines.h"
      "${ELD_PLUGIN_TOOLS_DIR}/lib/libLW.so.4")
    if(NOT EXISTS "${_required}")
      message(FATAL_ERROR "ELD package is missing required file: ${_required}")
    endif()
  endforeach()

  find_package(ZLIB REQUIRED)

  if(NOT TARGET ELD::LW)
    add_library(ELD::LW SHARED IMPORTED GLOBAL)
    set_target_properties(ELD::LW PROPERTIES
      IMPORTED_LOCATION "${ELD_PLUGIN_LIBRARY_DIR}/libLW.so.4"
      INTERFACE_INCLUDE_DIRECTORIES "${ELD_PLUGIN_INCLUDE_DIR}"
      INTERFACE_LINK_DIRECTORIES "${ELD_PLUGIN_LIBRARY_DIR}")
  endif()

  set(CMAKE_CXX_STANDARD 17 PARENT_SCOPE)
  set(CMAKE_CXX_STANDARD_REQUIRED YES PARENT_SCOPE)
  set(CMAKE_POSITION_INDEPENDENT_CODE ON PARENT_SCOPE)
endfunction()

function(eld_add_standalone_plugin target)
  cmake_parse_arguments(PLUGIN "" "" "SOURCES;LIBRARIES" ${ARGN})
  if(PLUGIN_UNPARSED_ARGUMENTS)
    message(FATAL_ERROR
      "Unknown arguments for eld_add_standalone_plugin(${target}): "
      "${PLUGIN_UNPARSED_ARGUMENTS}")
  endif()
  if(NOT PLUGIN_SOURCES)
    message(FATAL_ERROR
      "eld_add_standalone_plugin(${target}) requires SOURCES")
  endif()

  add_library("${target}" SHARED ${PLUGIN_SOURCES})
  target_include_directories("${target}" PRIVATE
    "${ELD_PLUGIN_INCLUDE_DIR}" "${ELD_PLUGIN_INCLUDE_ROOT}")
  target_link_libraries("${target}" PRIVATE ELD::LW ${PLUGIN_LIBRARIES})
  if(UNIX AND NOT APPLE)
    target_link_options("${target}" PRIVATE -Wl,-z,defs)
  endif()
  set_target_properties("${target}" PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED YES
    POSITION_INDEPENDENT_CODE YES
    BUILD_RPATH "${ELD_PLUGIN_LIBRARY_DIR}"
    INSTALL_RPATH "${ELD_PLUGIN_LIBRARY_DIR}")
endfunction()

function(eld_standalone_require_api plugin header symbol)
  if(NOT ELD_STANDALONE)
    return()
  endif()
  set(_header "${ELD_PLUGIN_INCLUDE_DIR}/${header}")
  if(NOT EXISTS "${_header}")
    message(FATAL_ERROR
      "${plugin} requires ELD API header '${header}', but it is missing "
      "from ${ELD_PLUGIN_INCLUDE_DIR}")
  endif()
  file(READ "${_header}" _header_contents)
  if(NOT _header_contents MATCHES "${symbol}")
    message(FATAL_ERROR
      "${plugin} requires ELD API '${symbol}' in '${header}', but the "
      "installed ELD package does not provide it")
  endif()
endfunction()

function(eld_standalone_resolve_test_target)
  set(_eld_test_target "${ELD_TEST_TARGET}")
  if(_eld_test_target STREQUAL "")
    string(TOLOWER "${ELD_TARGET_TRIPLE}" _eld_target_triple)
    if(_eld_target_triple MATCHES "^hexagon([-.]|$)")
      set(_eld_test_target Hexagon)
    elseif(_eld_target_triple MATCHES "^(riscv64|rv64)([-.]|$)")
      set(_eld_test_target RISCV64)
    elseif(_eld_target_triple MATCHES "^(riscv32|rv32)([-.]|$)")
      set(_eld_test_target RISCV)
    elseif(_eld_target_triple MATCHES "^(aarch64|arm64)([-.]|$)")
      set(_eld_test_target AArch64)
    elseif(_eld_target_triple MATCHES "^(arm|thumb)([-.]|$)")
      set(_eld_test_target ARM)
    elseif(_eld_target_triple MATCHES "^(x86_64|amd64|i[3-6]86)([-.]|$)")
      set(_eld_test_target X86)
    endif()
  endif()

  if(_eld_test_target STREQUAL "")
    if(ELD_ENABLE_LIT_TESTS)
      message(STATUS
        "Could not derive standalone lit architecture from "
        "ELD_TARGET_TRIPLE='${ELD_TARGET_TRIPLE}'; set ELD_TEST_TARGET to "
        "override it")
    endif()
    unset(ELD_TEST_TARGET_RESOLVED CACHE)
    unset(ELD_TEST_TARGET_CONFIG CACHE)
    return()
  endif()

  # Accept either the architecture name (RISCV64) or a complete lit target
  # name (RISCV64_default) as an override.
  string(REGEX REPLACE "_.*$" "" _eld_test_arch "${_eld_test_target}")
  set(ELD_TEST_TARGET_RESOLVED "${_eld_test_arch}" CACHE INTERNAL
    "Resolved standalone lit architecture" FORCE)
  set(ELD_TEST_TARGET_CONFIG "${_eld_test_arch}_default" CACHE INTERNAL
    "Resolved standalone lit target configuration" FORCE)
  message(STATUS "Standalone lit architecture: ${ELD_TEST_TARGET_RESOLVED}"
    " (${ELD_TEST_TARGET_CONFIG})")
endfunction()

function(eld_standalone_setup_tests)
  if(ELD_ENABLE_LIT_TESTS AND NOT BUILD_TESTING)
    message(FATAL_ERROR
      "ELD_ENABLE_LIT_TESTS requires BUILD_TESTING=ON")
  endif()
  if(NOT BUILD_TESTING)
    return()
  endif()

  find_package(Python3 COMPONENTS Interpreter QUIET)
  if(NOT Python3_Interpreter_FOUND)
    if(ELD_ENABLE_LIT_TESTS)
      message(FATAL_ERROR
        "ELD_ENABLE_LIT_TESTS requires a Python interpreter")
    endif()
    message(STATUS
      "Python not found; standalone functional tests are unavailable")
    return()
  endif()

  if(NOT ELD_TARGET_CXX OR NOT EXISTS "${ELD_TARGET_CXX}")
    message(FATAL_ERROR
      "Standalone tests require ELD_TARGET_CXX; set it to the target "
      "compiler for the selected architecture")
  endif()
  if(NOT EXISTS "${ELD_TARGET_LD}")
    message(FATAL_ERROR
      "Standalone tests require ELD_TARGET_LD; set it to the target ELD "
      "linker for the selected architecture")
  endif()

  set(ELD_STANDALONE_PYTHON "${Python3_EXECUTABLE}" CACHE INTERNAL "")

  if(ELD_ENABLE_LIT_TESTS)
    if(NOT ELD_TEST_TARGET_RESOLVED)
      message(FATAL_ERROR
        "ELD_ENABLE_LIT_TESTS could not determine the test architecture; "
        "set ELD_TARGET_TRIPLE or ELD_TEST_TARGET")
    endif()

    if(ELD_LLVM_LIT STREQUAL "")
      find_program(_eld_llvm_lit NAMES llvm-lit llvm-lit.py)
      if(_eld_llvm_lit)
        set(ELD_LLVM_LIT "${_eld_llvm_lit}" CACHE FILEPATH
          "External llvm-lit executable used by standalone plugin tests"
          FORCE)
      endif()
    endif()
    if(NOT ELD_LLVM_LIT OR NOT EXISTS "${ELD_LLVM_LIT}")
      message(FATAL_ERROR
        "ELD_ENABLE_LIT_TESTS requires ELD_LLVM_LIT; set it to an external "
        "llvm-lit executable")
    endif()

    get_filename_component(_eld_llvm_lit_dir "${ELD_LLVM_LIT}" DIRECTORY)
    if(ELD_LLVM_TOOLS_DIR STREQUAL "")
      set(ELD_LLVM_TOOLS_DIR "${_eld_llvm_lit_dir}" CACHE PATH
        "LLVM tools directory used by standalone plugin lit tests" FORCE)
    endif()
    if(ELD_LLVM_LIBS_DIR STREQUAL "")
      get_filename_component(_eld_llvm_root "${ELD_LLVM_TOOLS_DIR}"
        DIRECTORY)
      set(ELD_LLVM_LIBS_DIR "${_eld_llvm_root}/lib" CACHE PATH
        "LLVM libraries directory used by standalone plugin lit tests" FORCE)
    endif()

    set(_eld_required_lit_tools FileCheck not llvm-readelf)
    foreach(_tool IN LISTS _eld_required_lit_tools)
      find_program(_eld_lit_tool_path NAMES "${_tool}"
        PATHS "${ELD_LLVM_TOOLS_DIR}" NO_DEFAULT_PATH)
      if(NOT _eld_lit_tool_path)
        message(FATAL_ERROR
          "ELD_ENABLE_LIT_TESTS requires '${_tool}' in "
          "ELD_LLVM_TOOLS_DIR=${ELD_LLVM_TOOLS_DIR}")
      endif()
      unset(_eld_lit_tool_path)
      unset(_eld_lit_tool_path CACHE)
    endforeach()
    set(ELD_STANDALONE_LIT_ENABLED ON CACHE INTERNAL "")
  endif()
endfunction()

function(eld_add_standalone_test)
  cmake_parse_arguments(TEST "" "NAME;TARGET;SCRIPT" "" ${ARGN})
  if(NOT BUILD_TESTING OR NOT ELD_STANDALONE_PYTHON)
    return()
  endif()

  add_test(NAME "ELDStandalone.${TEST_NAME}"
    COMMAND "${ELD_STANDALONE_PYTHON}"
      "${TEST_SCRIPT}"
      --plugin-dir "$<TARGET_FILE_DIR:${TEST_TARGET}>"
      --tools-dir "${ELD_PLUGIN_TOOLS_DIR}"
      --target-cc "${ELD_TARGET_CC}"
      --target-cxx "${ELD_TARGET_CXX}"
      --target-ld "${ELD_TARGET_LD}"
      --target-triple "${ELD_TARGET_TRIPLE}"
      # Use the --option=value spelling so a flag value that begins with '-'
      # (for example, -G0 for Hexagon) is not interpreted as another Python
      # argparse option.
      "--target-cxx-flags=${ELD_TARGET_CXX_FLAGS}"
      "--target-ld-flags=${ELD_TARGET_LD_FLAGS}")
endfunction()

function(eld_add_standalone_lit_test)
  cmake_parse_arguments(TEST "" "NAME;TEST_DIR" "" ${ARGN})
  if(NOT ELD_STANDALONE_LIT_ENABLED)
    return()
  endif()

  set(_eld_lit_params
    "-Deld_obj_root=${CMAKE_BINARY_DIR}"
    "-Deld_tools_dir=${ELD_PLUGIN_TOOLS_DIR}"
    "-Dplugin_library_dir=${CMAKE_CURRENT_BINARY_DIR}"
    "-Dllvm_tools_dir=${ELD_LLVM_TOOLS_DIR}"
    "-Dllvm_libs_dir=${ELD_LLVM_LIBS_DIR}"
    # Let Plugins/lit.cfg derive the full <arch>_default name from the
    # target triple. A non-empty ELD_TEST_TARGET is passed through as the
    # explicit override.
    "-Deld_test_target=${ELD_TEST_TARGET}"
    "-Dtarget_triple=${ELD_TARGET_TRIPLE}"
    "-Dtarget_cc=${ELD_TARGET_CC}"
    "-Dtarget_cxx=${ELD_TARGET_CXX}"
    "-Dtarget_ld=${ELD_TARGET_LD}"
    "-Dtarget_cxx_flags=${ELD_TARGET_CXX_FLAGS}"
    "-Dtarget_ld_flags=${ELD_TARGET_LD_FLAGS}"
    "-Dpython_executable=${ELD_STANDALONE_PYTHON}"
    "-Dhas_zlib=1")

  set(_eld_lit_paths
    --path "${ELD_PLUGIN_TOOLS_DIR}/bin"
    --path "${ELD_LLVM_TOOLS_DIR}")
  get_filename_component(_eld_target_cxx_dir "${ELD_TARGET_CXX}" DIRECTORY)
  get_filename_component(_eld_target_ld_dir "${ELD_TARGET_LD}" DIRECTORY)
  list(APPEND _eld_lit_paths
    --path "${_eld_target_cxx_dir}"
    --path "${_eld_target_ld_dir}")
  if(ELD_TARGET_CC)
    get_filename_component(_eld_target_cc_dir "${ELD_TARGET_CC}" DIRECTORY)
    list(APPEND _eld_lit_paths --path "${_eld_target_cc_dir}")
  endif()
  add_test(NAME "ELDStandaloneLit.${TEST_NAME}"
    COMMAND "${ELD_LLVM_LIT}"
      -q
      ${_eld_lit_paths}
      ${_eld_lit_params}
      "${TEST_TEST_DIR}")

  set(_eld_lit_ld_path
    "${CMAKE_CURRENT_BINARY_DIR}:${ELD_PLUGIN_LIBRARY_DIR}:${ELD_LLVM_LIBS_DIR}")
  if(DEFINED ENV{LD_LIBRARY_PATH} AND NOT "$ENV{LD_LIBRARY_PATH}" STREQUAL "")
    set(_eld_lit_ld_path
      "${_eld_lit_ld_path}:$ENV{LD_LIBRARY_PATH}")
  endif()
  set_tests_properties("ELDStandaloneLit.${TEST_NAME}"
    PROPERTIES ENVIRONMENT "LD_LIBRARY_PATH=${_eld_lit_ld_path}")
endfunction()
