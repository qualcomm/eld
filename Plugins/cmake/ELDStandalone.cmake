# Helpers for building host plugins against an installed ELD package.
# Package discovery and API checks live in FindELD.cmake.

function(eld_add_standalone_plugin target)
  cmake_parse_arguments(PLUGIN "" "" "SOURCES;LIBRARIES" ${ARGN})
  if(PLUGIN_UNPARSED_ARGUMENTS OR NOT PLUGIN_SOURCES)
    message(FATAL_ERROR
      "eld_add_standalone_plugin(${target}) requires SOURCES and accepts "
      "optional LIBRARIES")
  endif()

  add_library("${target}" SHARED ${PLUGIN_SOURCES})
  target_link_libraries("${target}" PRIVATE ELD::LW ${PLUGIN_LIBRARIES})
  target_compile_features("${target}" PRIVATE cxx_std_17)
  if(UNIX AND NOT APPLE)
    target_link_options("${target}" PRIVATE -Wl,-z,defs)
  endif()
  # Windows uses DLL search paths instead of RPATH.
  if(UNIX)
    set_target_properties("${target}" PROPERTIES
      BUILD_RPATH "${ELD_LIBRARY_DIR}"
      INSTALL_RPATH "${ELD_LIBRARY_DIR}")
  endif()
endfunction()
