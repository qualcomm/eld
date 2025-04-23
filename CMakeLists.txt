cmake_minimum_required(VERSION 3.16)

set(CMAKE_COLOR_MAKEFILE ON)

set(ELD_SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR} CACHE STRING "")
set(ELD_BINARY_DIR ${CMAKE_CURRENT_BINARY_DIR} CACHE STRING "")

list(APPEND CMAKE_MODULE_PATH "${ELD_SOURCE_DIR}/cmake/modules")
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake/")

include(CMakeRequiredVariablesCheck REQUIRED)
include(VersionFromLinkerGit)
include(ELDColorMsg)

set(LINKER_PLUGIN_API_MAJOR_VERSION "4")
set(LINKER_PLUGIN_API_MINOR_VERSION "0")

# Compute the ELD version from the LLVM version.
string(REGEX MATCH "[0-9]+\\.[0-9]+(\\.[0-9]+)?" ELD_VERSION
             ${PACKAGE_VERSION})
info("ELD version: ${ELD_VERSION}")

string(REGEX REPLACE "([0-9]+)\\.[0-9]+(\\.[0-9]+)?" "\\1" ELD_VERSION_MAJOR
                     ${ELD_VERSION})
string(REGEX REPLACE "[0-9]+\\.([0-9]+)(\\.[0-9]+)?" "\\1" ELD_VERSION_MINOR
                     ${ELD_VERSION})

# ##############################################################################
# Support linker version
# ##############################################################################
add_version_info_eld_git(ELD_REPOSITORY ELD_REVISION ${ELD_SOURCE_DIR})

if(LLVM_VENDOR_NAME)
  set(LLVM_VENDOR_NAME ${LLVM_VENDOR_NAME})
else()
  info("Use LLVM_VENDOR_NAME to set more vendor name")
endif()

if(LLVM_VENDOR_VERSION)
  set(LLVM_VENDOR_VERSION ${LLVM_VENDOR_VERSION})
else()
  info("Use LLVM_VENDOR_VERSION to set more vendor version")
endif()

# Configures everything in the Version.inc file except ELD_REVISION.
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/include/eld/Config/Version.inc.in
               ${CMAKE_CURRENT_BINARY_DIR}/include/eld/Config/Version.inc.in @ONLY)
# 'ELD_VERSION_PRE_CONFIGURED_FILE' and 'ELD_VERSION_POST_CONFIGURED'
# variables are required in 'ELDVersionMonitor'.
set(ELD_VERSION_PRE_CONFIGURED_FILE
  ${CMAKE_CURRENT_BINARY_DIR}/include/eld/Config/Version.inc.in)
set(ELD_VERSION_POST_CONFIGURED_FILE
  ${CMAKE_CURRENT_BINARY_DIR}/include/eld/Config/Version.inc)
include(ELDVersionMonitor)

set(ELD_DEFAULT_TARGET_TRIPLE
    "${LLVM_DEFAULT_TARGET_TRIPLE}"
    CACHE STRING "Default target for which LLVM will generate code.")

# ##############################################################################
# ELD_RUNTIME_OUTPUT_DIR is needed since CMAKE_RUNTIME_OUTPUT_DIRECTORY appends
# config type by default.
set(ELD_RUNTIME_OUTPUT_DIR ${CMAKE_BINARY_DIR}/${CMAKE_CFG_INTDIR}/bin)

# These two variables are for runnable lit tests and should be set to the
# 32/64-bit emulators used to run the tests.
set(ELD_EMULATOR "")
set(ELD_EMULATOR64 "")

set(ELD_ALL_TARGETS Hexagon AArch64 ARM RISCV X86_64)

set(ELD_TARGETS_TO_BUILD
    ${LLVM_TARGETS_TO_BUILD}
    CACHE STRING "ELD Target(Targets) to build")
if(ELD_TARGETS_TO_BUILD STREQUAL "all")
  set(ELD_TARGETS_TO_BUILD ${ELD_ALL_TARGETS})
endif()


# set ELD_ENUM_TARGETS="ELD_TARGET($target_to_build)
set(ELD_ENUM_TARGETS_STR "")
set(ELD_ENUM_LINKERS_STR "")
set(ELD_DEFINE_TARGETS_STR "")
foreach(t ${ELD_TARGETS_TO_BUILD})
  set(ELD_ENUM_TARGETS_STR ${ELD_ENUM_TARGETS_STR} "ELD_TARGET(${t})\n")
  set(ELD_ENUM_LINKERS_STR ${ELD_ENUM_LINKERS_STR} "ELD_LINKER(${t})\n")
  string(TOUPPER ${t} TARGET_UPPER)
  info("ELD built with support for ${t}")
  set(ELD_ENABLE_TARGET_${TARGET_UPPER} ON CACHE BOOL ON FORCE)
endforeach()

string(REPLACE ";" "" ELD_ENUM_TARGETS "${ELD_ENUM_TARGETS_STR}")
string(REPLACE ";" "" ELD_ENUM_LINKERS "${ELD_ENUM_LINKERS_STR}")

configure_file(${ELD_SOURCE_DIR}/include/eld/Config/Targets.def.cmake
               ${ELD_BINARY_DIR}/include/eld/Config/Targets.def)

if(WIN32)
  if(CYGWIN)
    set(ELD_ON_WIN32 0)
    set(ELD_ON_UNIX 1)
  else(CYGWIN)
    set(ELD_ON_WIN32 1)
    set(ELD_ON_UNIX 0)
  endif(CYGWIN)
else(WIN32)
  if(UNIX)
    set(ELD_ON_WIN32 0)
    set(ELD_ON_UNIX 1)
  else(UNIX)
    message(SEND_ERROR "Unable to determine platform")
  endif(UNIX)
endif(WIN32)

# If MSVC set the appropriate flags as well.
if(NOT WIN32)
  set(ELD_ON_MSVC 0)
  info("Setting up build of ELD for Unix")
else()
  set(ELD_ON_MSVC 1)
  info("Setting up build of ELD for Windows")
endif()

if(UNIX)
  set(CMAKE_BUILD_WITH_INSTALL_RPATH TRUE)
  set(CMAKE_INSTALL_RPATH_USE_LINK_PATH TRUE)
  set(CMAKE_INSTALL_RPATH "\$ORIGIN/../lib")
endif()

# Configure the three ELD configuration header files.
configure_file(${ELD_SOURCE_DIR}/include/eld/Config/Config.h.cmake
               ${ELD_BINARY_DIR}/include/eld/Config/Config.h)
configure_file(${ELD_SOURCE_DIR}/include/eld/Config/Linkers.def.cmake
               ${ELD_BINARY_DIR}/include/eld/Config/Linkers.def)
configure_file(${ELD_SOURCE_DIR}/include/eld/Config/Targets.def.cmake
               ${ELD_BINARY_DIR}/include/eld/Config/Targets.def)

include_directories(
  BEFORE
  ${CMAKE_CURRENT_BINARY_DIR}/include
  ${CMAKE_CURRENT_BINARY_DIR}/include/eld/PluginAPI/
  ${CMAKE_CURRENT_SOURCE_DIR}/include
  ${CMAKE_CURRENT_SOURCE_DIR}/include/eld/PluginAPI/)

option(ELD_COVERAGE "Build ELD with coverage" OFF)

if(ELD_COVERAGE)
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} --coverage")
endif()

option(ELD_SANITIZE "Build ELD with Sanitizer" OFF)

if(LLVM_USE_SANITIZER OR ELD_SANITIZE)
  info("Sanitizing Linker for ${LLVM_USE_SANITIZER}")
  if(LLVM_USE_SANITIZER MATCHES "Undefined")
    set(BLACKLIST_FILE
        "${ELD_SOURCE_DIR}/utils/sanitizers/ubsan_blacklist.txt")
    if(EXISTS "${BLACKLIST_FILE}")
      append("-fsanitize-blacklist=${BLACKLIST_FILE}" CMAKE_C_FLAGS
             CMAKE_CXX_FLAGS)
    endif()
  endif()
  if(NOT ELD_SANITIZE)
    set(ELD_SANITIZE ON)
  endif()
  if(LLVM_USE_SANITIZER MATCHES "Thread")
    set(ELD_THREAD_SANITIZE ON)
  endif()
endif()

set(CMAKE_INCLUDE_CURRENT_DIR ON)
include_directories(BEFORE ${CMAKE_CURRENT_BINARY_DIR}/include
                    ${CMAKE_CURRENT_SOURCE_DIR}/include)
link_directories("${PATH_TO_LLVM_BUILD}/lib")

set(ENABLE_ELD_PLUGIN_SUPPORT
    ON
    CACHE BOOL ON FORCE)

add_subdirectory(include)
add_subdirectory(lib)
add_subdirectory(tools)
add_subdirectory(docs)
add_subdirectory(test)

install(
  DIRECTORY templates/
  DESTINATION templates
  COMPONENT linker-script
  FILES_MATCHING
  PATTERN "*.template"
  PATTERN ".git" EXCLUDE
  PATTERN ".svn" EXCLUDE)

set(ELD_INSTALL_YAML_MAP_PARSER
    ON
    CACHE BOOL "Set to OFF to skip installing YAMLMapParser.py")

if(ELD_INSTALL_YAML_MAP_PARSER)
  if("${ELD_TARGETS_TO_BUILD}" MATCHES "ARM|AArch64|RISCV")
    set(ELD_YAML_MAP_PARSER_DEST_DIR "tools/bin")
  else()
    set(ELD_YAML_MAP_PARSER_DEST_DIR "bin")
  endif()
  install(PROGRAMS utils/YAMLMapParser/YAMLMapParser.py
          DESTINATION ${ELD_YAML_MAP_PARSER_DEST_DIR})
endif()

