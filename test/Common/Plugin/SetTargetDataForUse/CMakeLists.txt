set(SOURCES SetTargetDataForUse.cpp)
find_package(Threads REQUIRED)

if(NOT CYGWIN AND LLVM_ENABLE_PIC)
  set(bsl ${BUILD_SHARED_LIBS})
  set(BUILD_SHARED_LIBS ON)
  add_llvm_library(SetTargetDataForUse ${SOURCES}
    PARTIAL_SOURCES_INTENDED LINK_LIBS LW Threads::Threads)
  add_llvm_library(SetTargetDataBeforeWritingOutput ${SOURCES}
    PARTIAL_SOURCES_INTENDED LINK_LIBS LW Threads::Threads)
  target_compile_definitions(SetTargetDataBeforeWritingOutput
    PRIVATE BEFORE_WRITING_OUTPUT)
  set(BUILD_SHARED_LIBS ${bsl})
endif()

add_plugin(SetTargetDataForUse)
add_plugin(SetTargetDataBeforeWritingOutput)
