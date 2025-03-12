include(ColorMsg)

set(LLVM_UTILS_DIR "${LLVM_MAIN_SRC_DIR}/utils")
if(NOT EXISTS "${LLVM_UTILS_DIR}/llvm-lit/llvm-lit.in")
  error("llvm-lit.in not found at ${LLVM_UTILS_DIR}/llvm-lit/llvm-lit.in, \
     couldn't create target specific llvm-lit")
  message(FATAL_ERROR "llvm-lit.in not found")
endif()
set(LLVM_LIT_IN_SRC_FILE "${LLVM_UTILS_DIR}/llvm-lit/llvm-lit.in")

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
  set(LLVM_EXTERNAL_LIT
      ${CMAKE_CURRENT_BINARY_DIR}/llvm-lit-${ARCH_LOWER}-${OPTIONNAME})
  add_lit_testsuite(
    ${TARGET_NAME}-test-${ARCH_LOWER}-${OPTIONNAME}
    "Running the Linker regression tests ${ADDITIONAL_TESTMSG} for option: ${OPTION} arch: ${ARCH}"
    ${CMAKE_CURRENT_BINARY_DIR}/${ARCH}-${OPTIONNAME}-config
    DEPENDS
    ${TEST_DEPENDS})
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

function(add_eld_lit_cfg ARCH OPTION OPTIONNAME)
  set(ELD_CURRENT_TARGET "${ARCH}_${OPTIONNAME}")
  set(ELD_OPTION ${OPTION})
  set(ELD_OPTION_NAME ${OPTIONNAME})
  string(TOLOWER ${ARCH} ARCH_LOWER)
  set(CUSTOM_LIT_CFG
      "${CMAKE_CURRENT_BINARY_DIR}/${ARCH}-${OPTIONNAME}-config/lit.site.cfg")
  configure_lit_site_cfg(${CMAKE_CURRENT_SOURCE_DIR}/lit.site.cfg.in
                         ${CUSTOM_LIT_CFG})
  generate_ext_lit(${ARCH} ${OPTIONNAME} "${CUSTOM_LIT_CFG}")
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
