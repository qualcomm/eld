# CMakeLists.txt for ELD Vim syntax-highlighting tests.
#
# Guarded by ELD_VIM_TESTS (OFF by default) in the parent CMakeLists.txt.
# Tests are run via lit and produce output under CMAKE_CURRENT_BINARY_DIR
# rather than the source tree.

# ── Locate vim ───────────────────────────────────────────────────────────────

find_program(VIM_EXECUTABLE NAMES vim DOC "Path to the vim binary")

if(NOT VIM_EXECUTABLE)
  message(WARNING
    "ELD_VIM_TESTS is ON but vim was not found in PATH. "
    "Skipping check-eld-vim target.")
  return()
endif()

# ── Version check: require vim > 9 ───────────────────────────────────────────

execute_process(
  COMMAND "${VIM_EXECUTABLE}" --version
  OUTPUT_VARIABLE VIM_VERSION_OUTPUT
  ERROR_QUIET
  OUTPUT_STRIP_TRAILING_WHITESPACE)

string(REGEX MATCH "VIM - Vi IMproved ([0-9]+)\\.([0-9]+)"
       _vim_ver_match "${VIM_VERSION_OUTPUT}")

if(NOT _vim_ver_match)
  message(WARNING
    "ELD_VIM_TESTS is ON but could not determine vim version from "
    "'${VIM_EXECUTABLE} --version'. Skipping check-eld-vim target.")
  return()
endif()

set(VIM_VERSION_MAJOR "${CMAKE_MATCH_1}")
set(VIM_VERSION_MINOR "${CMAKE_MATCH_2}")

message(STATUS "Found vim ${VIM_VERSION_MAJOR}.${VIM_VERSION_MINOR}: ${VIM_EXECUTABLE}")

if(VIM_VERSION_MAJOR LESS 9 OR
   (VIM_VERSION_MAJOR EQUAL 9 AND VIM_VERSION_MINOR LESS 1))
  message(WARNING
    "ELD_VIM_TESTS is ON but vim version ${VIM_VERSION_MAJOR}.${VIM_VERSION_MINOR} "
    "is less than 9.1. Skipping check-eld-vim target.")
  return()
endif()

# ── Locate Python 3 ──────────────────────────────────────────────────────────

find_package(Python3 QUIET COMPONENTS Interpreter)
if(NOT Python3_FOUND)
  message(WARNING
    "ELD_VIM_TESTS is ON but Python 3 was not found. "
    "Skipping check-eld-vim target.")
  return()
endif()
set(PYTHON_EXECUTABLE "${Python3_EXECUTABLE}")

# ── Paths ─────────────────────────────────────────────────────────────────────

set(ELD_VIM_TEST_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}")
set(ELD_VIM_TEST_EXEC_DIR   "${CMAKE_CURRENT_BINARY_DIR}")
set(ELD_VIM_BINARY          "${VIM_EXECUTABLE}")

# ── Generate lit site config ──────────────────────────────────────────────────

configure_lit_site_cfg(
  "${CMAKE_CURRENT_SOURCE_DIR}/lit.site.cfg.in"
  "${CMAKE_CURRENT_BINARY_DIR}/lit.site.cfg"
  MAIN_CONFIG "${CMAKE_CURRENT_SOURCE_DIR}/lit.cfg")

# ── check-eld-vim target ──────────────────────────────────────────────────────

if(TARGET check-eld)
  set(_check_eld_dep DEPENDS check-eld)
else()
  set(_check_eld_dep "")
endif()

add_lit_testsuite(check-eld-vim
  "Running ELD Vim syntax-highlighting tests"
  "${CMAKE_CURRENT_BINARY_DIR}"
  ARGS -j1
  ${_check_eld_dep})

set_target_properties(check-eld-vim PROPERTIES FOLDER "Linker Tests")
