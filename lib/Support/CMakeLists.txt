llvm_add_library(
  ELDSupport
  STATIC
  DynamicLibrary.cpp
  FileSystem.cpp
  HashUtils.cpp
  INIReader.cpp
  MappingFile.cpp
  MemoryArea.cpp
  Memory.cpp
  OutputTarWriter.cpp
  Path.cpp
  RegisterTimer.cpp
  StringUtils.cpp
  StringRefUtils.cpp
  Target.cpp
  TargetRegistry.cpp
  Utils.cpp)

target_link_libraries(ELDSupport PRIVATE ELDConfig ELDDiagnostics LLVMDemangle
                                         LLVMSupport)
