add_subdirectory(design)
add_subdirectory(userguide)
if (NOT ELD_USE_EXTERNAL_LLVM)
  add_subdirectory(multiversion)
endif()

if(LLVM_ENABLE_SPHINX)
  set(ELD_DOCS_PUBLISH_DIR
      "${CMAKE_BINARY_DIR}/docs/publish"
      CACHE PATH "Directory used by docs publish targets")

  if(TARGET eld-userguide-docs)
    add_custom_target(
      eld-publish-userguide-docs
      COMMAND ${CMAKE_COMMAND} -E make_directory
              ${ELD_DOCS_PUBLISH_DIR}/userguide
      COMMAND ${CMAKE_COMMAND} -E copy_directory
              ${CMAKE_BINARY_DIR}/docs/userguide/html
              ${ELD_DOCS_PUBLISH_DIR}/userguide
      DEPENDS eld-userguide-docs
      COMMENT "Publishing ELD user guide docs to ${ELD_DOCS_PUBLISH_DIR}/userguide")

    if(TARGET eld-userguide-pdf-docs)
      add_dependencies(eld-publish-userguide-docs eld-userguide-pdf-docs)
      add_custom_command(
        TARGET eld-publish-userguide-docs
        POST_BUILD
        COMMAND
          ${CMAKE_COMMAND} -E copy_if_different
          ${CMAKE_BINARY_DIR}/docs/userguide/pdf/ELD_UserGuide.pdf
          ${ELD_DOCS_PUBLISH_DIR}/userguide/ELD_UserGuide.pdf)
    endif()
  endif()

  if(TARGET eld-design-docs)
    add_custom_target(
      eld-publish-design-docs
      COMMAND ${CMAKE_COMMAND} -E make_directory
              ${ELD_DOCS_PUBLISH_DIR}/design
      COMMAND ${CMAKE_COMMAND} -E copy_directory
              ${CMAKE_BINARY_DIR}/docs/design/html
              ${ELD_DOCS_PUBLISH_DIR}/design
      DEPENDS eld-design-docs
      COMMENT "Publishing ELD design docs to ${ELD_DOCS_PUBLISH_DIR}/design")

    if(TARGET eld-design-pdf-docs)
      add_dependencies(eld-publish-design-docs eld-design-pdf-docs)
      add_custom_command(
        TARGET eld-publish-design-docs
        POST_BUILD
        COMMAND
          ${CMAKE_COMMAND} -E copy_if_different
          ${CMAKE_BINARY_DIR}/docs/design/pdf/ELD_DesignGuide.pdf
          ${ELD_DOCS_PUBLISH_DIR}/design/ELD_DesignGuide.pdf)
    endif()
  endif()

  if(TARGET eld-publish-userguide-docs AND TARGET eld-publish-design-docs)
    add_custom_target(
      eld-publish-docs
      DEPENDS eld-publish-userguide-docs eld-publish-design-docs)
  endif()
endif()
