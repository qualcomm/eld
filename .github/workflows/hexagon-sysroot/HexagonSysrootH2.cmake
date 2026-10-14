# HexagonSysrootH2.cmake
#
# Builds H2 libraries for Hexagon base architectures 68, 73, 81 via `make`.
# Mirrors sysroot-scripts/build_hexagon_h2.sh.
#
# Targets:
#   h2-base-68 / h2-base-73 / h2-base-81   (one make invocation each)
#   h2-fanout                              per-version fanout + install into sysroot
#   h2                                     umbrella
#
# The H2 build's offsets step compiles a Hexagon ELF and runs it under
# qemu-system-hexagon to generate asm_offsets.h.  We prepend both the
# toolchain's bin/ (for clang, hexagon-*) and qemu-system-hexagon's directory
# to PATH so make's $(shell $(CC) ...) invocations and the offsets runner
# can find their tools regardless of the user's shell PATH at build time.

# Hardcoded base-to-version fanout mapping (matches bash script).
set(_h2_versions_from_68 v68 v69 v71t v71)
set(_h2_versions_from_73 v73 v75 v77 v79)
set(_h2_versions_from_81 v81 v83 v85 v87 v89 v91)
set(_h2_all_versions ${_h2_versions_from_68} ${_h2_versions_from_73} ${_h2_versions_from_81})

# Extract the directory containing qemu-system-hexagon so we can add it
# to PATH for the H2 build.  QEMU_SYSTEM_HEXAGON is set in CMakeLists.txt.
cmake_path(GET QEMU_SYSTEM_HEXAGON PARENT_PATH _qemu_bin_dir)

set(_h2_base_targets "")
set(_h2_previous_target "")

foreach(base 68 73 81)
    set(_target      "h2-base-${base}")
    set(_install_dir "${CMAKE_BINARY_DIR}/h2-install-${base}")
    set(_stamp_dir   "${CMAKE_BINARY_DIR}/h2-stamps")
    set(_stamp       "${_stamp_dir}/h2-base-${base}.stamp")
    # libh2kernel.a is produced by the H2 install step for every base; using
    # it as the build byproduct lets cmake/ninja track completion.
    set(_byproduct   "${_install_dir}/lib/libh2kernel.a")

    # The H2 Makefile builds in-tree (BINARY_DIR = SOURCE_DIR = H2_SRC) and
    # is NOT parallel-safe across base architectures.  Force each base to
    # wait for the previous one by making its DEPENDS include the prior
    # h2-base-* target.  The first base only depends on picolibc.
    set(_deps picolibc)
    if(_h2_previous_target)
        list(APPEND _deps ${_h2_previous_target})
    endif()

    ExternalProject_Add(${_target}
        SOURCE_DIR       "${H2_SRC}"
        BINARY_DIR       "${H2_SRC}"           # H2 builds in-tree; not our choice
        INSTALL_DIR      "${_install_dir}"
        DOWNLOAD_COMMAND ""
        UPDATE_COMMAND   ""
        PATCH_COMMAND    ""
        CONFIGURE_COMMAND ""
        # Clean the shared H2 build tree AND the per-base install dir before
        # each base build.
        #
        # H2's Makefile derives its build directory as INSTALLPATH/../build/,
        # so every base build (which uses distinct INSTALLPATH values sharing
        # the same parent, i.e. ${CMAKE_BINARY_DIR}) writes to the SAME
        # ${CMAKE_BINARY_DIR}/build/ tree.  Without cleaning it between
        # bases, leftover .o files from the previous base would get archived
        # in this base's libraries, causing duplicate-symbol link errors.
        #
        # This matches the bash script's `rm -rf ${BUILDPATH}/build/` +
        # `rm -rf ${H2_INSTALL_ROOT}-${base}/` cleanup step.
        BUILD_COMMAND
            ${CMAKE_COMMAND} -E rm -rf ${CMAKE_BINARY_DIR}/build ${_install_dir}
        COMMAND
            ${CMAKE_COMMAND} -E env
                "PATH=${TOOLCHAIN}/bin:${_qemu_bin_dir}:$ENV{PATH}"
            ${MAKE_EXECUTABLE} -j1 -C ${H2_SRC}
                USE_PKW=0
                ARCHV=${base}
                TARGET=opt
                INSTALLPATH=${_install_dir}
                PICOLIBC=1
                NULL_ANGEL_TRAP=1
                JFLAG=-j1
        INSTALL_COMMAND
            ${CMAKE_COMMAND} -E make_directory ${_stamp_dir}
        COMMAND
            ${CMAKE_COMMAND} -E touch ${_stamp}
        BUILD_ALWAYS      FALSE
        BUILD_BYPRODUCTS  ${_byproduct} ${_stamp}
        DEPENDS           ${_deps}
    )

    list(APPEND _h2_base_targets ${_target})
    set(_h2_previous_target ${_target})
endforeach()

# ---------------------------------------------------------------------------
# Fan-out: copy each base install to its version-specific dirs, then install
# into the sysroot tree (bin/lib/include), then write h2-picolibc.cfg into
# ${TOOLCHAIN}/bin/.  All handled by HexagonInstallSymlinks.cmake in one shot.
# ---------------------------------------------------------------------------
add_custom_target(h2-fanout
    COMMAND ${CMAKE_COMMAND}
        -DMODE=h2-fanout
        -DBUILD_DIR=${CMAKE_BINARY_DIR}
        -DSYSROOT_H2_ELF=${SYSROOT_H2_ELF}
        -DFINAL_INSTALL=${FINAL_INSTALL}
        "-DH2_VERSIONS_FROM_68=${_h2_versions_from_68}"
        "-DH2_VERSIONS_FROM_73=${_h2_versions_from_73}"
        "-DH2_VERSIONS_FROM_81=${_h2_versions_from_81}"
        "-DH2_ALL_VERSIONS=${_h2_all_versions}"
        -P ${CMAKE_CURRENT_LIST_DIR}/HexagonInstallSymlinks.cmake
    DEPENDS ${_h2_base_targets}
    COMMENT "Fanning out H2 to per-version install trees + installing into sysroot"
    VERBATIM
)

add_custom_target(h2 ALL DEPENDS h2-fanout)
