llvm_add_library(ELDLayout STATIC LayoutInfo.cpp
                 LDYAML.cpp TextLayoutPrinter.cpp YamlLayoutPrinter.cpp)

target_link_libraries(
  ELDLayout
  PRIVATE ELDConfig
          ELDFragment
          ELDSupport
          ELDDiagnostics
          ELDTarget
          ELDSymbolResolver)
