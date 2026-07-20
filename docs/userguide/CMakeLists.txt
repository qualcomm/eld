add_subdirectory(api_docs)

if(LLVM_ENABLE_SPHINX)
  include(${ELD_SOURCE_DIR}/cmake/modules/BuildSphinxTarget.cmake)
  set(EXHALE_OUTPUT_DIR "${CMAKE_CURRENT_SOURCE_DIR}/api")
  configure_file(conf.py.in ${CMAKE_CURRENT_BINARY_DIR}/source/conf.py)
  file(MAKE_DIRECTORY ${EXHALE_OUTPUT_DIR})

  set(DOCS_BUILD_SOURCE ${CMAKE_CURRENT_BINARY_DIR}/source)
  # Copy the documentation sources to the build directory. We need to copy the
  # documentation sources to the build directory so that we have both
  # handwritten and automatically generated documentation sources in the same
  # directory. This is required because sphinx support only a single source
  # directory.
  add_custom_target(
    eld-copy-userguide-source
    COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_CURRENT_SOURCE_DIR}
            ${DOCS_BUILD_SOURCE}
    COMMAND ${CMAKE_COMMAND} -E copy_directory
            ${ELD_SOURCE_DIR}/docs/DeveloperDocs
            ${DOCS_BUILD_SOURCE}/DeveloperDocs
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR})

  add_custom_target(eld-linkeroptions-docs)
  set(LINKER_OPTS_DUMP_DIR ${DOCS_BUILD_SOURCE}/LinkerOptionsDump)
  set(OPTIONS_SUPPLEMENTS_DIR
      ${CMAKE_CURRENT_SOURCE_DIR}/CommandLineOptionsSupplements)
  add_custom_target(
    eld-linkeroptions-create-json-dump-dir
    COMMAND ${CMAKE_COMMAND} -E make_directory ${LINKER_OPTS_DUMP_DIR})

  foreach(target Gnu ARM Hexagon RISCV)
    string(TOLOWER ${target} targetLC)
    # Generate JSON dump file of ${target}LinkerOptions.td
    add_custom_target(
      eld-${targetLC}linkeroptions-json-dump
      COMMAND
        llvm-tblgen
        ${ELD_SOURCE_DIR}/include/eld/Driver/${target}LinkerOptions.td
        -dump-json -I${LLVM_MAIN_INCLUDE_DIR}
        -I${ELD_SOURCE_DIR}/include/eld/Driver -o
        ${LINKER_OPTS_DUMP_DIR}/${target}LinkerOptionsTblGenDump.json
      DEPENDS eld-linkeroptions-create-json-dump-dir)

    # Generates ${target}LinkerOptions Markdown documentation from the
    # ${target}LinkerOptions JSON dump file.
    set(SKIP_OPTION "")
    if(NOT target MATCHES "Gnu")
      set(SKIP_OPTION -s
                      ${LINKER_OPTS_DUMP_DIR}/GnuLinkerOptionsTblGenDump.json)
    endif()
    add_custom_target(
      eld-${targetLC}linkeroptions-docs
      COMMAND
        python3 ${CMAKE_CURRENT_SOURCE_DIR}/GenerateOptionsDocsFromTblGen.py
        ${LINKER_OPTS_DUMP_DIR}/${target}LinkerOptionsTblGenDump.json -o
        ${DOCS_BUILD_SOURCE}/documentation/options/${target}LinkerOptions.md
        ${SKIP_OPTION}
        -S ${OPTIONS_SUPPLEMENTS_DIR})

    add_dependencies(eld-${targetLC}linkeroptions-docs
                     eld-${targetLC}linkeroptions-json-dump)
    add_dependencies(eld-linkeroptions-docs eld-${targetLC}linkeroptions-docs)
    if(NOT target MATCHES "Gnu")
      add_dependencies(eld-${targetLC}linkeroptions-docs
                       eld-gnulinkeroptions-json-dump)
    endif()
  endforeach()

  # HTML user guide
  build_sphinx_target(
    html
    eld-userguide
    html_target_name
    DOXYGEN_XML_DIR
    ${DOXYGEN_OUTPUT_DIR}/xml
    DEPENDS
    eld-copy-userguide-source
    eld-linkeroptions-docs)

  add_custom_target(eld-docs DEPENDS ${html_target_name})

  # PDF user guide (optional)
  if(ELD_ENABLE_PDF_DOCS)
    find_package(Python3 REQUIRED COMPONENTS Interpreter)
    find_program(TYPST_EXECUTABLE typst)

    # Prefer the standalone Typst executable when available so CMake builds do
    # not depend on Python package import paths. Fall back to the Python module
    # for environments that install Typst through requirements.txt.
    set(ELD_TYPST_ARGS "")
    if(TYPST_EXECUTABLE)
      list(APPEND ELD_TYPST_ARGS --typst-executable ${TYPST_EXECUTABLE})
    else()
      execute_process(
        COMMAND ${Python3_EXECUTABLE} -c "import typst"
        RESULT_VARIABLE ELD_TYPST_IMPORT_RESULT
        OUTPUT_QUIET ERROR_QUIET)
      if(NOT ELD_TYPST_IMPORT_RESULT EQUAL 0)
        message(
          FATAL_ERROR
            "ELD_ENABLE_PDF_DOCS requires Typst: install the 'typst' executable or the Python 'typst' package")
      endif()
    endif()

    # The converter consumes Sphinx's singlehtml output before compiling Typst.
    build_sphinx_target(
      singlehtml
      eld-userguide
      singlehtml_target_name
      DOXYGEN_XML_DIR
      ${DOXYGEN_OUTPUT_DIR}/xml
      DEPENDS
      eld-copy-userguide-source
      eld-linkeroptions-docs)

    set(pdf_target_name docs-eld-userguide-pdf)
    set(ELD_USERGUIDE_SINGLEHTML_DIR ${CMAKE_CURRENT_BINARY_DIR}/singlehtml)
    set(ELD_USERGUIDE_PDF_DIR ${CMAKE_CURRENT_BINARY_DIR}/pdf)
    set(ELD_USERGUIDE_HTML_DIR ${CMAKE_CURRENT_BINARY_DIR}/html)
    set(ELD_USERGUIDE_TYPST ${ELD_USERGUIDE_SINGLEHTML_DIR}/ELD_UserGuide.typ)
    set(ELD_USERGUIDE_PDF ${ELD_USERGUIDE_PDF_DIR}/ELD_UserGuide.pdf)
    set(ELD_USERGUIDE_HTML_PDF ${ELD_USERGUIDE_HTML_DIR}/ELD_UserGuide.pdf)

    add_custom_command(
      OUTPUT ${ELD_USERGUIDE_PDF} ${ELD_USERGUIDE_HTML_PDF}
      COMMAND
        ${CMAKE_COMMAND} -E env --unset=PYTHONPATH
        ${Python3_EXECUTABLE}
        ${CMAKE_CURRENT_SOURCE_DIR}/GenerateTypstPDF.py
        ${ELD_USERGUIDE_SINGLEHTML_DIR}/index.html ${ELD_USERGUIDE_TYPST}
        --pdf-output ${ELD_USERGUIDE_PDF}
        --root ${ELD_USERGUIDE_SINGLEHTML_DIR}
        --title "ELD User Guide"
        --author "Qualcomm Technologies, Inc."
        ${ELD_TYPST_ARGS}
      COMMAND ${CMAKE_COMMAND} -E make_directory ${ELD_USERGUIDE_HTML_DIR}
      # Keep the generated PDF next to the HTML documentation for install and
      # packaging flows that already consume the Sphinx html output directory.
      COMMAND ${CMAKE_COMMAND} -E copy_if_different
              ${ELD_USERGUIDE_PDF} ${ELD_USERGUIDE_HTML_PDF}
      DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/GenerateTypstPDF.py
      COMMENT
        "Generating Typst PDF documentation for eld-userguide into \"${ELD_USERGUIDE_HTML_PDF}\"")

    add_custom_target(${pdf_target_name} DEPENDS ${ELD_USERGUIDE_HTML_PDF})
    add_dependencies(${pdf_target_name} ${singlehtml_target_name} ${html_target_name})
    add_dependencies(eld-docs ${pdf_target_name})
  endif()
endif()

if(ELD_ENABLE_ASCIIDOC)
  message(STATUS "ASCIIDOC Documentation Enabled")
  # Convert asciidoc to html documentation.
  include(${ELD_SOURCE_DIR}/cmake/modules/BuildASCIIDOCTarget.cmake)
  build_adoc_to_html(${ELD_SOURCE_DIR}/docs/userguide/documentation/asciidoc/)

  # Build asciidocs with eld-docs
  if(LLVM_ENABLE_SPHINX)
    add_dependencies(eld-docs eld-asciidocs)
  endif()
endif()
