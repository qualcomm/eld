add_eld_unittest(StaticResolverTests StaticResolverTest.cpp)

set(_lto_lib)
if(NOT APPLE)
  set(_lto_lib LLVMLTO)
endif()
target_link_libraries(
  StaticResolverTests
  PRIVATE ELDCore
          ELDReaders
          ELDSymbolResolver
          ELDTarget
          ELDObject
          ${_lto_lib}
          LW
          ${system_libs})
