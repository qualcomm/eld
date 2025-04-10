set(SOURCES TestMultithreadedDiagnostics.cpp)
find_package(Threads REQUIRED)

if(NOT CYGWIN AND LLVM_ENABLE_PIC)
  set(SHARED_LIB_SOURCES ${SOURCES})

  set(bsl ${BUILD_SHARED_LIBS})

  set(BUILD_SHARED_LIBS ON)

  add_llvm_library(TestMultithreadedDiagnostics ${SHARED_LIB_SOURCES} LINK_LIBS
                   LW Threads::Threads)

  set_target_properties(
    TestMultithreadedDiagnostics
    PROPERTIES LIBRARY_OUTPUT_DIRECTORY
               "${CMAKE_LIBRARY_OUTPUT_DIRECTORY}/test")

  set(BUILD_SHARED_LIBS ${bsl})
endif()

add_common_plugin(TestMultithreadedDiagnostics)
