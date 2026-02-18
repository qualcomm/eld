add_eld_unittest(ProgramHeaderTests ProgramHeaderTest.cpp)

target_link_libraries(
  ProgramHeaderTests
  PRIVATE ELDConfig
          ELDCore
          ELDDiagnostics
          ELDInput
          ELDObject
          ELDReaders
          ELDTarget
          LW)
