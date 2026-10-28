# Helpers for building host plugins against an installed ELD package.
# Package discovery and API checks live in FindELD.cmake.

# Include BEFORE project() so compiler probes use the same library search path
# as executable, shared, and module targets. Compiler/header selection remains
# with CMAKE_CXX_COMPILER / CMAKE_CXX_FLAGS.
set(ELD_CXX_RUNTIME_DIR "" CACHE PATH
  "Override the package host libc++ library directory")

# Remove only the prefix installed by this helper on the previous configure.
# This permits changing packages without accumulating flags, and preserves
# the user's other linker options.
foreach(_kind EXE SHARED MODULE)
  set(_var "CMAKE_${_kind}_LINKER_FLAGS")
  if(DEFINED ${_var})
    set(_flags "${${_var}}")
  else()
    set(_flags "$ENV{LDFLAGS} ${${_var}_INIT}")
    string(STRIP "${_flags}" _flags)
  endif()
  if(_ELD_PACKAGE_RUNTIME_LINK_PREFIX)
    string(FIND "${_flags}" "${_ELD_PACKAGE_RUNTIME_LINK_PREFIX}" _pos)
    if(_pos EQUAL 0)
      string(LENGTH "${_ELD_PACKAGE_RUNTIME_LINK_PREFIX}" _length)
      string(SUBSTRING "${_flags}" ${_length} -1 _flags)
    endif()
  endif()
  set(_eld_runtime_${_kind}_flags "${_flags}")
endforeach()

set(_eld_runtime_prefix "")
set(ELD_PACKAGE_CXX_RUNTIME_DIR "")
if(CMAKE_HOST_SYSTEM_NAME STREQUAL "Linux")
  # Never fall back to the system libc++.so: that is precisely the accidental
  # runtime selection this helper is intended to prevent. Resolve afresh so
  # reconfiguration cannot reuse a cached result from another package.
  unset(_eld_package_libcxx CACHE)
  if(ELD_CXX_RUNTIME_DIR)
    find_file(_eld_package_libcxx NAMES libc++.so
      PATHS "${ELD_CXX_RUNTIME_DIR}" NO_DEFAULT_PATH)
  else()
    find_file(_eld_package_libcxx NAMES libc++.so
      HINTS "${ELD_PACKAGE_ROOT}" "${ELD_ROOT}" ENV ELD_ROOT
            ${CMAKE_PREFIX_PATH}
      PATH_SUFFIXES lib lib64 Tools/lib Tools/lib64 NO_DEFAULT_PATH)
  endif()
  if(NOT _eld_package_libcxx)
    message(FATAL_ERROR
      "Standalone plugin building requires the package host libc++.so; "
      "set ELD_PACKAGE_ROOT or ELD_CXX_RUNTIME_DIR. No system fallback is used.")
  endif()
  get_filename_component(ELD_PACKAGE_CXX_RUNTIME_DIR
    "${_eld_package_libcxx}" DIRECTORY)
  unset(_eld_package_libcxx CACHE)
  if(NOT EXISTS "${ELD_PACKAGE_CXX_RUNTIME_DIR}/libc++abi.so")
    message(FATAL_ERROR
      "Standalone plugin building requires libc++abi.so in "
      "${ELD_PACKAGE_CXX_RUNTIME_DIR}")
  endif()
  set(_eld_runtime_prefix "-L\"${ELD_PACKAGE_CXX_RUNTIME_DIR}\" ")
  message(STATUS "ELD package C++ runtime: ${ELD_PACKAGE_CXX_RUNTIME_DIR}")
endif()

if(ELD_PACKAGE_CXX_RUNTIME_DIR OR _ELD_PACKAGE_RUNTIME_LINK_PREFIX)
  foreach(_kind EXE SHARED MODULE)
    set(CMAKE_${_kind}_LINKER_FLAGS
      "${_eld_runtime_prefix}${_eld_runtime_${_kind}_flags}"
      CACHE STRING "Flags used by the linker" FORCE)
    set(CMAKE_${_kind}_LINKER_FLAGS
      "${_eld_runtime_prefix}${_eld_runtime_${_kind}_flags}")
  endforeach()
endif()
set(_ELD_PACKAGE_RUNTIME_LINK_PREFIX "${_eld_runtime_prefix}"
  CACHE INTERNAL "Library search prefix managed by ELDStandalone" FORCE)

# Call after project() and find_package(ELD): compilation and package/header
# checks need the enabled host compiler and the resolved ELD installation.
function(eld_check_standalone_runtime)
  if(NOT ELD_PACKAGE_CXX_RUNTIME_DIR)
    return()
  endif()
  if(NOT ELD_CXX_RUNTIME_DIR)
    file(REAL_PATH "${ELD_PACKAGE_CXX_RUNTIME_DIR}" _eld_runtime_real)
    file(REAL_PATH "${ELD_TOOLS_DIR}/lib" _eld_lib_real)
    file(REAL_PATH "${ELD_TOOLS_DIR}/lib64" _eld_lib64_real)
    if(NOT _eld_runtime_real STREQUAL _eld_lib_real AND
       NOT _eld_runtime_real STREQUAL _eld_lib64_real)
      message(FATAL_ERROR
        "ELD headers/library and C++ runtime resolved to different packages. "
        "Use a fresh build directory when changing ELD packages, or set "
        "ELD_CXX_RUNTIME_DIR explicitly for a custom host runtime.")
    endif()
  endif()
  include(CheckCXXSourceCompiles)
  unset(ELD_PACKAGE_LIBCXX_WORKS CACHE)
  check_cxx_source_compiles("
    #include <string>
    #ifndef _LIBCPP_VERSION
    #error Select libc++ headers with a compatible host compiler/toolchain
    #endif
    int main() {
      std::string s(100, 'x');
      return s.size() != 100;
    }" ELD_PACKAGE_LIBCXX_WORKS)
  if(NOT ELD_PACKAGE_LIBCXX_WORKS)
    message(FATAL_ERROR
      "Standalone plugin building requires compatible libc++ headers and "
      "libraries. Select a compatible host Clang and "
      "CMAKE_CXX_FLAGS=-stdlib=libc++ (or a host toolchain file).")
  endif()
endfunction()

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
  # Package selection is link-time only. The loading environment supplies
  # runtime search paths; do not embed the build package's installation path.
  # Override inherited CMAKE_*RPATH settings as well as automatic link paths.
  set_target_properties("${target}" PROPERTIES
    SKIP_BUILD_RPATH TRUE
    BUILD_RPATH ""
    BUILD_WITH_INSTALL_RPATH FALSE
    INSTALL_RPATH ""
    INSTALL_RPATH_USE_LINK_PATH FALSE)
endfunction()
