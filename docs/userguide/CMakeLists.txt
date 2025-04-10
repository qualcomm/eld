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
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR})

  add_custom_target(eld-linkeroptions-docs)
  set(LINKER_OPTS_DUMP_DIR ${DOCS_BUILD_SOURCE}/LinkerOptionsDump)
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

    # Generates ${target}LinkerOptions restructureText documentation from the
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
        ${DOCS_BUILD_SOURCE}/documentation/options/${target}LinkerOptions.rst
        ${SKIP_OPTION})

    add_dependencies(eld-${targetLC}linkeroptions-docs
                     eld-${targetLC}linkeroptions-json-dump)
    add_dependencies(eld-linkeroptions-docs eld-${targetLC}linkeroptions-docs)
    if(NOT target MATCHES "Gnu")
      add_dependencies(eld-${targetLC}linkeroptions-docs
                       eld-gnulinkeroptions-json-dump)
    endif()
  endforeach()

  build_sphinx_target(
    html
    eld-userguide
    target_name
    DOXYGEN_XML_DIR
    ${DOXYGEN_OUTPUT_DIR}/xml
    DEPENDS
    eld-copy-userguide-source
    eld-linkeroptions-docs)

  add_custom_target(eld-docs DEPENDS ${target_name})
endif()
