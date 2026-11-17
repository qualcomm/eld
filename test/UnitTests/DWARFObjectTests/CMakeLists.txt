set(LLVM_OPTIONAL_SOURCES DWARFObjectFixture.cpp)

function(add_dwarf_object_fixture target output dwarf_version)
  add_library(${target} OBJECT DWARFObjectFixture.cpp)
  target_compile_options(${target} PRIVATE -g -gdwarf-${dwarf_version} -O0
                                           ${ARGN})
  set(${output} "$<TARGET_OBJECTS:${target}>" PARENT_SCOPE)
  set(${output}_TARGET ${target} PARENT_SCOPE)
endfunction()

add_dwarf_object_fixture(DWARFObjectFixtureObj DWARF_OBJECT_FIXTURE 4)
set(DWARF_OBJECT_FIXTURE_TARGETS ${DWARF_OBJECT_FIXTURE_TARGET})
include(CheckCXXCompilerFlag)
check_cxx_compiler_flag("-gdwarf-5" ELD_CXX_SUPPORTS_DWARF5)
if(ELD_CXX_SUPPORTS_DWARF5)
  add_dwarf_object_fixture(DWARFObjectFixtureDWARF5Obj
                           DWARF_OBJECT_DWARF5_FIXTURE 5)
  list(APPEND DWARF_OBJECT_FIXTURE_TARGETS
       ${DWARF_OBJECT_DWARF5_FIXTURE_TARGET})
endif()
if(LLVM_ENABLE_ZLIB)
  check_cxx_compiler_flag("-gz=zlib" ELD_CXX_SUPPORTS_GZ_ZLIB)
  if(ELD_CXX_SUPPORTS_GZ_ZLIB)
    add_dwarf_object_fixture(DWARFObjectFixtureCompressedObj
                             DWARF_OBJECT_COMPRESSED_FIXTURE 4 -gz=zlib)
    list(APPEND DWARF_OBJECT_FIXTURE_TARGETS
         ${DWARF_OBJECT_COMPRESSED_FIXTURE_TARGET})
  endif()
  check_cxx_compiler_flag("-gz=zlib-gnu" ELD_CXX_SUPPORTS_GZ_ZLIB_GNU)
  if(ELD_CXX_SUPPORTS_GZ_ZLIB_GNU)
    add_dwarf_object_fixture(DWARFObjectFixtureGNUCompressedObj
                             DWARF_OBJECT_GNU_COMPRESSED_FIXTURE 4
                             -gz=zlib-gnu)
    list(APPEND DWARF_OBJECT_FIXTURE_TARGETS
         ${DWARF_OBJECT_GNU_COMPRESSED_FIXTURE_TARGET})
  endif()
endif()

add_custom_target(DWARFObjectTestFixture)
add_dependencies(DWARFObjectTestFixture ${DWARF_OBJECT_FIXTURE_TARGETS})

add_eld_unittest(DWARFObjectTests DWARFObjectTest.cpp)
add_dependencies(DWARFObjectTests DWARFObjectTestFixture)
target_compile_definitions(
  DWARFObjectTests PRIVATE
  DWARF_OBJECT_FIXTURE_PATH="${DWARF_OBJECT_FIXTURE}")
if(ELD_CXX_SUPPORTS_DWARF5)
  target_compile_definitions(
    DWARFObjectTests PRIVATE
    DWARF_OBJECT_DWARF5_FIXTURE_PATH="${DWARF_OBJECT_DWARF5_FIXTURE}")
endif()
if(ELD_CXX_SUPPORTS_GZ_ZLIB)
  target_compile_definitions(
    DWARFObjectTests PRIVATE
    DWARF_OBJECT_COMPRESSED_FIXTURE_PATH="${DWARF_OBJECT_COMPRESSED_FIXTURE}")
endif()
if(ELD_CXX_SUPPORTS_GZ_ZLIB_GNU)
  target_compile_definitions(
    DWARFObjectTests PRIVATE
    DWARF_OBJECT_GNU_COMPRESSED_FIXTURE_PATH="${DWARF_OBJECT_GNU_COMPRESSED_FIXTURE}")
endif()
target_link_libraries(DWARFObjectTests PRIVATE ELDInput LW)
