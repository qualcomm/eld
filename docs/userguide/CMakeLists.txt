add_subdirectory(api_docs)

if(LLVM_ENABLE_SPHINX)
  include(${ELD_SOURCE_DIR}/cmake/modules/BuildSphinxTarget.cmake)
  set(EXHALE_OUTPUT_DIR "${CMAKE_CURRENT_SOURCE_DIR}/api")
  if(NOT ELD_D2_BIN)
    if(DEFINED ENV{D2_BIN})
      set(ELD_D2_BIN
          "$ENV{D2_BIN}"
          CACHE FILEPATH "Path to the D2 executable used for ELD diagrams")
    else()
      find_program(ELD_D2_BIN NAMES d2 DOC "Path to the D2 executable used for ELD diagrams")
    endif()
  endif()
  if(NOT ELD_D2_BIN)
    message(FATAL_ERROR "D2 executable is required to build ELD userguide diagrams. Set ELD_D2_BIN or D2_BIN.")
  endif()
  configure_file(conf.py.in ${CMAKE_CURRENT_BINARY_DIR}/source/conf.py)
  file(MAKE_DIRECTORY ${EXHALE_OUTPUT_DIR})

  set(DOCS_BUILD_SOURCE ${CMAKE_CURRENT_BINARY_DIR}/source)
  set(ELD_COPY_USERGUIDE_SOURCE_STAMP
      ${DOCS_BUILD_SOURCE}/.copy-userguide-source.stamp)
  file(GLOB_RECURSE ELD_USERGUIDE_SOURCE_FILES ${CMAKE_CURRENT_SOURCE_DIR}/*)
  list(FILTER ELD_USERGUIDE_SOURCE_FILES EXCLUDE REGEX "/(__pycache__|api)(/|$)")
  # Copy the documentation sources to the build directory. We need to copy the
  # documentation sources to the build directory so that we have both
  # handwritten and automatically generated documentation sources in the same
  # directory. This is required because sphinx support only a single source
  # directory.
  add_custom_command(
    OUTPUT ${ELD_COPY_USERGUIDE_SOURCE_STAMP}
    COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_CURRENT_SOURCE_DIR}
            ${DOCS_BUILD_SOURCE}
    COMMAND ${CMAKE_COMMAND} -E touch ${ELD_COPY_USERGUIDE_SOURCE_STAMP}
    DEPENDS ${ELD_USERGUIDE_SOURCE_FILES}
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    COMMENT "Copying ELD userguide sources"
    VERBATIM)
  add_custom_target(eld-copy-userguide-source
                    DEPENDS ${ELD_COPY_USERGUIDE_SOURCE_STAMP})

  set(LINKER_OPTS_DUMP_DIR ${DOCS_BUILD_SOURCE}/LinkerOptionsDump)
  set(ELD_LINKEROPTIONS_DUMP_DIR_STAMP ${LINKER_OPTS_DUMP_DIR}/.dir.stamp)
  set(OPTIONS_SUPPLEMENTS_DIR
      ${CMAKE_CURRENT_SOURCE_DIR}/CommandLineOptionsSupplements)
  file(GLOB ELD_LINKEROPTIONS_SUPPLEMENTS ${OPTIONS_SUPPLEMENTS_DIR}/*)
  add_custom_command(
    OUTPUT ${ELD_LINKEROPTIONS_DUMP_DIR_STAMP}
    COMMAND ${CMAKE_COMMAND} -E make_directory ${LINKER_OPTS_DUMP_DIR}
    COMMAND ${CMAKE_COMMAND} -E touch ${ELD_LINKEROPTIONS_DUMP_DIR_STAMP}
    COMMENT "Creating ELD linker options dump directory"
    VERBATIM)
  add_custom_target(eld-linkeroptions-create-json-dump-dir
                    DEPENDS ${ELD_LINKEROPTIONS_DUMP_DIR_STAMP})

  set(ELD_LINKEROPTIONS_DOC_OUTPUTS)
  set(ELD_API_DOC_OUTPUTS ${DOXYGEN_OUTPUT_DIR}/xml/index.xml
                          ${DOXYGEN_OUTPUT_DIR}/html/index.html)
  set(ELD_GNU_LINKEROPTIONS_JSON_DUMP
      ${LINKER_OPTS_DUMP_DIR}/GnuLinkerOptionsTblGenDump.json)

  foreach(target Gnu ARM Hexagon RISCV)
    string(TOLOWER ${target} targetLC)
    # Generate JSON dump file of ${target}LinkerOptions.td
    set(LINKER_OPTIONS_TD
        ${ELD_SOURCE_DIR}/include/eld/Driver/${target}LinkerOptions.td)
    set(LINKER_OPTIONS_JSON_DUMP
        ${LINKER_OPTS_DUMP_DIR}/${target}LinkerOptionsTblGenDump.json)
    set(LINKER_OPTIONS_JSON_DEPENDS
        llvm-tblgen
        ${ELD_LINKEROPTIONS_DUMP_DIR_STAMP}
        ${LINKER_OPTIONS_TD}
        ${LLVM_MAIN_INCLUDE_DIR}/llvm/Option/OptParser.td)
    if(NOT target MATCHES "Gnu")
      list(APPEND LINKER_OPTIONS_JSON_DEPENDS
           ${ELD_SOURCE_DIR}/include/eld/Driver/GnuLinkerOptions.td)
    endif()
    add_custom_command(
      OUTPUT ${LINKER_OPTIONS_JSON_DUMP}
      COMMAND
        llvm-tblgen
        ${LINKER_OPTIONS_TD}
        -dump-json -I${LLVM_MAIN_INCLUDE_DIR}
        -I${ELD_SOURCE_DIR}/include/eld/Driver -o
        ${LINKER_OPTIONS_JSON_DUMP}
      DEPENDS ${LINKER_OPTIONS_JSON_DEPENDS}
      COMMENT "Generating ${target} linker options JSON dump"
      VERBATIM)
    add_custom_target(eld-${targetLC}linkeroptions-json-dump
                      DEPENDS ${LINKER_OPTIONS_JSON_DUMP})

    # Generates ${target}LinkerOptions restructureText documentation from the
    # ${target}LinkerOptions JSON dump file.
    set(SKIP_OPTION "")
    set(LINKER_OPTIONS_DOC_DEPENDS
        ${LINKER_OPTIONS_JSON_DUMP}
        ${ELD_COPY_USERGUIDE_SOURCE_STAMP}
        ${CMAKE_CURRENT_SOURCE_DIR}/GenerateOptionsDocsFromTblGen.py
        ${ELD_LINKEROPTIONS_SUPPLEMENTS})
    if(NOT target MATCHES "Gnu")
      set(SKIP_OPTION -s
                      ${ELD_GNU_LINKEROPTIONS_JSON_DUMP})
      list(APPEND LINKER_OPTIONS_DOC_DEPENDS ${ELD_GNU_LINKEROPTIONS_JSON_DUMP})
    endif()
    set(LINKER_OPTIONS_DOC
        ${DOCS_BUILD_SOURCE}/documentation/options/${target}LinkerOptions.rst)
    add_custom_command(
      OUTPUT ${LINKER_OPTIONS_DOC}
      COMMAND ${CMAKE_COMMAND} -E make_directory
              ${DOCS_BUILD_SOURCE}/documentation/options
      COMMAND
        python3 ${CMAKE_CURRENT_SOURCE_DIR}/GenerateOptionsDocsFromTblGen.py
        ${LINKER_OPTIONS_JSON_DUMP} -o
        ${LINKER_OPTIONS_DOC}
        ${SKIP_OPTION}
        -S ${OPTIONS_SUPPLEMENTS_DIR}
      DEPENDS ${LINKER_OPTIONS_DOC_DEPENDS}
      COMMENT "Generating ${target} linker options documentation"
      VERBATIM)
    add_custom_target(eld-${targetLC}linkeroptions-docs
                      DEPENDS ${LINKER_OPTIONS_DOC})
    list(APPEND ELD_LINKEROPTIONS_DOC_OUTPUTS ${LINKER_OPTIONS_DOC})
  endforeach()

  add_custom_target(eld-linkeroptions-docs
                    DEPENDS ${ELD_LINKEROPTIONS_DOC_OUTPUTS})

  # HTML user guide
  build_sphinx_target(
    html
    eld-userguide
    html_target_name
    DOXYGEN_XML_DIR
    ${DOXYGEN_OUTPUT_DIR}/xml
    DEPENDS
    ${ELD_API_DOC_OUTPUTS}
    ${ELD_COPY_USERGUIDE_SOURCE_STAMP}
    ${ELD_LINKEROPTIONS_DOC_OUTPUTS})

  add_custom_target(eld-docs DEPENDS ${html_target_name})

  # PDF user guide (optional)
  if(ELD_ENABLE_PDF_DOCS)
    build_sphinx_target(
      pdf
      eld-userguide
      pdf_target_name
      DOXYGEN_XML_DIR
      ${DOXYGEN_OUTPUT_DIR}/xml
      DEPENDS
      ${ELD_API_DOC_OUTPUTS}
      ${ELD_COPY_USERGUIDE_SOURCE_STAMP}
      ${ELD_LINKEROPTIONS_DOC_OUTPUTS})

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
