set(SOURCES SetTargetDataForUse.cpp)
find_package(Threads REQUIRED)

if(NOT CYGWIN AND LLVM_ENABLE_PIC)
  set(bsl ${BUILD_SHARED_LIBS})
  set(BUILD_SHARED_LIBS ON)
  add_llvm_library(SetTargetDataForUse ${SOURCES} LINK_LIBS LW Threads::Threads)
  set(BUILD_SHARED_LIBS ${bsl})
endif()

add_plugin(SetTargetDataForUse)
