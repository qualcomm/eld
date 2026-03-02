# Check if llvm_vcsrevision_h target exists (only in integrated builds)
if(TARGET llvm_vcsrevision_h)
  set(LLVM_VCS_DEPS llvm_vcsrevision_h)
else()
  set(LLVM_VCS_DEPS)
endif()

llvm_add_library(
  ELDConfig
  STATIC
  GeneralOptions.cpp
  LinkerConfig.cpp
  TargetOptions.cpp
  Version.cpp
  DEPENDS
  ${LLVM_VCS_DEPS})

target_link_libraries(ELDConfig PRIVATE ELDDiagnostics ELDSupport ELDScript LLVMOption)
add_dependencies(ELDConfig update-eld-verinfo)
