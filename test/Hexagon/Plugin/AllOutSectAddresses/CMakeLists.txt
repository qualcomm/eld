set(SOURCES FindOutSectAddresses.cpp)

if(NOT CYGWIN AND LLVM_ENABLE_PIC)
  set(SHARED_LIB_SOURCES ${SOURCES})

  set(bsl ${BUILD_SHARED_LIBS})

  set(BUILD_SHARED_LIBS ON)

  add_llvm_library(FindOutSectAddresses ${SHARED_LIB_SOURCES} LINK_LIBS LW)

  set_target_properties(
    FindOutSectAddresses PROPERTIES LIBRARY_OUTPUT_DIRECTORY
                                    "${CMAKE_LIBRARY_OUTPUT_DIRECTORY}/test")

  set(BUILD_SHARED_LIBS ${bsl})
endif()

add_plugin(FindOutSectAddresses)
