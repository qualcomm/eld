if(LLVM_ENABLE_SPHINX)
  find_package(Doxygen REQUIRED)

  set(DOXYGEN_INPUT_INCLUDE_DIR ${ELD_SOURCE_DIR}/include/eld)
  set(DOXYGEN_INPUT_LIB_DIR ${ELD_SOURCE_DIR}/lib)
  set(DOXYGEN_OUTPUT_DIR
      ${CMAKE_CURRENT_BINARY_DIR}
      CACHE INTERNAL "")
  set(DOXYGEN_INDEX_FILE ${DOXYGEN_OUTPUT_DIR}/xml/index.xml)
  set(DOXYGEN_FILE_IN ${CMAKE_CURRENT_SOURCE_DIR}/Doxyfile.in)
  set(DOXYGEN_FILE_OUT ${CMAKE_CURRENT_BINARY_DIR}/Doxyfile)
  configure_file(${DOXYGEN_FILE_IN} ${DOXYGEN_FILE_OUT} @ONLY)
  file(MAKE_DIRECTORY ${DOXYGEN_OUTPUT_DIR})

  add_custom_target(
    eld-api-docs ALL
    COMMAND ${DOXYGEN_EXECUTABLE} ${DOXYGEN_FILE_OUT}
    COMMENT "Generating API documentation with Doxygen")

endif()
