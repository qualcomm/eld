set(SOURCES findconfig.cpp)

if(NOT CYGWIN AND LLVM_ENABLE_PIC)
  set(SHARED_LIB_SOURCES ${SOURCES})

  set(bsl ${BUILD_SHARED_LIBS})

  set(BUILD_SHARED_LIBS ON)

  add_llvm_library(findconfig ${SHARED_LIB_SOURCES} LINK_LIBS
                   LW)

  target_compile_definitions(findconfig PRIVATE IN_DLL)

  set(BUILD_SHARED_LIBS ${bsl})

endif()

add_common_plugin(findconfig)
