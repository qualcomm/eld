if(LLVM_ENABLE_SPHINX)
  include(${ELD_SOURCE_DIR}/cmake/modules/BuildSphinxTarget.cmake)
  set(DOCS_BUILD_SOURCE ${CMAKE_CURRENT_BINARY_DIR}/source)
  file(MAKE_DIRECTORY ${DOCS_BUILD_SOURCE})
  configure_file(conf.py.in ${DOCS_BUILD_SOURCE}/conf.py)

  add_custom_target(
    eld-copy-design-source
    COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_CURRENT_SOURCE_DIR}
            ${DOCS_BUILD_SOURCE}
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR})

  if(${SPHINX_OUTPUT_HTML})
    build_sphinx_target(
      html
      eld-design
      html_target_name
      SOURCE_DIR
      ${DOCS_BUILD_SOURCE}
      DOXYGEN_XML_DIR
      ${DOXYGEN_OUTPUT_DIR}/xml
      DEPENDS
      eld-copy-design-source)
    add_custom_target(eld-design-docs DEPENDS ${html_target_name})
  endif()

  if(ELD_ENABLE_PDF_DOCS)
    find_package(Python3 COMPONENTS Interpreter QUIET)
    set(ELD_DESIGN_HAS_RST2PDF OFF)
    if(Python3_Interpreter_FOUND)
      execute_process(
        COMMAND ${Python3_EXECUTABLE} -c "import rst2pdf.pdfbuilder"
        RESULT_VARIABLE ELD_DESIGN_RST2PDF_CHECK
        OUTPUT_QUIET
        ERROR_QUIET)
      if(ELD_DESIGN_RST2PDF_CHECK EQUAL 0)
        set(ELD_DESIGN_HAS_RST2PDF ON)
      endif()
    endif()

    if(ELD_DESIGN_HAS_RST2PDF)
      build_sphinx_target(
        pdf
        eld-design
        pdf_target_name
        SOURCE_DIR
        ${DOCS_BUILD_SOURCE}
        DOXYGEN_XML_DIR
        ${DOXYGEN_OUTPUT_DIR}/xml
        DEPENDS
        eld-copy-design-source)
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
    else()
      message(
        WARNING
          "ELD_ENABLE_PDF_DOCS=ON but rst2pdf is unavailable in the configured Python environment. Skipping design PDF docs target."
      )
    endif()
  endif()
endif()
