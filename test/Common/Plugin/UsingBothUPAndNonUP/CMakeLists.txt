if(NOT CYGWIN AND LLVM_ENABLE_PIC)
  set(bsl ${BUILD_SHARED_LIBS})

  set(BUILD_SHARED_LIBS ON)

  add_llvm_library(UP UP.cpp PARTIAL_SOURCES_INTENDED LINK_LIBS LW)

  set_target_properties(UP PROPERTIES LIBRARY_OUTPUT_DIRECTORY
                                      "${CMAKE_LIBRARY_OUTPUT_DIRECTORY}/test")

  add_llvm_library(NonUP NonUP.cpp PARTIAL_SOURCES_INTENDED LINK_LIBS LW)

  set_target_properties(UP PROPERTIES LIBRARY_OUTPUT_DIRECTORY
                                      "${CMAKE_LIBRARY_OUTPUT_DIRECTORY}/test")

  set_target_properties(
    NonUP PROPERTIES LIBRARY_OUTPUT_DIRECTORY
                     "${CMAKE_LIBRARY_OUTPUT_DIRECTORY}/test")

  set(BUILD_SHARED_LIBS ${bsl})
endif()

add_common_plugin(UP)
add_common_plugin(NonUP)
