set(SOURCES AdvancedLTO.cpp)

if(NOT CYGWIN AND LLVM_ENABLE_PIC)
  set(SHARED_LIB_SOURCES ${SOURCES})

  set(bsl ${BUILD_SHARED_LIBS})
  set(BUILD_SHARED_LIBS ON)

  add_llvm_library(AdvancedLTO BUILDTREE_ONLY ${SHARED_LIB_SOURCES} LINK_LIBS LW)

  set(BUILD_SHARED_LIBS ${bsl})

  install(TARGETS AdvancedLTO
          COMPONENT ld.eld
          LIBRARY DESTINATION lib${LLVM_LIBDIR_SUFFIX}
          ARCHIVE DESTINATION lib${LLVM_LIBDIR_SUFFIX}
          RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}")

  if(TARGET ld.eld)
    add_dependencies(ld.eld AdvancedLTO)
  endif()

  if(TARGET install-ld.eld)
    add_dependencies(install-ld.eld AdvancedLTO)
  endif()

  if(TARGET check-eld)
    add_dependencies(check-eld AdvancedLTO)
  endif()
endif()
