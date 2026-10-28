cmake_minimum_required(VERSION 3.20)

option(BUILD_STANDALONE_PLUGINS
  "Build plugins against an installed ELD package instead of in-tree ELD" OFF)

if(BUILD_STANDALONE_PLUGINS)
  if(TARGET LW)
    message(FATAL_ERROR
      "BUILD_STANDALONE_PLUGINS is for a separate plugin build, not an ELD build")
  endif()
  # Use CMAKE_CXX_COMPILER and CMAKE_CXX_FLAGS (or a host toolchain file).
  # In particular, libc++ must be selected before project() tests the compiler.
  include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/ELDStandalone.cmake")
  project(ELDStandalonePlugins LANGUAGES CXX)
  list(PREPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
  find_package(ELD REQUIRED MODULE)
  eld_check_standalone_runtime()
elseif(NOT TARGET LW)
  message(FATAL_ERROR
    "For a package-based plugin build, pass -DBUILD_STANDALONE_PLUGINS=ON")
endif()
set(ELD_STANDALONE ${BUILD_STANDALONE_PLUGINS})

# A plugin is an immediate child directory containing its own CMakeLists.txt.
# CONFIGURE_DEPENDS makes CMake re-run configuration when a plugin directory is
# added or removed. Infrastructure directories are explicitly excluded.
set(_eld_plugin_excludes cmake Support)
file(GLOB _eld_plugin_candidates
  LIST_DIRECTORIES true
  CONFIGURE_DEPENDS
  RELATIVE "${CMAKE_CURRENT_SOURCE_DIR}"
  "*")
set(_eld_all_plugins)
foreach(_candidate IN LISTS _eld_plugin_candidates)
  if(NOT IS_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/${_candidate}")
    continue()
  endif()
  if(_candidate IN_LIST _eld_plugin_excludes)
    continue()
  endif()
  if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/${_candidate}/CMakeLists.txt")
    list(APPEND _eld_all_plugins "${_candidate}")
  endif()
endforeach()
list(SORT _eld_all_plugins)

if(NOT _eld_all_plugins)
  message(FATAL_ERROR
    "No plugin directories found below ${CMAKE_CURRENT_SOURCE_DIR}")
endif()
message(STATUS "Discovered ELD plugins: ${_eld_all_plugins}")

if(ELD_STANDALONE)
  set(ELD_PLUGINS "${_eld_all_plugins}" CACHE STRING
    "Semicolon-separated plugins to build")
else()
  # The normal ELD build includes every plugin.
  set(ELD_PLUGINS "${_eld_all_plugins}")
endif()

foreach(_plugin IN LISTS ELD_PLUGINS)
  if(NOT _plugin IN_LIST _eld_all_plugins)
    message(FATAL_ERROR
      "Unknown ELD plugin '${_plugin}'. Known plugins: ${_eld_all_plugins}")
  endif()
  add_subdirectory("${_plugin}")
endforeach()

if(ELD_STANDALONE)
  message(STATUS "ELD package: ${ELD_TOOLS_DIR}")
  message(STATUS "Standalone plugins: ${ELD_PLUGINS}")
endif()
