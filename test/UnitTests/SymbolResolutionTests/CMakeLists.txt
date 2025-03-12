add_eld_unittest(SymbolResolutionTests SymbolResolutionTest.cpp)

target_link_libraries(
  SymbolResolutionTests
  PRIVATE ELDCore
          ELDReaders
          ELDSymbolResolver
          ELDTarget
          ELDObject
          LLVMLTO
          LW)
