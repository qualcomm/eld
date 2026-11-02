set(SOURCES HelloWorldPlugin.cpp)

if(ELD_STANDALONE)
  eld_add_standalone_plugin(HelloWorldPlugin
    SOURCES ${SOURCES})
elseif(NOT CYGWIN AND LLVM_ENABLE_PIC)
  set(SHARED_LIB_SOURCES ${SOURCES})

  set(bsl ${BUILD_SHARED_LIBS})

  set(BUILD_SHARED_LIBS ON)

  add_llvm_library(HelloWorldPlugin ${SHARED_LIB_SOURCES} LINK_LIBS LW)

  set(BUILD_SHARED_LIBS ${bsl})
endif()

if(NOT ELD_STANDALONE)
  eld_register_plugin_tests(${CMAKE_CURRENT_SOURCE_DIR}/test HelloWorldPlugin)
endif()
