add_eld_unittest(PluginRelocationTests PluginRelocationTest.cpp)
target_link_libraries(PluginRelocationTests PRIVATE ELDCore ELDConfig
                       ELDDiagnostics ELDReaders LW)
