cmake_minimum_required(VERSION 3.20)

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

# When Plugins is the source directory, this is a package-only standalone
# project. When it is added by the ELD build, the parent project has already
# selected the compiler and configured the ELD libraries.
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
  include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/ELDStandalone.cmake")
  eld_standalone_prepare_compiler()
  project(ELDStandalonePlugins LANGUAGES CXX)

  set(ELD_STANDALONE ON)
  set(ELD_PLUGINS "${_eld_all_plugins}" CACHE STRING
    "Semicolon-separated plugins to build")
  eld_standalone_setup()
  eld_standalone_resolve_test_target()
  include(CTest)
  eld_standalone_setup_tests()
else()
  # The normal ELD build includes every plugin. Standalone selection is only
  # meaningful when this directory is configured as the source tree.
  set(ELD_PLUGINS "${_eld_all_plugins}")
  find_package(ZLIB REQUIRED)
endif()

foreach(_plugin IN LISTS ELD_PLUGINS)
  if(NOT _plugin IN_LIST _eld_all_plugins)
    message(FATAL_ERROR
      "Unknown ELD plugin '${_plugin}'. Known plugins: ${_eld_all_plugins}")
  endif()
  add_subdirectory("${CMAKE_CURRENT_SOURCE_DIR}/${_plugin}")
endforeach()

if(ELD_STANDALONE)
  message(STATUS "ELD package: ${ELD_PACKAGE_ROOT}")
  message(STATUS "Standalone plugins: ${ELD_PLUGINS}")
endif()
