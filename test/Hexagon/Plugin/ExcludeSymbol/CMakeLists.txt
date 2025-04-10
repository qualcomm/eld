set(SOURCES ExcludeSymbol.cpp)

if(NOT CYGWIN AND LLVM_ENABLE_PIC)
  set(SHARED_LIB_SOURCES ${SOURCES})

  set(bsl ${BUILD_SHARED_LIBS})

  set(BUILD_SHARED_LIBS ON)

  add_llvm_library(ExcludeSymbol ${SHARED_LIB_SOURCES} LINK_LIBS LW)

  if(MSVC)
    if(CMAKE_C_COMPILER MATCHES "clang-cl" AND CMAKE_CXX_COMPILER MATCHES
                                               "clang-cl")
      set_target_properties(
        ExcludeSymbol PROPERTIES COMPILE_FLAGS -Wno-deprecated-declarations)
    else()
      set_target_properties(ExcludeSymbol PROPERTIES COMPILE_FLAGS /wd4996)
    endif()
  else()
    set_target_properties(ExcludeSymbol PROPERTIES COMPILE_FLAGS
                                                   -Wno-deprecated-declarations)
  endif()

  set(BUILD_SHARED_LIBS ${bsl})

endif()

add_plugin(ExcludeSymbol)
