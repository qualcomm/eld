# Locate the public ELD plugin headers and host LinkerWrapper library.
#
# Hints: ELD_PACKAGE_ROOT, ELD_ROOT, or CMAKE_PREFIX_PATH.
# Both <prefix>/{include,lib,bin} and <prefix>/Tools/{include,lib,bin}
# are accepted. No LLVM development package or target compiler is required.
#
# Results: ELD_FOUND, ELD_INCLUDE_DIR, ELD_INCLUDE_ROOT, ELD_LIBRARY,
# ELD_LIBRARY_DIR, ELD_TOOLS_DIR, and imported target ELD::LW.
# On Windows ELD_LIBRARY is the import library and ELD_RUNTIME_LIBRARY is
# the DLL. Cache entries can be overridden for nonstandard installations.

set(ELD_PACKAGE_ROOT "" CACHE PATH "Optional root of an installed ELD package")
find_path(ELD_INCLUDE_DIR NAMES LinkerPlugin.h
  HINTS "${ELD_PACKAGE_ROOT}"
  PATH_SUFFIXES include/ELD/PluginAPI include/eld/PluginAPI
                Tools/include/ELD/PluginAPI Tools/include/eld/PluginAPI)

set(ELD_API_HEADERS_FOUND FALSE)
if(ELD_INCLUDE_DIR)
  get_filename_component(_eld_include_parent "${ELD_INCLUDE_DIR}" DIRECTORY)
  get_filename_component(ELD_INCLUDE_ROOT "${_eld_include_parent}" DIRECTORY)
  get_filename_component(ELD_TOOLS_DIR "${ELD_INCLUDE_ROOT}" DIRECTORY)
  set(ELD_API_HEADERS_FOUND TRUE)
  foreach(_header LinkerPlugin.h PluginBase.h PluginADT.h Defines.h)
    if(NOT EXISTS "${ELD_INCLUDE_DIR}/${_header}")
      set(ELD_API_HEADERS_FOUND FALSE)
    endif()
  endforeach()

  # Find the normal linker name. The host linker records the selected
  # library's SONAME, so the build does not need to infer it from the API.
  find_library(ELD_LIBRARY NAMES LW
    PATHS "${ELD_TOOLS_DIR}/lib" "${ELD_TOOLS_DIR}/lib64"
    NO_DEFAULT_PATH)
  if(WIN32)
    find_file(ELD_RUNTIME_LIBRARY NAMES LW.dll libLW.dll
      PATHS "${ELD_TOOLS_DIR}/bin" "${ELD_TOOLS_DIR}/lib"
      NO_DEFAULT_PATH)
  endif()
endif()

set(_eld_required ELD_INCLUDE_DIR ELD_API_HEADERS_FOUND ELD_LIBRARY)
if(WIN32)
  list(APPEND _eld_required ELD_RUNTIME_LIBRARY)
endif()
include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(ELD
  REQUIRED_VARS ${_eld_required}
  REASON_FAILURE_MESSAGE
    "Set ELD_PACKAGE_ROOT or CMAKE_PREFIX_PATH to a complete ELD installation.")
mark_as_advanced(ELD_INCLUDE_DIR ELD_LIBRARY ELD_RUNTIME_LIBRARY)

if(ELD_FOUND)
  get_filename_component(ELD_LIBRARY_DIR "${ELD_LIBRARY}" DIRECTORY)
  if(NOT TARGET ELD::LW)
    add_library(ELD::LW SHARED IMPORTED GLOBAL)
    set_target_properties(ELD::LW PROPERTIES
      INTERFACE_INCLUDE_DIRECTORIES "${ELD_INCLUDE_DIR};${ELD_INCLUDE_ROOT}")
    if(WIN32)
      set_target_properties(ELD::LW PROPERTIES
        IMPORTED_IMPLIB "${ELD_LIBRARY}"
        IMPORTED_LOCATION "${ELD_RUNTIME_LIBRARY}")
    else()
      set_target_properties(ELD::LW PROPERTIES
        IMPORTED_LOCATION "${ELD_LIBRARY}")
    endif()
  endif()
endif()

# Verify that an installed ELD package declares a public PluginAPI symbol.
#
#   eld_standalone_require_api(<plugin> <header> <symbol_regex>)
#
# <plugin> is the plugin target or another descriptive name used in a
# configuration error. <header> is the PluginAPI header path relative to
# ELD_INCLUDE_DIR, for example LinkerWrapper.h. <symbol_regex> is a CMake
# regular expression searched in that header; escape regex metacharacters when
# matching a literal spelling. This is an early availability check only: the
# plugin's normal compilation still validates the declaration's type and use.
function(eld_standalone_require_api plugin header symbol_regex)
  set(_header "${ELD_INCLUDE_DIR}/${header}")
  if(NOT EXISTS "${_header}")
    message(FATAL_ERROR "${plugin} requires ELD API header ${_header}")
  endif()
  file(READ "${_header}" _contents)
  if(NOT _contents MATCHES "${symbol_regex}")
    message(FATAL_ERROR
      "${plugin} requires ELD API matching '${symbol_regex}' in '${header}', but the "
      "installed ELD package does not provide it")
  endif()
endfunction()
