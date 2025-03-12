add_llvm_library(
  ELDAArch64LDBackend
  AArch64ELFDynamic.cpp
  AArch64GOT.cpp
  AArch64Info.cpp
  AArch64Emulation.cpp
  AArch64Errata843419Stub.cpp
  AArch64ErrataFactory.cpp
  AArch64ErrataIslandFactory.cpp
  AArch64FarcallStub.cpp
  AArch64LDBackend.cpp
  AArch64NoteGNUPropertyFragment.cpp
  AArch64PLT.cpp
  AArch64Relocator.cpp
  AArch64TargetMachine.cpp)

add_subdirectory(TargetInfo)

target_link_libraries(
  ELDAArch64LDBackend PRIVATE ELDAArch64TargetInfo ELDCore ELDFragment
                               ELDObject ELDTarget)
