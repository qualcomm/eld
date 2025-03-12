add_executable(ELDExpectedUsage ELDExpectedUsage.cpp)
target_include_directories(ELDExpectedUsage PRIVATE ${CMAKE_SOURCE_DIR}/include/eld/PluginAPI)
target_link_libraries(ELDExpectedUsage PRIVATE LW)
set_target_properties(ELDExpectedUsage PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin/tests)