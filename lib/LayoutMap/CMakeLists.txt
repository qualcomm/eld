llvm_add_library(ELDLayout STATIC LayoutPrinter.cpp LDYAML.cpp
                 TextLayoutPrinter.cpp YamlLayoutPrinter.cpp)

target_link_libraries(
  ELDLayout
  ELDConfig
  ELDFragment
  ELDSupport
  ELDDiagnostics
  ELDTarget
  ELDSymbolResolver)
