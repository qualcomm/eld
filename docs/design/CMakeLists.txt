if(LLVM_ENABLE_SPHINX)
  include(${ELD_SOURCE_DIR}/cmake/modules/BuildSphinxTarget.cmake)

  if(${SPHINX_OUTPUT_HTML})
    build_sphinx_target(
      html
      eld-design
      html_target_name
      SOURCE_DIR
      ${CMAKE_CURRENT_SOURCE_DIR}
      DOXYGEN_XML_DIR
      ${DOXYGEN_OUTPUT_DIR}/xml)
    add_custom_target(eld-design-docs DEPENDS ${html_target_name})
  endif()

  if(ELD_ENABLE_PDF_DOCS)
    build_sphinx_target(
      pdf
      eld-design
      pdf_target_name
      SOURCE_DIR
      ${CMAKE_CURRENT_SOURCE_DIR}
      DOXYGEN_XML_DIR
      ${DOXYGEN_OUTPUT_DIR}/xml)
    add_custom_target(eld-design-pdf-docs DEPENDS ${pdf_target_name})

    # Keep PDF alongside design HTML so index.rst link works in local builds.
    add_custom_command(
      TARGET eld-design-pdf-docs
      POST_BUILD
      COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_CURRENT_BINARY_DIR}/html
      COMMAND
        ${CMAKE_COMMAND} -E copy_if_different
        ${CMAKE_CURRENT_BINARY_DIR}/pdf/ELD_DesignGuide.pdf
        ${CMAKE_CURRENT_BINARY_DIR}/html/ELD_DesignGuide.pdf)
  endif()
endif()
