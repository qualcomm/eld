llvm_add_library(
  STATIC
  ELDTemplateLDBackend
  TemplateEmulation.cpp
  TemplateInfo.cpp
  TemplateLDBackend.cpp
  TemplateVRelocator.cpp
  TemplateTargetMachine.cpp
  TemplateRelocationCompute.cpp)

add_subdirectory(TargetInfo)

target_link_libraries(
  ELDTemplateLDBackend PRIVATE ELDTemplateTargetInfo ELDCore ELDFragment
                               ELDObject ELDTarget LLVMTemplateCodeGen)
