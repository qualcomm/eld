llvm_add_library(
  ELDARMLDBackend
  STATIC
  ARMAttributeFragment.cpp
  ARMELFDynamic.cpp
  ARMEmulation.cpp
  ARMEXIDXSection.cpp
  ARMInfo.cpp
  ARMGOT.cpp
  ARMLDBackend.cpp
  ARMPLT.cpp
  ARMRelocator.cpp
  ARMTargetMachine.cpp
  ARMToARMStub.cpp
  ARMToTHMStub.cpp
  THMToARMStub.cpp
  THMToTHMStub.cpp)

add_subdirectory(TargetInfo)

target_link_libraries(ELDARMLDBackend PRIVATE ELDARMTargetInfo ELDCore
                                              ELDFragment ELDObject ELDTarget)
