add_eld_unittest(RelocationDataTests RelocationDataTest.cpp)
target_link_libraries(RelocationDataTests PRIVATE ELDCore ELDConfig
                       ELDDiagnostics ELDReaders ELDLayout ELDFragment LW)
