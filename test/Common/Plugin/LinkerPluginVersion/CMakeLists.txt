set(SOURCES LinkerPluginVersion.cpp)

if(NOT CYGWIN AND LLVM_ENABLE_PIC)

  set(bsl ${BUILD_SHARED_LIBS})

  set(BUILD_SHARED_LIBS ON)

  add_llvm_library(LinkerPluginVersion ${SOURCES} LINK_LIBS LW)

  set(BUILD_SHARED_LIBS ${bsl})

endif()

add_common_plugin(LinkerPluginVersion)

if(NOT CYGWIN AND LLVM_ENABLE_PIC)

  set(bsl ${BUILD_SHARED_LIBS})

  set(BUILD_SHARED_LIBS ON)

  add_llvm_library(LinkerPluginNoVersion ${SOURCES} LINK_LIBS LW)
  target_compile_definitions(LinkerPluginNoVersion PRIVATE NO_VERSION)

  set(BUILD_SHARED_LIBS ${bsl})

endif()

add_common_plugin(LinkerPluginNoVersion)
