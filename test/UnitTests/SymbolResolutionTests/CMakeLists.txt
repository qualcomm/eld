set(SYMBOL_RESOLUTION_TEST_SOURCES SymbolResolutionTest.cpp)

if(ELD_ENABLE_SYMBOL_VERSIONING)
  list(APPEND SYMBOL_RESOLUTION_TEST_SOURCES
       VersionedSymbolsSymbolResolutionTest.cpp)
endif()

add_eld_unittest(SymbolResolutionTests ${SYMBOL_RESOLUTION_TEST_SOURCES}
  PARTIAL_SOURCES_INTENDED)

target_link_libraries(
  SymbolResolutionTests
  PRIVATE ELDCore
          ELDReaders
          ELDSymbolResolver
          ELDTarget
          ELDObject
          LLVMLTO
          LW)
