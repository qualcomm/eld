# HexagonSysrootPicolibc.cmake
#
# Builds picolibc for hexagon-unknown-none-elf, per (core, variant), using
# meson.  Mirrors sysroot-scripts/build_hexagon_picolibc.sh.
#
# Targets:
#   picolibc-<core>-<variant>          e.g. picolibc-v68-G0
#   picolibc-symlinks                  fan-out to non-built cores + h2/qurt
#   picolibc                           umbrella

# ---------------------------------------------------------------------------
# Per-variant meson c_args and meson options
#
#   variant     extra_cflags                        meson extra opts
#   non-G0      -fno-pic -fno-PIE -static           (none)
#   G0          -fno-pic -fno-PIE -static -G0       (none)
#   G0-pic      -fPIC    -static -G0                -Dtls-model=local-dynamic
# ---------------------------------------------------------------------------
function(_hexagon_picolibc_variant_flags variant out_suffix out_extra_cflags out_meson_opts)
    if(variant STREQUAL "non-G0")
        set(${out_suffix}       ""    PARENT_SCOPE)
        set(${out_extra_cflags} "-fno-pic;-fno-PIE;-static" PARENT_SCOPE)
        set(${out_meson_opts}   "" PARENT_SCOPE)
    elseif(variant STREQUAL "G0")
        set(${out_suffix}       "-G0" PARENT_SCOPE)
        set(${out_extra_cflags} "-fno-pic;-fno-PIE;-static;-G0" PARENT_SCOPE)
        set(${out_meson_opts}   "" PARENT_SCOPE)
    elseif(variant STREQUAL "G0-pic")
        set(${out_suffix}       "-G0-pic" PARENT_SCOPE)
        set(${out_extra_cflags} "-fPIC;-static;-G0" PARENT_SCOPE)
        set(${out_meson_opts}   "-Dtls-model=local-dynamic" PARENT_SCOPE)
    else()
        message(FATAL_ERROR "Unknown variant: ${variant}")
    endif()
endfunction()

# Common c_args added after per-variant extras (matches bash script).
set(_picolibc_common_cflags
    "--target=hexagon-unknown-none-elf"
    "--cstdlib=picolibc"
    "-nostdlib"
    "-ffunction-sections"
    "-fdata-sections"
    "-fvisibility=hidden"
)

# Common c_link_args (identical for all variants).
set(_picolibc_common_link_args
    "--target=hexagon-unknown-none-elf"
    "--cstdlib=picolibc"
    "-nostdlib"
)

# ---------------------------------------------------------------------------
# Helper: format a semicolon-separated cmake list as a meson array literal,
# e.g. "-fPIC;-static" -> "[ '-fPIC', '-static' ]"
# ---------------------------------------------------------------------------
function(_meson_array_literal list_var out_var)
    set(_items "")
    foreach(_item IN LISTS ${list_var})
        list(APPEND _items "'${_item}'")
    endforeach()
    list(JOIN _items ", " _joined)
    set(${out_var} "[ ${_joined} ]" PARENT_SCOPE)
endfunction()

# ---------------------------------------------------------------------------
# Per-(core, variant) ExternalProject
# ---------------------------------------------------------------------------
set(_picolibc_targets "")

foreach(core IN LISTS BUILD_CORES_LIST)
    foreach(variant IN LISTS VARIANTS)
        _hexagon_picolibc_variant_flags(${variant}
            _suffix _extra_cflags _meson_extra)

        set(_target       "picolibc-${core}-${variant}")
        set(_build_dir    "${CMAKE_BINARY_DIR}/picolibc/${core}${_suffix}")
        set(_install_dir  "${CMAKE_BINARY_DIR}/picolibc-install/${core}${_suffix}")
        set(_dest_lib_dir "${SYSROOT_NONE_ELF}/lib/${core}${_suffix}")
        set(_cross_file   "${CMAKE_BINARY_DIR}/picolibc/cross-clang-hexagon-${core}${_suffix}.txt")

        # Build the full c_args list: per-variant extras + -m<core> + common.
        set(_c_args_list ${_extra_cflags} "-m${core}" ${_picolibc_common_cflags})
        _meson_array_literal(_c_args_list      C_ARGS)
        _meson_array_literal(_picolibc_common_link_args C_LINK_ARGS)

        # Generate the meson cross-file via configure_file at cmake configure
        # time.  @CLANG@ / @CLANGXX@ / @C_ARGS@ / @C_LINK_ARGS@ are substituted.
        configure_file(
            "${CMAKE_CURRENT_LIST_DIR}/HexagonPicolibcCrossFile.cmake.in"
            "${_cross_file}"
            @ONLY
        )

        # ExternalProject_Add for a meson build.  Every step (configure,
        # build, install) is wrapped with `cmake -E env` to prepend the
        # toolchain's bin/ to PATH so that bare tool names in meson's
        # generated build.ninja (llvm-ar, llvm-strip, eld, etc.)
        # resolve to the correct toolchain binaries.
        ExternalProject_Add(${_target}
            SOURCE_DIR       "${PICOLIBC_SRC}"
            BINARY_DIR       "${_build_dir}"
            INSTALL_DIR      "${_install_dir}"
            CONFIGURE_COMMAND
                ${CMAKE_COMMAND} -E env
                    "PATH=${TOOLCHAIN}/bin:$ENV{PATH}"
                ${MESON_EXECUTABLE} setup
                    --buildtype=release
                    --cross-file=${_cross_file}
                    --prefix=${_install_dir}
                    -Dtests=false
                    -Dstdio-locking=true
                    -Dstdio-exit-flush=true
                    -Dmultilib=false
                    -Dposix-console=true
                    ${_meson_extra}
                    <BINARY_DIR>
                    <SOURCE_DIR>
            BUILD_COMMAND
                ${CMAKE_COMMAND} -E env
                    "PATH=${TOOLCHAIN}/bin:$ENV{PATH}"
                ${NINJA_EXECUTABLE} -C <BINARY_DIR> -j${JOBS}
            INSTALL_COMMAND
                ${CMAKE_COMMAND} -E env
                    "PATH=${TOOLCHAIN}/bin:$ENV{PATH}"
                ${NINJA_EXECUTABLE} -C <BINARY_DIR> install
            COMMAND
                ${CMAKE_COMMAND} -E make_directory ${_dest_lib_dir}
            COMMAND
                ${CMAKE_COMMAND}
                    -DMODE=picolibc-copy-libs
                    -DSRC_LIB_DIR=${_install_dir}/lib
                    -DDEST_LIB_DIR=${_dest_lib_dir}
                    -P ${CMAKE_CURRENT_LIST_DIR}/HexagonInstallSymlinks.cmake
            BUILD_ALWAYS FALSE
            DEPENDS builtins
        )

        list(APPEND _picolibc_targets ${_target})
    endforeach()
endforeach()

# ---------------------------------------------------------------------------
# Fan-out symlinks + headers install + h2-elf symlinks + qurt-elf copy.
#
# Headers come from the SOURCE_CORE-G0 install (identical across variants).
# ---------------------------------------------------------------------------
add_custom_target(picolibc-symlinks
    COMMAND ${CMAKE_COMMAND}
        -DMODE=picolibc-fanout
        -DSYSROOT_NONE_ELF=${SYSROOT_NONE_ELF}
        -DSYSROOT_H2_ELF=${SYSROOT_H2_ELF}
        -DSYSROOT_QURT_ELF=${SYSROOT_QURT_ELF}
        -DSOURCE_CORE=${SOURCE_CORE}
        "-DBUILD_CORES_LIST=${BUILD_CORES_LIST}"
        "-DALL_CORES_LIST=${ALL_CORES_LIST}"
        -DPICOLIBC_HEADERS_SRC=${CMAKE_BINARY_DIR}/picolibc-install/${SOURCE_CORE}-G0/include
        -P ${CMAKE_CURRENT_LIST_DIR}/HexagonInstallSymlinks.cmake
    DEPENDS ${_picolibc_targets}
    COMMENT "Fanning out picolibc to non-built cores + h2/qurt + installing headers"
    VERBATIM
)

add_custom_target(picolibc ALL DEPENDS picolibc-symlinks)
