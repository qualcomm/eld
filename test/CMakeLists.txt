set(LLVM_SOURCE_DIR "${LLVM_MAIN_SRC_DIR}")
if(ELD_USE_EXTERNAL_LLVM)
  # Tests use the LLVM object root (%llvmobjroot) as the build tree holding
  # ld.eld, libLW and test programs. With an external LLVM, this is the ELD
  # build tree; the LLVM installation is typically not writable.
  set(LLVM_BINARY_DIR "${ELD_BINARY_DIR}")
else()
  set(LLVM_BINARY_DIR "${LLVM_BINARY_DIR}")
endif()
set(LLVM_BUILD_MODE "%(build_mode)s")
set(LLVM_TOOLS_DIR "${LLVM_TOOLS_BINARY_DIR}/%(build_config)s")
set(LLVM_LIBS_DIR "${LLVM_BINARY_DIR}/lib/%(build_config)s")
set(TEST_TARGETS "${ELD_TARGETS_TO_BUILD}")
set(ELD_TESTSUITE)

# Directory of libLW and test plugins (%libsdir). With an external LLVM, this
# is not the LLVM library directory.
set(ELD_LIBS_BINARY_DIR "")
if(ELD_USE_EXTERNAL_LLVM)
  get_target_property(ELD_LIBS_BINARY_DIR LW LIBRARY_OUTPUT_DIRECTORY)
  if(NOT ELD_LIBS_BINARY_DIR)
    set(ELD_LIBS_BINARY_DIR "")
  endif()
endif()

# Directory of ld.eld in the build tree. With an external LLVM, this is not
# the LLVM tools directory.
get_target_property(ELD_TOOLS_BINARY_DIR ld.eld RUNTIME_OUTPUT_DIRECTORY)
if(NOT ELD_TOOLS_BINARY_DIR)
  set(ELD_TOOLS_BINARY_DIR "")
endif()

if(NOT Python3_EXECUTABLE)
  find_package(Python3 REQUIRED COMPONENTS Interpreter)
endif()
if(NOT PYTHON_EXECUTABLE)
  set(PYTHON_EXECUTABLE "${Python3_EXECUTABLE}")
endif()

include(LITModules)

set(OPTIONS "default")
set(OPTIONNAMES "default")

foreach(args ${ELD_TESTS_TO_RUN})
  string(REPLACE "," ";" ELD_TESTS_TO_RUN_OPTIONS_STR "${args}")
  list(GET ELD_TESTS_TO_RUN_OPTIONS_STR 0 OPTIONNAMES_STR)
  list(GET ELD_TESTS_TO_RUN_OPTIONS_STR 1 OPTIONS_STR)
  set(OPTIONS "${OPTIONS};${OPTIONS_STR}")
  set(OPTIONNAMES "${OPTIONNAMES};${OPTIONNAMES_STR}")
endforeach()

list(FIND TEST_TARGETS "RISCV" test_riscv)
if(test_riscv GREATER -1)
  set(TEST_TARGETS "${TEST_TARGETS};RISCV64")
endif()

list(FIND TEST_TARGETS "X86" test_x86)
if(test_x86 GREATER -1)
  set(TEST_TARGETS "${TEST_TARGETS};X86_32")
endif()

#Remove the Template backend from the testing infrastructure.
list(REMOVE_ITEM TEST_TARGETS "Template")

add_custom_target(PluginUnitTests)
set_target_properties(PluginUnitTests PROPERTIES FOLDER
  "plugin unit tests")

get_property(ELD_PLUGIN_TEST_TARGETS GLOBAL PROPERTY ELD_PLUGIN_TEST_TARGETS)
if(ELD_PLUGIN_TEST_TARGETS)
  add_dependencies(PluginUnitTests ${ELD_PLUGIN_TEST_TARGETS})
endif()

function(add_plugin pluginname)
  add_dependencies(PluginUnitTests ${pluginname})
endfunction()

option(USE_LLVM_UTILS_FROM_INSTALL "Use LLVM tools from installation" OFF)
if (NOT USE_LLVM_UTILS_FROM_INSTALL OR ELD_USE_EXTERNAL_LLVM)
  set(LLVM_UTILS_DEPEND
      llvm-addr2line
      llvm-ar
      llvm-as
      llvm-dwarfdump
      llvm-mc
      llvm-nm
      llvm-nm
      llvm-objcopy
      llvm-objdump
      llvm-readelf
      llvm-readobj
      llvm-strings
      llvm-strip
    )
endif()

# Set the depends list as a variable so that it can grow conditionally. NOTE:
# Sync the substitutions in test/lit.cfg when adding to this list.
set(LLVM_TEST_TOOLS
    llvm-config
    count
    FileCheck
    not
    obj2yaml
    yaml2obj
    split-file
    ${LLVM_UTILS_DEPEND})
if(ELD_USE_EXTERNAL_LLVM)
  # The LLVM tools are not built along with ELD; check that they are
  # installed instead of depending on them.
  foreach(tool IN LISTS LLVM_TEST_TOOLS)
    if(NOT EXISTS "${LLVM_TOOLS_BINARY_DIR}/${tool}${CMAKE_EXECUTABLE_SUFFIX}")
      message(WARNING "${tool} not found in ${LLVM_TOOLS_BINARY_DIR}, tests \
using it will fail")
    endif()
  endforeach()
  set(LLVM_TEST_TOOLS)
endif()

set(TEST_DEPENDS
    ${LLVM_TEST_TOOLS}
    ld.eld
    ${ELD_DEPENDS}
    ELDUnitTests
    ELDExpectedUsage
    LSParserVerifier
    PluginUnitTests)

set(TARGET_NAME check-eld)
add_custom_target(${TARGET_NAME})

process_options_and_generate_lit_test_suite("${TARGET_NAME}" "${TEST_TARGETS}"
                                            "${OPTIONS}" "${OPTIONNAMES}")

# Collect one JSON results path per arch+option configuration so that
# check-eld-summary can aggregate counts into a single table.
set(SUMMARY_JSON_FILES)
foreach(ARCH ${TEST_TARGETS})
  set(OPTIONCOUNT 0)
  foreach(OPTION ${OPTIONS})
    list(GET OPTIONNAMES ${OPTIONCOUNT} OPTIONNAME)
    list(APPEND SUMMARY_JSON_FILES
      "${CMAKE_CURRENT_BINARY_DIR}/${ARCH}-${OPTIONNAME}-config/results.json")
    math(EXPR OPTIONCOUNT "${OPTIONCOUNT}+1")
  endforeach()
endforeach()

add_custom_target(check-eld-summary
  COMMAND ${Python3_EXECUTABLE}
          ${CMAKE_CURRENT_SOURCE_DIR}/eld-lit-summary.py
          ${SUMMARY_JSON_FILES}
  COMMENT "Summarizing ELD lit test results across all configurations"
  USES_TERMINAL)
set_target_properties(check-eld-summary PROPERTIES FOLDER "Linker Tests")

# Extended test target: builds and runs everything check-eld does, plus a
# per-arch lit suite with --use-old-rule-matching appended to %linkopts.
set(EXTENDED_TARGET_NAME check-eld-extended)
add_custom_target(${EXTENDED_TARGET_NAME})
add_dependencies(${EXTENDED_TARGET_NAME} ${TARGET_NAME})

process_options_and_generate_lit_test_suite(
  "${EXTENDED_TARGET_NAME}" "${TEST_TARGETS}"
  "--use-old-rule-matching" "old_rule_matching")

add_subdirectory(UnitTests)
add_subdirectory(Common)
add_subdirectory(Hexagon)

# Setup a legacy alias for 'check-eld'. This will likely change to be an alias
# for 'check-all' at some point in the future.
set_target_properties(check-eld PROPERTIES FOLDER "Linker Tests")
set_target_properties(check-eld-extended PROPERTIES FOLDER "Linker Tests")
if(USE_LINKER_ALT_NAME)
  string(TOLOWER ${USE_LINKER_ALT_NAME} linker_alt_name_lower)
  if(linker_alt_name_lower MATCHES "ld.([a-z]+)*")
    set(linker_alt_name_suffix ${CMAKE_MATCH_1})
    info("Registering alternate check: check-${linker_alt_name_suffix}")
    add_custom_target(check-${linker_alt_name_suffix} DEPENDS check-eld)
  else()
    error("linker alt name ${USE_LINKER_ALT_NAME} should always start with ld")
    message(
      FATAL_ERROR
        "linker alt name ${USE_LINKER_ALT_NAME} should always start with ld")
  endif()
endif()
