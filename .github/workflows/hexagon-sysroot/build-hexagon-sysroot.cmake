# build-hexagon-sysroot.cmake
#
# CMake initial cache file for building the Hexagon sysroot on top of an
# existing Hexagon LLVM/Clang toolchain.
#
# Usage:
#   cmake -C cmake/hexagon-sysroot/build-hexagon-sysroot.cmake \
#         -DTOOLCHAIN=/path/to/Tools \
#         -DSOURCECODE=/path/to/llvm-project \
#         -DPICOLIBC_SRC=/path/to/picolibc \
#         -DH2_SRC=/path/to/hexagon-hypervisor \
#         -B build-dir/ \
#         -S cmake/hexagon-sysroot/
#
#   cmake --build build-dir/ -jN
#
# The four sysroot components (compiler-rt builtins, picolibc, H2, runtimes)
# are built as ExternalProjects with the following dependency chain:
#
#   builtins ──► picolibc ──► h2 ──► runtimes (none-elf + h2-elf)
#
# Each is built per (core, variant) triple.  Non-built cores get per-file
# symlinks into the built cores.  Everything installs directly under
# ${TOOLCHAIN}/target/picolibc/ so clang can find the sysroot for each
# subsequent step (matching clang's DEFAULT_SYSROOT of bin/../target/).

# ---------------------------------------------------------------------------
# Required inputs — no defaults; CMakeLists.txt will FATAL_ERROR if empty.
# Override with -D<VAR>=<path> on the cmake command line.
# ---------------------------------------------------------------------------
set(TOOLCHAIN    "" CACHE PATH "Installed Hexagon LLVM toolchain (contains bin/clang)")
set(SOURCECODE   "" CACHE PATH "llvm-project source tree (compiler-rt + runtimes)")
set(PICOLIBC_SRC "" CACHE PATH "picolibc source tree (contains meson.build)")
set(H2_SRC       "" CACHE PATH "hexagon-hypervisor source tree (contains makefile)")

# ---------------------------------------------------------------------------
# Optional tuning
# ---------------------------------------------------------------------------
set(BUILD_CORES "v68"
    CACHE STRING "Space-separated Hexagon arch versions to fully build")
set(ALL_CORES "v68 v69 v71 v71t v73 v75 v77 v79 v81 v83 v85 v87 v89 v91"
    CACHE STRING "Space-separated full set of Hexagon arch versions to install for")
set(JOBS "" CACHE STRING "Parallel jobs for ninja/make (empty = number of logical cores)")
