# HexagonSysrootBuiltins.cmake
#
# Builds compiler-rt builtins for hexagon-unknown-none-elf, per (core, variant).
# Mirrors sysroot-scripts/build_hexagon_builtins.sh.
#
# Targets:
#   builtins-<core>-<variant>          e.g. builtins-v68-G0
#   builtins-symlinks                  fan-out to non-built cores + h2/qurt
#   builtins                           umbrella (depends on all of the above)

# ---------------------------------------------------------------------------
# Per-variant flag table
#
#   variant    G0_FLAG   PIC_FLAG   PIC_ENABLED   SUFFIX   EXTRA_FLAGS
#   non-G0     (empty)   (empty)    OFF           (empty)  -O3 -ffunction-sections -fdata-sections
#   G0         -G0       (empty)    OFF           -G0      -O3 -ffunction-sections -fdata-sections
#   G0-pic     -G0       -fPIC      ON            -G0-pic  -O3 -ffunction-sections -fdata-sections -fvisibility=hidden
# ---------------------------------------------------------------------------
function(_hexagon_builtins_variant_flags variant out_g0 out_pic out_pic_enabled out_suffix out_extra)
    if(variant STREQUAL "non-G0")
        set(${out_g0}          ""    PARENT_SCOPE)
        set(${out_pic}         ""    PARENT_SCOPE)
        set(${out_pic_enabled} "OFF" PARENT_SCOPE)
        set(${out_suffix}      ""    PARENT_SCOPE)
        set(${out_extra}       "-O3 -ffunction-sections -fdata-sections" PARENT_SCOPE)
    elseif(variant STREQUAL "G0")
        set(${out_g0}          "-G0" PARENT_SCOPE)
        set(${out_pic}         ""    PARENT_SCOPE)
        set(${out_pic_enabled} "OFF" PARENT_SCOPE)
        set(${out_suffix}      "-G0" PARENT_SCOPE)
        set(${out_extra}       "-O3 -ffunction-sections -fdata-sections" PARENT_SCOPE)
    elseif(variant STREQUAL "G0-pic")
        set(${out_g0}          "-G0"    PARENT_SCOPE)
        set(${out_pic}         "-fPIC"  PARENT_SCOPE)
        set(${out_pic_enabled} "ON"     PARENT_SCOPE)
        set(${out_suffix}      "-G0-pic" PARENT_SCOPE)
        set(${out_extra}
            "-O3 -ffunction-sections -fdata-sections -fvisibility=hidden"
            PARENT_SCOPE)
    else()
        message(FATAL_ERROR "Unknown variant: ${variant}")
    endif()
endfunction()

# ---------------------------------------------------------------------------
# Per-(core, variant) ExternalProject
# ---------------------------------------------------------------------------
set(_builtins_targets "")

foreach(core IN LISTS BUILD_CORES_LIST)
    foreach(variant IN LISTS VARIANTS)
        _hexagon_builtins_variant_flags(${variant}
            _g0 _pic _pic_enabled _suffix _extra)

        set(_target        "builtins-${core}-${variant}")
        set(_build_dir     "${CMAKE_BINARY_DIR}/builtins/${core}${_suffix}")
        set(_install_dir   "${CMAKE_BINARY_DIR}/builtins-install/${core}${_suffix}")
        set(_dest_lib_dir  "${SYSROOT_NONE_ELF}/lib/${core}${_suffix}")

        # With LLVM_ENABLE_PER_TARGET_RUNTIME_DIR=ON compiler-rt installs to:
        #   <prefix>/lib/hexagon-unknown-none-elf/libclang_rt.builtins.a
        set(_builtins_lib
            "${_install_dir}/lib/hexagon-unknown-none-elf/libclang_rt.builtins.a")

        # Bake --target=hexagon-unknown-none-elf into every flag string.  The
        # nightly clang is multi-arch, so without an explicit --target the
        # driver (especially the ASM driver, which CMAKE_C/CXX_COMPILER_TARGET
        # does not reach) defaults to the host triple and fails on Hexagon
        # assembler directives (.falign, memd(...)).
        set(_c_flags   "--target=hexagon-unknown-none-elf ${_g0} -ffreestanding -m${core} ${_pic} ${_extra} --cstdlib=picolibc")
        set(_cxx_flags "--target=hexagon-unknown-none-elf ${_g0} -ffreestanding -m${core} ${_pic} ${_extra} --cstdlib=picolibc")
        set(_asm_flags "--target=hexagon-unknown-none-elf ${_g0} -mlong-calls -m${core} ${_pic} ${_extra} --cstdlib=picolibc")

        ExternalProject_Add(${_target}
            SOURCE_DIR       "${SOURCECODE}/compiler-rt"
            BINARY_DIR       "${_build_dir}"
            INSTALL_DIR      "${_install_dir}"
            CMAKE_GENERATOR  "Ninja"
            CMAKE_ARGS
                -DCMAKE_C_COMPILER=${CLANG}
                -DCMAKE_CXX_COMPILER=${CLANGXX}
                -DCMAKE_ASM_FLAGS=${_asm_flags}
                -DCMAKE_C_FLAGS=${_c_flags}
                -DCMAKE_CXX_FLAGS=${_cxx_flags}
                -DCMAKE_BUILD_TYPE=Release
                -DCMAKE_INSTALL_PREFIX=${_install_dir}
                -DLLVM_ENABLE_PER_TARGET_RUNTIME_DIR:BOOL=ON
                -DLLVM_TARGET_TRIPLE=hexagon-unknown-none-elf
                -DCOMPILER_RT_DEFAULT_TARGET_TRIPLE=hexagon-unknown-none-elf
                -DCOMPILER_RT_BUILD_BUILTINS:BOOL=ON
                -DCOMPILER_RT_BUILD_SANITIZERS:BOOL=OFF
                -DCOMPILER_RT_BUILD_XRAY:BOOL=OFF
                -DCOMPILER_RT_BUILD_LIBFUZZER:BOOL=OFF
                -DCOMPILER_RT_BUILD_PROFILE:BOOL=OFF
                -DCOMPILER_RT_BUILD_MEMPROF:BOOL=OFF
                -DCOMPILER_RT_BUILD_ORC:BOOL=OFF
                -DCOMPILER_RT_BUILD_GWP_ASAN:BOOL=OFF
                -DCOMPILER_RT_BUILTINS_ENABLE_PIC:BOOL=${_pic_enabled}
                -DCOMPILER_RT_SUPPORTED_ARCH=hexagon
                -DCOMPILER_RT_BAREMETAL_BUILD:BOOL=ON
                -DCMAKE_CROSSCOMPILING:BOOL=ON
                -DCAN_TARGET_hexagon=1
                -DCMAKE_C_COMPILER_FORCED:BOOL=ON
                -DCMAKE_CXX_COMPILER_FORCED:BOOL=ON
                -DCMAKE_C_COMPILER_TARGET=hexagon-unknown-none-elf
                -DCMAKE_CXX_COMPILER_TARGET=hexagon-unknown-none-elf
            BUILD_COMMAND
                ${CMAKE_COMMAND} --build <BINARY_DIR> -j${JOBS} --target install-builtins
            # install-builtins is invoked via BUILD_COMMAND above; suppress the
            # default install step so ExternalProject doesn't run cmake --install.
            INSTALL_COMMAND
                ${CMAKE_COMMAND} -E make_directory ${_dest_lib_dir}
            COMMAND
                ${CMAKE_COMMAND} -E copy ${_builtins_lib} ${_dest_lib_dir}/libclang_rt.builtins.a
            BUILD_BYPRODUCTS ${_builtins_lib}
        )

        list(APPEND _builtins_targets ${_target})
    endforeach()
endforeach()

# ---------------------------------------------------------------------------
# Fan-out symlinks: non-built cores → SOURCE_CORE, then h2-elf → none-elf,
# then qurt-elf = deref copy of none-elf.
# ---------------------------------------------------------------------------
add_custom_target(builtins-symlinks
    COMMAND ${CMAKE_COMMAND}
        -DMODE=builtins-fanout
        -DSYSROOT_NONE_ELF=${SYSROOT_NONE_ELF}
        -DSYSROOT_H2_ELF=${SYSROOT_H2_ELF}
        -DSYSROOT_QURT_ELF=${SYSROOT_QURT_ELF}
        -DSOURCE_CORE=${SOURCE_CORE}
        "-DBUILD_CORES_LIST=${BUILD_CORES_LIST}"
        "-DALL_CORES_LIST=${ALL_CORES_LIST}"
        -P ${CMAKE_CURRENT_LIST_DIR}/HexagonInstallSymlinks.cmake
    DEPENDS ${_builtins_targets}
    COMMENT "Fanning out compiler-rt builtins to non-built cores + h2/qurt"
    VERBATIM
)

add_custom_target(builtins ALL DEPENDS builtins-symlinks)
