set(LLVM_OPTIONAL_SOURCES DWARFObjectFixture.cpp)

set(DWARF_OBJECT_FIXTURE
    ${CMAKE_CURRENT_BINARY_DIR}/DWARFObjectFixture.o)

add_custom_command(
  OUTPUT ${DWARF_OBJECT_FIXTURE}
  COMMAND ${CMAKE_CXX_COMPILER} -g -O0 -c
          ${CMAKE_CURRENT_SOURCE_DIR}/DWARFObjectFixture.cpp -o
          ${DWARF_OBJECT_FIXTURE}
  DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/DWARFObjectFixture.cpp
  VERBATIM)

add_custom_target(DWARFObjectTestFixture DEPENDS ${DWARF_OBJECT_FIXTURE})

add_eld_unittest(DWARFObjectTests DWARFObjectTest.cpp)
add_dependencies(DWARFObjectTests DWARFObjectTestFixture)
target_compile_definitions(
  DWARFObjectTests PRIVATE DWARF_OBJECT_FIXTURE_PATH="${DWARF_OBJECT_FIXTURE}")
target_link_libraries(DWARFObjectTests PRIVATE ELDInput LW)
