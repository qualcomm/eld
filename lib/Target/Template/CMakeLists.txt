llvm_add_library(
  ELDTemplateLDBackend
  STATIC
  TemplateELFDynamic.cpp
  TemplateEmulation.cpp
  TemplateGOT.cpp
  TemplateInfo.cpp
  TemplateLDBackend.cpp
  TemplatePLT.cpp
  TemplateRelocator.cpp
  TemplateRelocationCompute.cpp)

add_subdirectory(TargetInfo)

target_link_libraries(
  ELDTemplateLDBackend PRIVATE ELDTemplateTargetInfo ELDCore ELDFragment
                               ELDObject ELDTarget )

# Add this to the end of target_link_libraries: LLVMTemplateCodeGen
