llvm_add_library(
  ELDARMLDBackend
  STATIC
  ARMAttributeFragment.cpp
  ARMEmulation.cpp
  ARMEXIDXFragment.cpp
  ARMInfo.cpp
  ARMGOT.cpp
  ARMLDBackend.cpp
  ARMPLT.cpp
  ARMRelocator.cpp
  ARMToARMStub.cpp
  ARMToTHMStub.cpp
  THMToARMStub.cpp
  THMToTHMStub.cpp)

add_subdirectory(TargetInfo)

target_link_libraries(ELDARMLDBackend PRIVATE ELDARMTargetInfo ELDCore
                                              ELDFragment ELDObject ELDTarget)
