add_eld_unittest(StaticResolverTests StaticResolverTest.cpp)

target_link_libraries(
  StaticResolverTests
  PRIVATE ELDCore
          ELDReaders
          ELDSymbolResolver
          ELDTarget
          ELDObject
          LLVMLTO
          LW
          ${system_libs})
