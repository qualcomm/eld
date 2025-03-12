set(SOURCES ConfigFilePlugin.cpp)

if(NOT CYGWIN AND LLVM_ENABLE_PIC)
  set(SHARED_LIB_SOURCES ${SOURCES})

  set(bsl ${BUILD_SHARED_LIBS})

  set(BUILD_SHARED_LIBS ON)

  add_llvm_library(ConfigFilePlugin ${SHARED_LIB_SOURCES} LINK_LIBS LW LLVMSupport)

  set(BUILD_SHARED_LIBS ${bsl})

endif()

add_plugin(ConfigFilePlugin)
