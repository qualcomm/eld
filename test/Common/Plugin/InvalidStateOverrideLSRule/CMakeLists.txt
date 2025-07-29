set(SOURCES InvalidStateOverrideLSRule.cpp)

if(NOT CYGWIN AND LLVM_ENABLE_PIC)
  set(SHARED_LIB_SOURCES ${SOURCES})

  set(bsl ${BUILD_SHARED_LIBS})

  set(BUILD_SHARED_LIBS ON)

  add_llvm_library(InvalidStateOverrideLSRule ${SHARED_LIB_SOURCES} LINK_LIBS LW)

  set(BUILD_SHARED_LIBS ${bsl})

endif()

add_common_plugin(InvalidStateOverrideLSRule)