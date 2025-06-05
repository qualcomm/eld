if(ELD_ENABLE_ASCIIDOC)
  find_package(Python3 REQUIRED)
endif()

# Convert asciidoc documentation to html
function(build_adoc_to_html adoc_source_dir)
  file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/html/adoc/)
  file(GLOB ADOC_FILES "${adoc_source_dir}/*")
  foreach(ADOC_FILE ${ADOC_FILES})
    get_filename_component(ADOC_FILE_NAME ${ADOC_FILE} NAME)
    add_custom_target(
      eld-asciidocs
      COMMAND
        ${Python3_EXECUTABLE} -m asciidoc -o
        ${CMAKE_CURRENT_BINARY_DIR}/html/adoc/${ADOC_FILE_NAME}.html
        ${ADOC_FILE}
      COMMAND
        tree -H "${CMAKE_CURRENT_BINARY_DIR}/html/adoc/" -L 1 -T
        "ADOC Documentation"--noreport -I "index.html" -o
        ${CMAKE_CURRENT_BINARY_DIR}/html/adoc/index.html
      WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/html/adoc/
      DEPENDS ${Python3_EXECUTABLE}
      COMMENT
        "Generating ASCIIDOC documentation in ${CMAKE_CURRENT_BINARY_DIR}/html/adoc/"
    )
  endforeach()
endfunction()
