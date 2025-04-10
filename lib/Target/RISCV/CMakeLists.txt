llvm_add_library(
  ELDRISCVLDBackend
  STATIC
  RISCVAttributeFragment.cpp
  RISCVELFDynamic.cpp
  RISCVEmulation.cpp
  RISCVGOT.cpp
  RISCVInfo.cpp
  RISCVPLT.cpp
  RISCVLDBackend.cpp
  RISCVRelocationCompute.cpp
  RISCVRelocationInfo.cpp
  RISCVRelocator.cpp
  RISCVRelaxationStats.cpp
  RISCVTargetMachine.cpp)

add_subdirectory(TargetInfo)

target_link_libraries(
  ELDRISCVLDBackend PRIVATE ELDRISCVTargetInfo ELDCore ELDFragment ELDObject
                            ELDTarget LLVMRISCVCodeGen)
