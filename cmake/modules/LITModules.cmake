include(ELDColorMsg)

set(LLVM_UTILS_DIR "${LLVM_MAIN_SRC_DIR}/utils")
set(LLVM_LIT_IN_SRC_FILE "${LLVM_UTILS_DIR}/llvm-lit/llvm-lit.in")
if(EXISTS "${LLVM_LIT_IN_SRC_FILE}")
  # Generate a target specific llvm-lit for each test configuration.
  set(ELD_GENERATE_LLVM_LIT ON)
elseif(ELD_USE_EXTERNAL_LLVM)
  # Without an LLVM source tree, run the test suites with an installed lit,
  # e.g. from the LLVM tools package or from "pip install lit".
  find_program(
    LLVM_EXTERNAL_LIT
    NAMES llvm-lit lit lit.py
    HINTS "${LLVM_TOOLS_BINARY_DIR}"
    DOC "Path to lit, used to run the tests")
  if(NOT LLVM_EXTERNAL_LIT)
    error("lit not found, set LLVM_EXTERNAL_LIT to the lit executable")
    message(FATAL_ERROR "lit not found")
  endif()
  info("Using lit: ${LLVM_EXTERNAL_LIT}")
  set(ELD_GENERATE_LLVM_LIT OFF)
else()
  error("llvm-lit.in not found at ${LLVM_LIT_IN_SRC_FILE}, \
     couldn't create target specific llvm-lit")
  message(FATAL_ERROR "llvm-lit.in not found")
endif()

option(LIT_VERBOSE "Set verbose output for Lit tests" OFF)

if(LIT_VERBOSE)
  set(LLVM_LIT_ARGS "-vv  ")
endif()

if(ELD_THREAD_SANITIZE)
  set(LLVM_LIT_ARGS "-j1 ${LLVM_LIT_ARGS} ")
endif()

set(ADDITIONAL_TESTMSG "")
if(LLVM_USE_SANITIZE)
  set(ADDITIONAL_TESTMSG "With Sanitizer = ${LLVM_USE_SANITIZE}")
endif()

set(PATHFUNC "def path(p):\n    return p\n")

function(add_external_eld_lit TARGET_NAME ARCH OPTION OPTIONNAME)
  string(TOLOWER ${ARCH} ARCH_LOWER)
  if(ELD_GENERATE_LLVM_LIT)
    set(LLVM_EXTERNAL_LIT
        ${CMAKE_CURRENT_BINARY_DIR}/llvm-lit-${ARCH_LOWER}-${OPTIONNAME})
  endif()
  set(JSON_OUTPUT
      "${CMAKE_CURRENT_BINARY_DIR}/${ARCH}-${OPTIONNAME}-config/results.json")

  get_property(ELD_PLUGIN_TEST_DIRS GLOBAL PROPERTY ELD_PLUGIN_TEST_DIRS)
  set(ELD_PLUGIN_LIT_PARAMS
      "eld_src_root=${ELD_SOURCE_DIR}"
      "eld_obj_root=${ELD_BINARY_DIR}"
      "llvm_src_root=${LLVM_SOURCE_DIR}"
      "llvm_obj_root=${LLVM_BINARY_DIR}"
      "llvm_tools_dir=${LLVM_TOOLS_BINARY_DIR}"
      "llvm_libs_dir=${LLVM_LIBRARY_DIR}"
      "eld_targets_to_build=${ARCH}_${OPTIONNAME}"
      "targets_to_build=${ARCH}_${OPTIONNAME}"
      "eld_test_target=${ARCH}"
      "target_triple="
      "python_executable=${Python3_EXECUTABLE}"
      "emulator=${ELD_EMULATOR}"
      "emulator64=${ELD_EMULATOR64}"
      "eld_option=${OPTION}"
      "eld_option_name=${OPTIONNAME}"
      "has_sanitize=${ELD_SANITIZE}"
      "has_tsan=${ELD_THREAD_SANITIZE}"
      "has_symbol_versioning=${ELD_ENABLE_SYMBOL_VERSIONING_TESTS}"
      "cpp_compiler=${CMAKE_CXX_COMPILER}"
      "cmake_executable=${CMAKE_COMMAND}"
      "enable_run_tests=${ELD_ENABLE_RUN_TESTS}"
      "has_zlib=${LLVM_ENABLE_ZLIB}")
  if(ELD_USE_EXTERNAL_LLVM)
    # ld.eld and libLW are not in the LLVM tools and library directories.
    list(APPEND ELD_PLUGIN_LIT_PARAMS "eld_tools_dir=${ELD_BINARY_DIR}")
  endif()
  add_lit_testsuite(
    ${TARGET_NAME}-test-${ARCH_LOWER}-${OPTIONNAME}
    "Running the Linker regression tests ${ADDITIONAL_TESTMSG} for option: ${OPTION} arch: ${ARCH}"
    ${CMAKE_CURRENT_BINARY_DIR}/${ARCH}-${OPTIONNAME}-config
    ${ELD_PLUGIN_TEST_DIRS}
    PARAMS ${ELD_PLUGIN_LIT_PARAMS}
    DEPENDS
    ${TEST_DEPENDS}
    ARGS --output ${JSON_OUTPUT})
  set_target_properties(${TARGET_NAME}-test-${ARCH_LOWER}-${OPTIONNAME}
                        PROPERTIES FOLDER "Tests")
  add_dependencies(${TARGET_NAME}
                   ${TARGET_NAME}-test-${ARCH_LOWER}-${OPTIONNAME})
endfunction()

function(generate_ext_lit ARCH OPTIONNAME LIT_CFG_PATH)
  # Populate all the variables that are needed to go in
  # llvm-lit-<arch>-<optionname>
  get_filename_component(ABS_LIT_CFG_PATH ${LIT_CFG_PATH} REALPATH)
  get_filename_component(ABS_CMAKE_SRC_LIT_CFG_PATH
                         ${CMAKE_CURRENT_SOURCE_DIR}/lit.cfg REALPATH)
  set(MAP "\nmap_config(r'${ABS_CMAKE_SRC_LIT_CFG_PATH}', \
      r'${ABS_LIT_CFG_PATH}')")
  set(LLVM_LIT_CONFIG_MAP "${PATHFUNC}${MAP}")
  # Generate target and option specific llvm-lit file
  trace(
    "Generating ${CMAKE_CURRENT_BINARY_DIR}/llvm-lit-${ARCH_LOWER}-${OPTIONNAME}"
  )
  configure_file(
    "${LLVM_LIT_IN_SRC_FILE}"
    "${CMAKE_CURRENT_BINARY_DIR}/llvm-lit-${ARCH_LOWER}-${OPTIONNAME}")
endfunction()

if(ELD_ENABLE_SYMBOL_VERSIONING)
  set(ELD_ENABLE_SYMBOL_VERSIONING_TESTS "true")
endif()

if (ELD_ENABLE_RUN_TESTS)
  set(ELD_ENABLE_RUN_TESTS "true")
endif()

function(add_eld_lit_cfg ARCH OPTION OPTIONNAME)
  set(ELD_CURRENT_TARGET "${ARCH}_${OPTIONNAME}")
  set(ELD_OPTION ${OPTION})
  set(ELD_OPTION_NAME ${OPTIONNAME})
  string(TOLOWER ${ARCH} ARCH_LOWER)
  set(CUSTOM_LIT_CFG
      "${CMAKE_CURRENT_BINARY_DIR}/${ARCH}-${OPTIONNAME}-config/lit.site.cfg")
  configure_lit_site_cfg(${CMAKE_CURRENT_SOURCE_DIR}/lit.site.cfg.in
                         ${CUSTOM_LIT_CFG})
  if(ELD_GENERATE_LLVM_LIT)
    generate_ext_lit(${ARCH} ${OPTIONNAME} "${CUSTOM_LIT_CFG}")
  endif()
endfunction()

function(process_options_and_generate_lit_test_suite TARGET_NAME TEST_TARGETS
         OPTIONS OPTIONNAMES)

  foreach(ARCH ${TEST_TARGETS})
    set(OPTIONCOUNT 0)
    foreach(OPTION ${OPTIONS})
      list(GET OPTIONNAMES ${OPTIONCOUNT} OPTIONNAME)
      add_eld_lit_cfg(${ARCH} ${OPTION} ${OPTIONNAME})
      add_external_eld_lit(${TARGET_NAME} ${ARCH} ${OPTION} ${OPTIONNAME})
      math(EXPR OPTIONCOUNT "${OPTIONCOUNT}+1")
    endforeach()
  endforeach()

endfunction()
