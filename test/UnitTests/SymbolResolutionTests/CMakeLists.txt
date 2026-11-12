add_eld_unittest(SymbolResolutionTests SymbolResolutionTest.cpp)

set(_lto_lib)
if(NOT LLVM_LINK_LLVM_DYLIB)
  set(_lto_lib LLVMLTO)
endif()
target_link_libraries(
  SymbolResolutionTests
  PRIVATE ELDCore
          ELDReaders
          ELDSymbolResolver
          ELDTarget
          ELDObject
          ${_lto_lib}
          LW)
