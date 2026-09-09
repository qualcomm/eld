add_eld_unittest(LTOPreserveListTests LTOPreserveListTest.cpp)

set(_lto_lib)
if(NOT APPLE)
  set(_lto_lib LLVMLTO)
endif()
target_link_libraries(
  LTOPreserveListTests
  PRIVATE ELDCore
          ELDReaders
          ELDSymbolResolver
          ELDTarget
          ELDObject
          ${_lto_lib}
          LW)
