set(LLVM_OPTIONAL_SOURCES DWARFObjectFixture.cpp)

function(add_dwarf_object_fixture target output)
  add_library(${target} OBJECT DWARFObjectFixture.cpp)
  target_compile_options(${target} PRIVATE -g -O0 -fno-lto ${ARGN})
  set(${output} "$<TARGET_OBJECTS:${target}>" PARENT_SCOPE)
  set(${output}_TARGET ${target} PARENT_SCOPE)
endfunction()

add_dwarf_object_fixture(DWARFObjectFixtureObj DWARF_OBJECT_FIXTURE)
set(DWARF_OBJECT_FIXTURE_TARGETS ${DWARF_OBJECT_FIXTURE_TARGET})

include(CheckCXXCompilerFlag)
if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  # This fixture checks DWARF 5 address tables. Compile it directly so host
  # CMAKE_CXX_FLAGS (for example -march=native or -gdwarf-4) do not leak into
  # the cross-target command.
  set(_ELD_SAVED_CXX_FLAGS "${CMAKE_CXX_FLAGS}")
  set(_ELD_SAVED_REQUIRED_FLAGS "${CMAKE_REQUIRED_FLAGS}")
  if(DEFINED CMAKE_TRY_COMPILE_TARGET_TYPE)
    set(_ELD_SAVED_TRY_COMPILE_TARGET_TYPE_DEFINED TRUE)
    set(_ELD_SAVED_TRY_COMPILE_TARGET_TYPE
        "${CMAKE_TRY_COMPILE_TARGET_TYPE}")
  else()
    set(_ELD_SAVED_TRY_COMPILE_TARGET_TYPE_DEFINED FALSE)
  endif()
  set(CMAKE_CXX_FLAGS "")
  set(CMAKE_REQUIRED_FLAGS "--target=armv7-none-eabi")
  set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)
  check_cxx_compiler_flag("-gdwarf-5" ELD_CXX_SUPPORTS_ARM32_DWARF5)
  set(CMAKE_CXX_FLAGS "${_ELD_SAVED_CXX_FLAGS}")
  set(CMAKE_REQUIRED_FLAGS "${_ELD_SAVED_REQUIRED_FLAGS}")
  if(_ELD_SAVED_TRY_COMPILE_TARGET_TYPE_DEFINED)
    set(CMAKE_TRY_COMPILE_TARGET_TYPE
        "${_ELD_SAVED_TRY_COMPILE_TARGET_TYPE}")
  else()
    unset(CMAKE_TRY_COMPILE_TARGET_TYPE)
  endif()

  if(ELD_CXX_SUPPORTS_ARM32_DWARF5)
    set(DWARF_OBJECT_ARM32_DWARF5_FIXTURE
        "${CMAKE_CURRENT_BINARY_DIR}/DWARFObjectFixtureARM32.o")
    set(DWARF_OBJECT_ARM32_DWARF5_FIXTURE_TARGET
        DWARFObjectFixtureARM32Obj)
    add_custom_command(
      OUTPUT "${DWARF_OBJECT_ARM32_DWARF5_FIXTURE}"
      COMMAND "${CMAKE_CXX_COMPILER}" ${CMAKE_CXX_COMPILER_ARG1}
              --target=armv7-none-eabi -g -gdwarf-5 -O0 -fno-lto -c
              "${CMAKE_CURRENT_SOURCE_DIR}/DWARFObjectFixture.cpp" -o
              "${DWARF_OBJECT_ARM32_DWARF5_FIXTURE}"
      DEPENDS DWARFObjectFixture.cpp
      COMMENT "Generating ARM32 DWARF 5 fixture"
      VERBATIM)
    add_custom_target(DWARFObjectFixtureARM32Obj
                      DEPENDS "${DWARF_OBJECT_ARM32_DWARF5_FIXTURE}")
    list(APPEND DWARF_OBJECT_FIXTURE_TARGETS
         ${DWARF_OBJECT_ARM32_DWARF5_FIXTURE_TARGET})
  endif()
endif()

add_custom_target(DWARFObjectTestFixture)
add_dependencies(DWARFObjectTestFixture ${DWARF_OBJECT_FIXTURE_TARGETS})

add_eld_unittest(DWARFObjectTests DWARFObjectTest.cpp)
add_dependencies(DWARFObjectTests DWARFObjectTestFixture)
target_compile_definitions(
  DWARFObjectTests PRIVATE
  DWARF_OBJECT_FIXTURE_PATH="${DWARF_OBJECT_FIXTURE}")
if(ELD_CXX_SUPPORTS_ARM32_DWARF5)
  target_compile_definitions(
    DWARFObjectTests PRIVATE
    DWARF_OBJECT_ARM32_DWARF5_FIXTURE_PATH="${DWARF_OBJECT_ARM32_DWARF5_FIXTURE}")
endif()
target_link_libraries(DWARFObjectTests PRIVATE ELDInput LW)
