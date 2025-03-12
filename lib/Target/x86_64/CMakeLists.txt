add_llvm_library(
  ELDx86_64LDBackend
  x86_64Emulation.cpp
  x86_64Info.cpp
  x86_64LDBackend.cpp
  x86_64TargetMachine.cpp
  x86_64Relocator.cpp
  x86_64RelocationCompute.cpp
  x86_64PLT.cpp
  x86_64GOT.cpp)

add_subdirectory(TargetInfo)

target_link_libraries(
  ELDx86_64LDBackend PRIVATE ELDx86_64TargetInfo ELDCore ELDFragment
                              ELDObject ELDTarget LLVMX86CodeGen)
