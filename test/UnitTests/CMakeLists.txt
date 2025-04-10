set(LLVM_LINK_COMPONENTS
    BinaryFormat
    BitReader
    CodeGen
    Core
    Linker
    LTO
    MC
    Object
    Passes
    Support
    Target
    TargetParser)

add_custom_target(ELDUnitTests)
set_target_properties(ELDUnitTests PROPERTIES FOLDER "eld unit tests")
get_property(
  system_libs
  TARGET LLVMSupport
  PROPERTY LLVM_SYSTEM_LIBS)

# add_eld_unittest(test_dirname file1.cpp file2.cpp)
#
# Will compile the list of files together and link against eld Produces a binary
# named 'basename(test_dirname)'.
function(add_eld_unittest test_dirname)
  add_unittest(ELDUnitTests ${test_dirname} ${ARGN})
  target_link_libraries(${test_dirname} PRIVATE ${system_libs})
endfunction()

configure_lit_site_cfg(${CMAKE_CURRENT_SOURCE_DIR}/lit.site.cfg.in
                       ${CMAKE_CURRENT_BINARY_DIR}/lit.site.cfg)

set(TEST_DEPENDS ELDUnitTests)

set(ADDITIONAL_TESTMSG "")
if(LLVM_USE_SANITIZE)
  set(ADDITIONAL_TESTMSG "With Sanitizer = ${LLVM_USE_SANITIZE}")
endif()

add_lit_testsuite(
  eld-unit-test "Running the Linker unit tests ${ADDITIONAL_TESTMSG}"
  ${CMAKE_CURRENT_BINARY_DIR} DEPENDS ${TEST_DEPENDS})

set_target_properties(eld-unit-test PROPERTIES FOLDER "Tests")

# Setup a legacy alias for 'check-eld-unit'. This will likely change to be an
# alias for 'check-all' at some point in the future.
add_custom_target(check-eld-unit)
add_dependencies(check-eld-unit eld-unit-test)
set_target_properties(check-eld-unit PROPERTIES FOLDER "Linker Unit Tests")
add_dependencies(check-eld check-eld-unit)

add_subdirectory(DiagnosticFrameworkTests)
add_subdirectory(SampleTests)
add_subdirectory(StaticResolverTests)
add_subdirectory(SymbolResolutionTests)
add_subdirectory(LTOPreserveListTests)
add_subdirectory(LinkerDriverTests)
add_subdirectory(INIWriterTests)
add_subdirectory(InputFileTests)
add_subdirectory(PluginAPI)
