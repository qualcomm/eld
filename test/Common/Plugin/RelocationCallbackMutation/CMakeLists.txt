set(SOURCES RelocationCallbackMutation.cpp)

if(NOT CYGWIN AND LLVM_ENABLE_PIC)
  set(SHARED_LIB_SOURCES ${SOURCES})
  set(BUILD_SHARED_LIBS_SAVED ${BUILD_SHARED_LIBS})
  set(BUILD_SHARED_LIBS ON)

  add_llvm_library(RelocationCallbackMutation ${SHARED_LIB_SOURCES} LINK_LIBS
                   LW)

  set(BUILD_SHARED_LIBS ${BUILD_SHARED_LIBS_SAVED})
endif()

add_plugin(RelocationCallbackMutation)
