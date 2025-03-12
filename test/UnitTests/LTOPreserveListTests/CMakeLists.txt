add_eld_unittest(LTOPreserveListTests LTOPreserveListTest.cpp)

target_link_libraries(
  LTOPreserveListTests
  PRIVATE ELDCore
          ELDReaders
          ELDSymbolResolver
          ELDTarget
          ELDObject
          LLVMLTO
          LW)
