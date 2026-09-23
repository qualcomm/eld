llvm_add_library(
  ELDX86LDBackend
  STATIC
  X86Emulation.cpp
  X86LDBackend.cpp
  x86_64Emulation.cpp
  x86_64Info.cpp
  x86_64LDBackend.cpp
  x86_64Relocator.cpp
  x86_64RelocationCompute.cpp
  x86_64PLT.cpp
  x86_64GOT.cpp
  X86_32Emulation.cpp
  X86_32LDBackend.cpp
  X86_32Relocator.cpp)

add_subdirectory(TargetInfo)

target_link_libraries(
  ELDX86LDBackend PRIVATE ELDX86TargetInfo ELDCore ELDFragment ELDObject
                             ELDTarget LLVMX86CodeGen)
