add_llvm_library(
  ELDSymbolResolver
  IRBuilder.cpp
  LDSymbol.cpp
  NamePool.cpp
  ResolveInfo.cpp
  Resolver.cpp
  StaticResolver.cpp
  SymbolInfo.cpp
  SymbolResolutionInfo.cpp)

target_link_libraries(ELDSymbolResolver PRIVATE ELDCore)
