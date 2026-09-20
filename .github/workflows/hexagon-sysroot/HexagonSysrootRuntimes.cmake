# HexagonSysrootRuntimes.cmake
#
# Builds runtimes (libunwind, libcxxabi, libcxx) for two triples:
#   hexagon-unknown-none-elf   (baremetal: threads OFF, no localization, no wide chars)
#   hexagon-unknown-h2-elf     (H2: threads ON, localization ON, wide chars ON, H2 site defines)
#
# Per (triple, core, variant).  Mirrors sysroot-scripts/build_hexagon_runtimes.sh.
#
# Both triples depend on `h2` because the h2-elf runtimes need pthread.h
# from H2, and we keep none-elf runtimes serialized after H2 too for
# consistency with the bash-script build order.
#
# Targets:
#   runtimes-<triple-short>-<core>-<variant>   e.g. runtimes-none-elf-v68-G0
#   runtimes-symlinks                          fan-out to non-built cores
#   runtimes                                   umbrella

# ---------------------------------------------------------------------------
# Per-variant flag table (identical shape to builtins)
# ---------------------------------------------------------------------------
function(_hexagon_runtimes_variant_flags variant out_suffix out_g0 out_pic out_extra)
    if(variant STREQUAL "non-G0")
        set(${out_suffix} ""    PARENT_SCOPE)
        set(${out_g0}     ""    PARENT_SCOPE)
        set(${out_pic}    ""    PARENT_SCOPE)
        set(${out_extra}  "-O3 -ffunction-sections -fdata-sections" PARENT_SCOPE)
    elseif(variant STREQUAL "G0")
        set(${out_suffix} "-G0" PARENT_SCOPE)
        set(${out_g0}     "-G0" PARENT_SCOPE)
        set(${out_pic}    ""    PARENT_SCOPE)
        set(${out_extra}  "-O3 -ffunction-sections -fdata-sections" PARENT_SCOPE)
    elseif(variant STREQUAL "G0-pic")
        set(${out_suffix} "-G0-pic" PARENT_SCOPE)
        set(${out_g0}     "-G0"     PARENT_SCOPE)
        set(${out_pic}    "-fPIC"   PARENT_SCOPE)
        set(${out_extra}  "-O3 -ffunction-sections -fdata-sections -fvisibility=hidden" PARENT_SCOPE)
    else()
        message(FATAL_ERROR "Unknown variant: ${variant}")
    endif()
endfunction()

# ---------------------------------------------------------------------------
# Emit one ExternalProject_Add for a (triple, core, variant) combo
# ---------------------------------------------------------------------------
function(_hexagon_runtimes_add triple triple_short core variant
                               enable_threads monotonic_clock wide_chars localization
                               extra_cxx_flags extra_site_defines dest_base
                               out_target_var)
    _hexagon_runtimes_variant_flags(${variant} _suffix _g0 _pic _extra)

    set(_target      "runtimes-${triple_short}-${core}-${variant}")
    set(_build_dir   "${CMAKE_BINARY_DIR}/runtimes/${triple_short}-${core}${_suffix}")
    set(_install_dir "${CMAKE_BINARY_DIR}/runtimes-install/${triple_short}-${core}${_suffix}")

    # Bake --target=${triple} into every flag string.  The nightly clang is
    # multi-arch; without an explicit --target the driver (especially the ASM
    # driver, which CMAKE_C/CXX_COMPILER_TARGET does not reach) defaults to the
    # host triple and fails on libunwind's Hexagon .S sources.
    set(_c_flags   "--target=${triple} ${_g0} -m${core} ${_pic} ${_extra} --cstdlib=picolibc")
    set(_cxx_flags "--target=${triple} ${_g0} -m${core} ${_pic} ${_extra} --cstdlib=picolibc ${extra_cxx_flags}")
    set(_asm_flags "--target=${triple} ${_g0} -m${core} ${_pic} ${_extra} --cstdlib=picolibc")

    ExternalProject_Add(${_target}
        SOURCE_DIR       "${SOURCECODE}/runtimes"
        BINARY_DIR       "${_build_dir}"
        INSTALL_DIR      "${_install_dir}"
        CMAKE_GENERATOR  "Ninja"
        # Use | as the list separator so semicolon-separated cmake lists
        # (LLVM_ENABLE_RUNTIMES) can be passed as -D<VAR>=... without cmake
        # splitting them into multiple arguments.
        LIST_SEPARATOR "|"
        CMAKE_ARGS
            -DCMAKE_C_COMPILER=${CLANG}
            -DCMAKE_CXX_COMPILER=${CLANGXX}
            -DCMAKE_C_COMPILER_TARGET=${triple}
            -DCMAKE_CXX_COMPILER_TARGET=${triple}
            -DCMAKE_ASM_COMPILER_TARGET=${triple}
            -DCMAKE_C_FLAGS=${_c_flags}
            -DCMAKE_CXX_FLAGS=${_cxx_flags}
            -DCMAKE_ASM_FLAGS=${_asm_flags}
            -DLIBCXX_EXTRA_SITE_DEFINES=${extra_site_defines}
            -DCMAKE_BUILD_TYPE=Release
            -DCMAKE_INSTALL_PREFIX=${_install_dir}
            -DCMAKE_CROSSCOMPILING=ON
            -DCMAKE_TRY_COMPILE_TARGET_TYPE=STATIC_LIBRARY
            -DLLVM_ENABLE_RUNTIMES=libunwind|libcxxabi|libcxx
            -DLIBUNWIND_ENABLE_SHARED=OFF
            -DLIBUNWIND_ENABLE_THREADS=${enable_threads}
            -DLIBUNWIND_USE_COMPILER_RT=ON
            -DLIBUNWIND_IS_BAREMETAL=ON
            -DLIBCXXABI_ENABLE_SHARED=OFF
            -DLIBCXXABI_ENABLE_THREADS=${enable_threads}
            -DLIBCXXABI_BAREMETAL=ON
            -DLIBCXXABI_USE_COMPILER_RT=ON
            -DLIBCXXABI_USE_LLVM_UNWINDER=ON
            -DLIBCXX_CXX_ABI=libcxxabi
            -DLIBCXX_ENABLE_SHARED=OFF
            -DLIBCXX_ENABLE_THREADS=${enable_threads}
            -DLIBCXX_HAS_PTHREAD_API=${enable_threads}
            -DLIBCXX_ENABLE_TIME_ZONE_DATABASE=OFF
            -DLIBCXX_ENABLE_EXCEPTIONS=ON
            -DLIBCXX_ENABLE_FILESYSTEM=${enable_threads}
            -DLIBCXX_ENABLE_MONOTONIC_CLOCK=${monotonic_clock}
            -DLIBCXX_ENABLE_RANDOM_DEVICE=OFF
            -DLIBCXX_ENABLE_RTTI=ON
            -DLIBCXX_ENABLE_WIDE_CHARACTERS=${wide_chars}
            -DLIBCXX_ENABLE_LOCALIZATION=${localization}
            -DLIBCXX_USE_COMPILER_RT=ON
            -DRUNTIMES_USE_LIBC=picolibc
        BUILD_COMMAND
            ${CMAKE_COMMAND} --build <BINARY_DIR> -j${JOBS}
        INSTALL_COMMAND
            ${CMAKE_COMMAND} --install <BINARY_DIR>
        COMMAND
            ${CMAKE_COMMAND}
                -DMODE=runtimes-copy
                -DSRC_INSTALL_DIR=${_install_dir}
                -DDEST_LIB_DIR=${dest_base}/lib/${core}${_suffix}
                -DDEST_INC_DIR=${dest_base}/include
                -P ${CMAKE_CURRENT_LIST_DIR}/HexagonInstallSymlinks.cmake
        BUILD_ALWAYS FALSE
        # Runtimes need builtins + picolibc + H2 (H2 provides pthread.h in
        # hexagon-unknown-h2-elf/include/ which the threaded libcxx build
        # requires).  Depend on the umbrella targets so all (core, variant)
        # combinations of the deps are complete first.
        DEPENDS builtins picolibc h2
    )

    set(${out_target_var} ${_target} PARENT_SCOPE)
endfunction()

# ---------------------------------------------------------------------------
# Configuration 1: hexagon-unknown-none-elf (baremetal)
# ---------------------------------------------------------------------------
set(_runtimes_targets "")

foreach(core IN LISTS BUILD_CORES_LIST)
    foreach(variant IN LISTS VARIANTS)
        _hexagon_runtimes_add(
            "hexagon-unknown-none-elf"  "none-elf"
            ${core} ${variant}
            "OFF"                       # enable_threads
            "OFF"                       # monotonic_clock
            "OFF"                       # wide_chars
            "OFF"                       # localization
            "-nostdlib++ -nostdinc++"   # extra_cxx_flags
            ""                          # extra_site_defines
            "${SYSROOT_NONE_ELF}"       # dest_base
            _t
        )
        list(APPEND _runtimes_targets ${_t})
    endforeach()
endforeach()

# ---------------------------------------------------------------------------
# Configuration 2: hexagon-unknown-h2-elf (threads + localization + wide chars)
#
# H2_SITE_DEFINES describe the picolibc platform's capabilities and are baked
# into the installed __config_site header so all downstream consumers see them.
# ---------------------------------------------------------------------------
# Convert the semicolon-separated cmake list of site defines into a
# pipe-separated string (paired with LIST_SEPARATOR "|" on ExternalProject_Add)
# so cmake doesn't split it into multiple -D arguments to the sub-build.
set(_h2_site_defines "_GNU_SOURCE=|_PICOLIBC_CTYPE_SMALL=0|_POSIX_TIMERS=1|_POSIX_THREADS")

foreach(core IN LISTS BUILD_CORES_LIST)
    foreach(variant IN LISTS VARIANTS)
        _hexagon_runtimes_add(
            "hexagon-unknown-h2-elf"    "h2-elf"
            ${core} ${variant}
            "ON"                        # enable_threads
            "ON"                        # monotonic_clock
            "ON"                        # wide_chars
            "ON"                        # localization
            "-nostdlib++ -nostdinc++"   # extra_cxx_flags
            "${_h2_site_defines}"       # extra_site_defines
            "${SYSROOT_H2_ELF}"         # dest_base
            _t
        )
        list(APPEND _runtimes_targets ${_t})
    endforeach()
endforeach()

# ---------------------------------------------------------------------------
# Fan-out symlinks for non-built cores under both triples
# ---------------------------------------------------------------------------
add_custom_target(runtimes-symlinks
    COMMAND ${CMAKE_COMMAND}
        -DMODE=runtimes-fanout
        -DSYSROOT_NONE_ELF=${SYSROOT_NONE_ELF}
        -DSYSROOT_H2_ELF=${SYSROOT_H2_ELF}
        -DSOURCE_CORE=${SOURCE_CORE}
        "-DBUILD_CORES_LIST=${BUILD_CORES_LIST}"
        "-DALL_CORES_LIST=${ALL_CORES_LIST}"
        -P ${CMAKE_CURRENT_LIST_DIR}/HexagonInstallSymlinks.cmake
    DEPENDS ${_runtimes_targets}
    COMMENT "Fanning out runtimes to non-built cores"
    VERBATIM
)

add_custom_target(runtimes ALL DEPENDS runtimes-symlinks)
