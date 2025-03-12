add_eld_unittest(InputFileTests InputFileTest.cpp)

target_link_libraries(
  InputFileTests
  PRIVATE ELDInput
          LW)
