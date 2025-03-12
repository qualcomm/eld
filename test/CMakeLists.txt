set(LLVM_SOURCE_DIR "${LLVM_MAIN_SRC_DIR}")
set(LLVM_BINARY_DIR "${LLVM_BINARY_DIR}")
set(LLVM_BUILD_MODE "%(build_mode)s")
set(LLVM_TOOLS_DIR "${LLVM_TOOLS_BINARY_DIR}/%(build_config)s")
set(LLVM_LIBS_DIR "${LLVM_BINARY_DIR}/lib/%(build_config)s")
set(TEST_TARGETS "${LLVM_TARGETS_TO_BUILD}")
set(ELD_TESTSUITE)

include(LITModules)

set(OPTIONS "default")
set(OPTIONNAMES "default")

foreach(args ${ELD_TESTS_TO_RUN})
  string(REPLACE "," ";" ELD_TESTS_TO_RUN_OPTIONS_STR "${args}")
  list (GET ELD_TESTS_TO_RUN_OPTIONS_STR 0 OPTIONNAMES_STR)
  list (GET ELD_TESTS_TO_RUN_OPTIONS_STR 1 OPTIONS_STR)
  set(OPTIONS "${OPTIONS};${OPTIONS_STR}")
  set(OPTIONNAMES "${OPTIONNAMES};${OPTIONNAMES_STR}")
endforeach()

list(FIND TEST_TARGETS "RISCV" test_riscv)
if(test_riscv GREATER -1)
  set(TEST_TARGETS "${TEST_TARGETS};RISCV64")
endif()

# Set the depends list as a variable so that it can grow conditionally. NOTE:
# Sync the substitutions in test/lit.cfg when adding to this list.
set(TEST_DEPENDS
    FileCheck
    not
    count
    llvm-nm
    ld.eld
    llvm-config
    llvm-mc
    obj2yaml
    yaml2obj
    llvm-objdump
    llvm-addr2line
    llvm-readobj
    llvm-readelf
    llvm-nm
    llvm-dwarfdump
    llvm-ar
    llvm-objcopy
    llvm-strip
    llvm-strings
    ELDUnitTests
    split-file
    ${ELD_DEPENDS}
    ELDExpectedUsage
    LSParserVerifier
    CommonPluginUnitTests)

set(TARGET_NAME check-eld)
add_custom_target(${TARGET_NAME})

process_options_and_generate_lit_test_suite("${TARGET_NAME}" "${TEST_TARGETS}" "${OPTIONS}"
                                            "${OPTIONNAMES}")

add_subdirectory(UnitTests)
add_subdirectory(Common)
add_subdirectory(Hexagon)

# Setup a legacy alias for 'check-eld'. This will likely change to be an alias
# for 'check-all' at some point in the future.
set_target_properties(check-eld PROPERTIES FOLDER "Linker Tests")
if (USE_LINKER_ALT_NAME)
  string(TOLOWER ${USE_LINKER_ALT_NAME} linker_alt_name_lower)
  if (linker_alt_name_lower MATCHES "ld.([a-z]+)*")
      set(linker_alt_name_suffix ${CMAKE_MATCH_1})
      info("Registering alternate check: check-${linker_alt_name_suffix}")
      add_custom_target(check-${linker_alt_name_suffix} DEPENDS check-eld)
  else()
    error("linker alt name ${USE_LINKER_ALT_NAME} should always start with ld")
    message(FATAL_ERROR "linker alt name ${USE_LINKER_ALT_NAME} should always start with ld")
  endif()
endif()
