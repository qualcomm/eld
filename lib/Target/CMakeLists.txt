include(ELDColorMsg)

llvm_add_library(
  ELDTarget
  STATIC
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
  if (t STREQUAL "X86")
    set(t "x86_64")
  endif()
  trace("Targeting Linker for ${t}")
  add_subdirectory(${t})
endforeach()
