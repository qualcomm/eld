llvm_add_library(
  ELDInput
  STATIC
  ArchiveFile.cpp
  ArchiveMemberInput.cpp
  BinaryFile.cpp
  BitcodeFile.cpp
  ELFDynObjectFile.cpp
  ELFFileBase.cpp
  ELFObjectFile.cpp
  ELFExecutableFile.cpp
  InputAction.cpp
  InputBuilder.cpp
  Input.cpp
  InputFile.cpp
  JustSymbolsAction.cpp
  LinkerScriptFile.cpp
  ELDDirectory.cpp
  ObjectFile.cpp
  SearchDirs.cpp
  SymDefFile.cpp)

target_link_libraries(ELDInput PRIVATE ELDSupport ELDFragment
                                       LLVMDebugInfoDWARF)
