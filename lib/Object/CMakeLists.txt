llvm_add_library(
  ELDObject
  STATIC
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
