set(SOURCES MissingINIFile.cpp)

if(NOT CYGWIN AND LLVM_ENABLE_PIC)
  set(SHARED_LIB_SOURCES ${SOURCES})

  set(bsl ${BUILD_SHARED_LIBS})

  set(BUILD_SHARED_LIBS ON)

  add_llvm_library(missinginifile ${SHARED_LIB_SOURCES} LINK_LIBS LW)

  target_compile_definitions(missinginifile PRIVATE IN_DLL)

  set(BUILD_SHARED_LIBS ${bsl})

endif()

add_common_plugin(missinginifile)
