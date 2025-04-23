include(ELDColorMsg)

add_llvm_library(
  ELDTarget
  ELFDynamic.cpp
  ELFEmulation.cpp
  ELFFileFormat.cpp
  ELFSegment.cpp
  ELFSegmentFactory.cpp
  TargetInfo.cpp
  GNULDBackend.cpp
  LDFileFormat.cpp
  Relocator.cpp)

target_link_libraries(
  ELDTarget
  PRIVATE ELDBranchIsland
          ELDCore
          ELDGC
          ELDLayout
          ELDObject
          ELDReaders
          ELDWriters)

foreach(t ${ELD_TARGETS_TO_BUILD})
  trace("Targeting Linker for ${t}")
  add_subdirectory(${t})
endforeach()
