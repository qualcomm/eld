add_llvm_library(
  ELDObject
  GroupReader.cpp
  ScriptMemoryRegion.cpp
  ObjectBuilder.cpp
  ObjectLinker.cpp
  OutputSectionEntry.cpp
  RuleContainer.cpp
  SectionMap.cpp
  DEPENDS
  intrinsics_gen)

target_link_libraries(
  ELDObject
  PRIVATE ELDCore
          ELDFragment
          ELDReaders
          ELDScript
          ELDSupport
          ELDTarget
          LLVMSupport
          LLVMLTO)
