add_eld_unittest(TarReaderTests TarReaderTest.cpp)

target_link_libraries(TarReaderTests PRIVATE LW ELDSupport ELDScript)
