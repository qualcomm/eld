add_eld_unittest(RuleMatchingTests RuleMatchingTest.cpp)

target_link_libraries(
  RuleMatchingTests
  PRIVATE ELDConfig
          ELDCore
          ELDDiagnostics
          ELDInput
          ELDObject
          ELDReaders
          ELDScript
          ELDScriptParser
          LW)
