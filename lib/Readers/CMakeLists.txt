set(tablegen_deps intrinsics_gen)

add_llvm_library(
  ELDReaders
  ArchiveParser.cpp
  BinaryFileParser.cpp
  BitcodeReader.cpp
  DynamicELFReader.cpp
  EhFrameSection.cpp
  EhFrameHdrSection.cpp
  ELFSection.cpp
  ELFDynObjParser.cpp
  ELFExecObjParser.cpp
  ELFReaderBase.cpp
  ELFReader.cpp
  ELFRelocObjParser.cpp
  ExecELFReader.cpp
  RelocELFReader.cpp
  Relocation.cpp
  Section.cpp
  SymDefReader.cpp
  TimingSection.cpp
  TimingSlice.cpp
  DEPENDS
  ${tablegen_deps})

target_link_libraries(ELDReaders PRIVATE ELDInput)
