llvm_add_library(
  ELDConfig
  STATIC
  GeneralOptions.cpp
  LinkerConfig.cpp
  TargetOptions.cpp
  Version.cpp
  DEPENDS
  llvm_vcsrevision_h)

target_link_libraries(ELDConfig PRIVATE ELDDiagnostics ELDSupport ELDScript LLVMOption)
add_dependencies(ELDConfig update-eld-verinfo)
