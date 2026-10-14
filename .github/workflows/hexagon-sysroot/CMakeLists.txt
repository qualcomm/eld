# CMakeLists.txt — Hexagon sysroot orchestrator
#
# Do not invoke directly.  See build-hexagon-sysroot.cmake for the intended
# usage.  This file validates the user-supplied inputs, derives shared paths,
# and includes the four component modules.

cmake_minimum_required(VERSION 3.20)

# project(... NONE) — we don't build anything with the host toolchain here.
# All actual compilation is done inside ExternalProject sub-builds that use
# the installed Hexagon LLVM toolchain (${TOOLCHAIN}/bin/clang etc.).
project(HexagonSysroot NONE)

include(ExternalProject)

# ---------------------------------------------------------------------------
# Validate required inputs
# ---------------------------------------------------------------------------
foreach(var IN ITEMS TOOLCHAIN SOURCECODE PICOLIBC_SRC H2_SRC)
    if(NOT ${var})
        message(FATAL_ERROR
            "${var} is not set.  Pass -D${var}=<path> on the cmake command line.")
    endif()
    if(NOT IS_DIRECTORY "${${var}}")
        message(FATAL_ERROR "${var} = '${${var}}' is not a directory.")
    endif()
endforeach()

# ---------------------------------------------------------------------------
# Derived paths
# ---------------------------------------------------------------------------
set(CLANG   "${TOOLCHAIN}/bin/clang"    CACHE INTERNAL "")
set(CLANGXX "${TOOLCHAIN}/bin/clang++"  CACHE INTERNAL "")

if(NOT EXISTS "${CLANG}")
    message(FATAL_ERROR "Hexagon clang not found at ${CLANG}")
endif()
if(NOT EXISTS "${CLANGXX}")
    message(FATAL_ERROR "Hexagon clang++ not found at ${CLANGXX}")
endif()

# INSTALLPATH is the parent of TOOLCHAIN so that:
#   ${INSTALLPATH}/Tools  ==  ${TOOLCHAIN}
# and everything the scripts install under ${INSTALLPATH}/Tools/target/picolibc/
# lands at ${TOOLCHAIN}/target/picolibc/ — exactly where clang's compiled-in
# sysroot lookup (bin/../target/picolibc/<triple>/) resolves.
cmake_path(GET TOOLCHAIN PARENT_PATH INSTALLPATH)
set(INSTALLPATH "${INSTALLPATH}" CACHE INTERNAL "")

# Final install root (matches FINAL_INSTALL in the bash scripts).
set(FINAL_INSTALL "${TOOLCHAIN}" CACHE INTERNAL "")

# Sysroot subtree under target/picolibc/ for each triple.
set(SYSROOT_ROOT           "${FINAL_INSTALL}/target/picolibc" CACHE INTERNAL "")
set(SYSROOT_NONE_ELF       "${SYSROOT_ROOT}/hexagon-unknown-none-elf" CACHE INTERNAL "")
set(SYSROOT_H2_ELF         "${SYSROOT_ROOT}/hexagon-unknown-h2-elf" CACHE INTERNAL "")
set(SYSROOT_QURT_ELF       "${SYSROOT_ROOT}/hexagon-unknown-qurt-elf" CACHE INTERNAL "")

# ---------------------------------------------------------------------------
# Resolve JOBS (empty = number of logical cores on the build host)
# ---------------------------------------------------------------------------
if(NOT JOBS)
    cmake_host_system_information(RESULT JOBS QUERY NUMBER_OF_LOGICAL_CORES)
endif()
set(JOBS "${JOBS}" CACHE INTERNAL "")

# ---------------------------------------------------------------------------
# Convert space-separated core lists to cmake list form.  SOURCE_CORE is the
# first entry in BUILD_CORES — non-built cores get per-file symlinks into it.
# ---------------------------------------------------------------------------
string(REPLACE " " ";" BUILD_CORES_LIST "${BUILD_CORES}")
string(REPLACE " " ";" ALL_CORES_LIST   "${ALL_CORES}")
list(GET BUILD_CORES_LIST 0 SOURCE_CORE)
set(BUILD_CORES_LIST "${BUILD_CORES_LIST}" CACHE INTERNAL "")
set(ALL_CORES_LIST   "${ALL_CORES_LIST}"   CACHE INTERNAL "")
set(SOURCE_CORE      "${SOURCE_CORE}"      CACHE INTERNAL "")

# Variants and their per-variant compiler flags (mirrors the bash scripts).
set(VARIANTS "non-G0;G0;G0-pic" CACHE INTERNAL "")

# ---------------------------------------------------------------------------
# Locate build tools
# ---------------------------------------------------------------------------
find_program(NINJA_EXECUTABLE ninja REQUIRED)
find_program(MESON_EXECUTABLE meson REQUIRED)
find_program(MAKE_EXECUTABLE  make  REQUIRED)
# qemu-system-hexagon is required by the H2 build (runs the asm_offsets
# generator ELF in a Hexagon system emulator).  Located at configure time
# so the full path can be baked into the H2 build environment; a user
# adding it to PATH after cmake configure would not be visible otherwise
# because cmake -E env captures $ENV{PATH} at configure time.
find_program(QEMU_SYSTEM_HEXAGON qemu-system-hexagon REQUIRED)

# ---------------------------------------------------------------------------
# Diagnostic banner
# ---------------------------------------------------------------------------
message(STATUS "=== Hexagon Sysroot Build ===")
message(STATUS "  TOOLCHAIN     = ${TOOLCHAIN}")
message(STATUS "  SOURCECODE    = ${SOURCECODE}")
message(STATUS "  PICOLIBC_SRC  = ${PICOLIBC_SRC}")
message(STATUS "  H2_SRC        = ${H2_SRC}")
message(STATUS "  INSTALLPATH   = ${INSTALLPATH}")
message(STATUS "  SYSROOT_ROOT  = ${SYSROOT_ROOT}")
message(STATUS "  BUILD_CORES   = ${BUILD_CORES}")
message(STATUS "  ALL_CORES     = ${ALL_CORES}")
message(STATUS "  SOURCE_CORE   = ${SOURCE_CORE}")
message(STATUS "  VARIANTS      = ${VARIANTS}")
message(STATUS "  JOBS          = ${JOBS}")
message(STATUS "  ninja         = ${NINJA_EXECUTABLE}")
message(STATUS "  meson         = ${MESON_EXECUTABLE}")
message(STATUS "  make          = ${MAKE_EXECUTABLE}")
message(STATUS "  qemu          = ${QEMU_SYSTEM_HEXAGON}")

# ---------------------------------------------------------------------------
# Include the four component modules.  Each defines its own ExternalProject
# targets and the appropriate DEPENDS chain.
# ---------------------------------------------------------------------------
include(${CMAKE_CURRENT_LIST_DIR}/HexagonSysrootBuiltins.cmake)
include(${CMAKE_CURRENT_LIST_DIR}/HexagonSysrootPicolibc.cmake)
include(${CMAKE_CURRENT_LIST_DIR}/HexagonSysrootH2.cmake)
include(${CMAKE_CURRENT_LIST_DIR}/HexagonSysrootRuntimes.cmake)

# TODO(sysroot): once the cmake build is validated in production, replace the
# four bash scripts under sysroot-scripts/ and update
# .github/workflows/build-sysroot.yml to invoke `cmake -C ... -B build/` and
# `cmake --build build/` in place of the four `bash sysroot-scripts/*.sh` steps.
