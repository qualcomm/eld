# HexagonInstallSymlinks.cmake
#
# Multi-mode -P script.  Dispatches to a helper function based on the MODE
# variable passed via -DMODE=<mode>.
#
# Modes:
#   builtins-fanout      builtins: fan out SOURCE_CORE variant dirs to non-built
#                        cores as per-file symlinks; symlink h2-elf into
#                        none-elf; copy (deref) none-elf into qurt-elf.
#   picolibc-copy-libs   picolibc: copy per-(core, variant) install lib dir
#                        into ${SYSROOT_NONE_ELF}/lib/${core}${suffix}/.
#   picolibc-fanout      picolibc: install headers, fan out to non-built cores,
#                        symlink h2-elf into none-elf, copy (deref) qurt-elf.
#   h2-fanout            h2: fan out h2-install-<base> -> h2-install-<version>,
#                        install bin/lib/include into hexagon-unknown-h2-elf/,
#                        write h2-picolibc.cfg into ${FINAL_INSTALL}/bin/.
#   runtimes-copy        runtimes: copy per-(triple, core, variant) install
#                        into ${dest}/lib/${core}${suffix}/ and merge headers
#                        into ${dest}/include/.
#   runtimes-fanout      runtimes: symlink non-built cores to SOURCE_CORE
#                        under both none-elf and h2-elf.

cmake_minimum_required(VERSION 3.20)

# ---------------------------------------------------------------------------
# Helper: create per-entry symlinks from src_dir/* to dest_dir/, with the
# symlink target being ${rel_prefix}/<name>.  Overwrites existing entries.
#
# By default, subdirectories are skipped (flat, per-file fanout as used for
# lib/<core>/ mirrors).  Pass `INCLUDE_DIRS TRUE` to also create directory
# symlinks (needed for header trees that have sys/, bits/, machine/, etc.
# subdirectories).
# ---------------------------------------------------------------------------
function(_symlink_dir_files src_dir dest_dir rel_prefix)
    cmake_parse_arguments(PARSE_ARGV 3 SL "" "INCLUDE_DIRS" "")
    if(NOT IS_DIRECTORY "${src_dir}")
        return()
    endif()
    file(MAKE_DIRECTORY "${dest_dir}")
    file(GLOB _files LIST_DIRECTORIES TRUE "${src_dir}/*")
    foreach(_src IN LISTS _files)
        # Skip real subdirectories unless caller opts in via INCLUDE_DIRS.
        if(IS_DIRECTORY "${_src}" AND NOT IS_SYMLINK "${_src}")
            if(NOT SL_INCLUDE_DIRS)
                continue()
            endif()
        endif()
        get_filename_component(_name "${_src}" NAME)
        set(_dest "${dest_dir}/${_name}")
        file(REMOVE_RECURSE "${_dest}")
        file(CREATE_LINK "${rel_prefix}/${_name}" "${_dest}" SYMBOLIC)
    endforeach()
endfunction()

# ---------------------------------------------------------------------------
# MODE: builtins-fanout
# ---------------------------------------------------------------------------
function(_mode_builtins_fanout)
    message(STATUS "[builtins-fanout] SOURCE_CORE=${SOURCE_CORE}")

    set(_lib_base "${SYSROOT_NONE_ELF}/lib")

    # 1) Non-built cores in ALL_CORES_LIST get per-file symlinks into SOURCE_CORE
    foreach(_core IN LISTS ALL_CORES_LIST)
        if(_core IN_LIST BUILD_CORES_LIST)
            continue()
        endif()
        foreach(_suffix "" "-G0" "-G0-pic")
            _symlink_dir_files(
                "${_lib_base}/${SOURCE_CORE}${_suffix}"
                "${_lib_base}/${_core}${_suffix}"
                "../${SOURCE_CORE}${_suffix}"
            )
        endforeach()
    endforeach()

    # 2) h2-elf/lib/<core>[-suffix]/<file> -> ../../../hexagon-unknown-none-elf/lib/<core>[-suffix]/<file>
    set(_h2_lib_base "${SYSROOT_H2_ELF}/lib")
    file(GLOB _core_dirs LIST_DIRECTORIES TRUE "${_lib_base}/*")
    foreach(_core_dir IN LISTS _core_dirs)
        if(NOT IS_DIRECTORY "${_core_dir}")
            continue()
        endif()
        get_filename_component(_core_name "${_core_dir}" NAME)
        _symlink_dir_files(
            "${_lib_base}/${_core_name}"
            "${_h2_lib_base}/${_core_name}"
            "../../../hexagon-unknown-none-elf/lib/${_core_name}"
        )
    endforeach()

    # 3) qurt-elf = real copy of none-elf (dereferences symlinks)
    file(MAKE_DIRECTORY "${SYSROOT_QURT_ELF}")
    if(IS_DIRECTORY "${SYSROOT_QURT_ELF}/lib")
        file(REMOVE_RECURSE "${SYSROOT_QURT_ELF}/lib")
    endif()
    file(COPY "${_lib_base}/"
         DESTINATION "${SYSROOT_QURT_ELF}/lib"
         FOLLOW_SYMLINK_CHAIN)

    message(STATUS "[builtins-fanout] done")
endfunction()

# ---------------------------------------------------------------------------
# MODE: picolibc-copy-libs
#
# Copies ${SRC_LIB_DIR}/. into ${DEST_LIB_DIR}/ (matches `cp -a src/. dest/`).
# ---------------------------------------------------------------------------
function(_mode_picolibc_copy_libs)
    if(NOT IS_DIRECTORY "${SRC_LIB_DIR}")
        message(WARNING "[picolibc-copy-libs] SRC_LIB_DIR '${SRC_LIB_DIR}' does not exist")
        return()
    endif()
    file(MAKE_DIRECTORY "${DEST_LIB_DIR}")
    file(GLOB _entries LIST_DIRECTORIES TRUE "${SRC_LIB_DIR}/*")
    foreach(_entry IN LISTS _entries)
        file(COPY "${_entry}" DESTINATION "${DEST_LIB_DIR}")
    endforeach()
    message(STATUS "[picolibc-copy-libs] ${SRC_LIB_DIR} -> ${DEST_LIB_DIR}")
endfunction()

# ---------------------------------------------------------------------------
# MODE: picolibc-fanout
# ---------------------------------------------------------------------------
function(_mode_picolibc_fanout)
    message(STATUS "[picolibc-fanout] SOURCE_CORE=${SOURCE_CORE}")

    # 1) Install headers into ${SYSROOT_NONE_ELF}/include/
    if(NOT IS_DIRECTORY "${PICOLIBC_HEADERS_SRC}")
        # Fallback: any include dir under picolibc-install
        file(GLOB _fallback "${PICOLIBC_HEADERS_SRC}/../*/include")
        list(FILTER _fallback INCLUDE REGEX "/include$")
        list(LENGTH _fallback _nf)
        if(_nf GREATER 0)
            list(GET _fallback 0 PICOLIBC_HEADERS_SRC)
        else()
            message(FATAL_ERROR "[picolibc-fanout] no picolibc headers found")
        endif()
    endif()
    file(MAKE_DIRECTORY "${SYSROOT_NONE_ELF}/include")
    file(GLOB _hdr_entries LIST_DIRECTORIES TRUE "${PICOLIBC_HEADERS_SRC}/*")
    foreach(_h IN LISTS _hdr_entries)
        file(COPY "${_h}" DESTINATION "${SYSROOT_NONE_ELF}/include")
    endforeach()

    set(_lib_base "${SYSROOT_NONE_ELF}/lib")

    # 2) Non-built cores get per-file symlinks into SOURCE_CORE
    foreach(_core IN LISTS ALL_CORES_LIST)
        if(_core IN_LIST BUILD_CORES_LIST)
            continue()
        endif()
        foreach(_suffix "" "-G0" "-G0-pic")
            _symlink_dir_files(
                "${_lib_base}/${SOURCE_CORE}${_suffix}"
                "${_lib_base}/${_core}${_suffix}"
                "../${SOURCE_CORE}${_suffix}"
            )
        endforeach()
    endforeach()

    # 3) h2-elf: rebuild from scratch as per-file symlinks into none-elf.
    if(IS_DIRECTORY "${SYSROOT_H2_ELF}")
        file(REMOVE_RECURSE "${SYSROOT_H2_ELF}")
    endif()
    file(MAKE_DIRECTORY "${SYSROOT_H2_ELF}/include" "${SYSROOT_H2_ELF}/lib")

    # 3a) headers: h2-elf/include/<entry> -> ../../hexagon-unknown-none-elf/include/<entry>
    #     INCLUDE_DIRS TRUE — the picolibc header tree has sys/, bits/,
    #     machine/, arpa/, rpc/, ssp/ subdirectories that must be
    #     symlinked (as directory symlinks) so `#include <sys/cdefs.h>`
    #     resolves through the h2-elf sysroot.
    _symlink_dir_files(
        "${SYSROOT_NONE_ELF}/include"
        "${SYSROOT_H2_ELF}/include"
        "../../hexagon-unknown-none-elf/include"
        INCLUDE_DIRS TRUE
    )

    # 3b) libs: h2-elf/lib/<core>[-suffix]/<file> -> ../../../hexagon-unknown-none-elf/lib/<core>[-suffix]/<file>
    file(GLOB _core_dirs LIST_DIRECTORIES TRUE "${_lib_base}/*")
    foreach(_core_dir IN LISTS _core_dirs)
        if(NOT IS_DIRECTORY "${_core_dir}")
            continue()
        endif()
        get_filename_component(_core_name "${_core_dir}" NAME)
        _symlink_dir_files(
            "${_lib_base}/${_core_name}"
            "${SYSROOT_H2_ELF}/lib/${_core_name}"
            "../../../hexagon-unknown-none-elf/lib/${_core_name}"
        )
    endforeach()

    # 4) qurt-elf = full copy of none-elf (preserves symlinks as-is)
    if(IS_DIRECTORY "${SYSROOT_QURT_ELF}")
        file(REMOVE_RECURSE "${SYSROOT_QURT_ELF}")
    endif()
    file(COPY "${SYSROOT_NONE_ELF}/" DESTINATION "${SYSROOT_QURT_ELF}")

    message(STATUS "[picolibc-fanout] done")
endfunction()

# ---------------------------------------------------------------------------
# MODE: h2-fanout
# ---------------------------------------------------------------------------
function(_mode_h2_fanout)
    message(STATUS "[h2-fanout] BUILD_DIR=${BUILD_DIR}")

    # 1) Copy h2-install-<base> to h2-install-<version> for each version.
    foreach(_pair
            "68;${H2_VERSIONS_FROM_68}"
            "73;${H2_VERSIONS_FROM_73}"
            "81;${H2_VERSIONS_FROM_81}")
        list(GET _pair 0 _base)
        list(REMOVE_AT _pair 0)
        set(_src "${BUILD_DIR}/h2-install-${_base}")
        foreach(_v IN LISTS _pair)
            set(_dst "${BUILD_DIR}/h2-install-${_v}")
            if(IS_DIRECTORY "${_dst}")
                file(REMOVE_RECURSE "${_dst}")
            endif()
            file(COPY "${_src}/" DESTINATION "${_dst}")
        endforeach()
    endforeach()

    # 2) Install per-version bin -> h2-elf/bin/<v>/G0/, plus non-G0 symlinks.
    foreach(_v IN LISTS H2_ALL_VERSIONS)
        set(_bin_src "${BUILD_DIR}/h2-install-${_v}/bin")
        set(_bin_g0  "${SYSROOT_H2_ELF}/bin/${_v}/G0")
        set(_bin_top "${SYSROOT_H2_ELF}/bin/${_v}")
        if(IS_DIRECTORY "${_bin_src}")
            file(MAKE_DIRECTORY "${_bin_g0}")
            file(GLOB _binfiles LIST_DIRECTORIES TRUE "${_bin_src}/*")
            foreach(_bf IN LISTS _binfiles)
                file(COPY "${_bf}" DESTINATION "${_bin_g0}")
            endforeach()
            # non-G0 symlinks: bin/<v>/<file> -> G0/<file>
            file(GLOB _inst LIST_DIRECTORIES FALSE "${_bin_g0}/*")
            foreach(_f IN LISTS _inst)
                get_filename_component(_name "${_f}" NAME)
                set(_lnk "${_bin_top}/${_name}")
                file(REMOVE "${_lnk}")
                file(CREATE_LINK "G0/${_name}" "${_lnk}" SYMBOLIC)
            endforeach()
        endif()
    endforeach()

    # 3) Install per-version lib -> h2-elf/lib/<v>-G0/, plus non-G0 symlinks.
    foreach(_v IN LISTS H2_ALL_VERSIONS)
        set(_lib_src "${BUILD_DIR}/h2-install-${_v}/lib")
        set(_lib_g0  "${SYSROOT_H2_ELF}/lib/${_v}-G0")
        set(_lib_top "${SYSROOT_H2_ELF}/lib/${_v}")
        if(IS_DIRECTORY "${_lib_src}")
            file(MAKE_DIRECTORY "${_lib_g0}")
            file(GLOB _libfiles LIST_DIRECTORIES TRUE "${_lib_src}/*")
            foreach(_lf IN LISTS _libfiles)
                file(COPY "${_lf}" DESTINATION "${_lib_g0}")
            endforeach()
            # non-G0 symlinks: lib/<v>/<file> -> ../<v>-G0/<file>
            file(MAKE_DIRECTORY "${_lib_top}")
            file(GLOB _inst LIST_DIRECTORIES FALSE "${_lib_g0}/*")
            foreach(_f IN LISTS _inst)
                get_filename_component(_name "${_f}" NAME)
                set(_lnk "${_lib_top}/${_name}")
                file(REMOVE "${_lnk}")
                file(CREATE_LINK "../${_v}-G0/${_name}" "${_lnk}" SYMBOLIC)
            endforeach()
        endif()
    endforeach()

    # 4) Headers from v81 (canonical) -> h2-elf/include/
    set(_inc_src "${BUILD_DIR}/h2-install-v81/include")
    set(_inc_dst "${SYSROOT_H2_ELF}/include")
    if(IS_DIRECTORY "${_inc_src}")
        file(MAKE_DIRECTORY "${_inc_dst}")
        file(GLOB _hdrs LIST_DIRECTORIES TRUE "${_inc_src}/*")
        foreach(_h IN LISTS _hdrs)
            file(COPY "${_h}" DESTINATION "${_inc_dst}")
        endforeach()
    endif()

    # 5) Write h2-picolibc.cfg into ${FINAL_INSTALL}/bin/
    file(MAKE_DIRECTORY "${FINAL_INSTALL}/bin")
    file(WRITE "${FINAL_INSTALL}/bin/h2-picolibc.cfg"
"--target=hexagon-unknown-h2-elf \\
--cstdlib=picolibc \\
\$-Wl,--undefined=__retarget_lock_init \\
\$-Wl,-l:liblocks.a \\
\$-Wl,--section-start=.start=0x2000000 \\
\$-Wl,-T,<CFGDIR>/../templates/staticExecutable/static-executable-h2-picolibc.lcs.template
")

    message(STATUS "[h2-fanout] done")
endfunction()

# ---------------------------------------------------------------------------
# MODE: runtimes-copy
#
# Copies ${SRC_INSTALL_DIR}/lib/* into ${DEST_LIB_DIR}/ and
# ${SRC_INSTALL_DIR}/include/* into ${DEST_INC_DIR}/.
# ---------------------------------------------------------------------------
function(_mode_runtimes_copy)
    if(IS_DIRECTORY "${SRC_INSTALL_DIR}/lib")
        file(MAKE_DIRECTORY "${DEST_LIB_DIR}")
        file(GLOB _libs LIST_DIRECTORIES TRUE "${SRC_INSTALL_DIR}/lib/*")
        foreach(_l IN LISTS _libs)
            file(COPY "${_l}" DESTINATION "${DEST_LIB_DIR}")
        endforeach()
    endif()
    if(IS_DIRECTORY "${SRC_INSTALL_DIR}/include")
        file(MAKE_DIRECTORY "${DEST_INC_DIR}")
        file(GLOB _incs LIST_DIRECTORIES TRUE "${SRC_INSTALL_DIR}/include/*")
        foreach(_i IN LISTS _incs)
            file(COPY "${_i}" DESTINATION "${DEST_INC_DIR}")
        endforeach()
    endif()
    message(STATUS "[runtimes-copy] ${SRC_INSTALL_DIR} -> ${DEST_LIB_DIR}")
endfunction()

# ---------------------------------------------------------------------------
# MODE: runtimes-fanout
# ---------------------------------------------------------------------------
function(_mode_runtimes_fanout)
    foreach(_sysroot "${SYSROOT_NONE_ELF}" "${SYSROOT_H2_ELF}")
        set(_lib_base "${_sysroot}/lib")
        if(NOT IS_DIRECTORY "${_lib_base}")
            continue()
        endif()
        foreach(_core IN LISTS ALL_CORES_LIST)
            if(_core IN_LIST BUILD_CORES_LIST)
                continue()
            endif()
            foreach(_suffix "" "-G0" "-G0-pic")
                _symlink_dir_files(
                    "${_lib_base}/${SOURCE_CORE}${_suffix}"
                    "${_lib_base}/${_core}${_suffix}"
                    "../${SOURCE_CORE}${_suffix}"
                )
            endforeach()
        endforeach()
    endforeach()
    message(STATUS "[runtimes-fanout] done")
endfunction()

# ---------------------------------------------------------------------------
# Dispatch
# ---------------------------------------------------------------------------
if(NOT DEFINED MODE)
    message(FATAL_ERROR "MODE variable is required (-DMODE=<mode>)")
endif()

if(MODE STREQUAL "builtins-fanout")
    _mode_builtins_fanout()
elseif(MODE STREQUAL "picolibc-copy-libs")
    _mode_picolibc_copy_libs()
elseif(MODE STREQUAL "picolibc-fanout")
    _mode_picolibc_fanout()
elseif(MODE STREQUAL "h2-fanout")
    _mode_h2_fanout()
elseif(MODE STREQUAL "runtimes-copy")
    _mode_runtimes_copy()
elseif(MODE STREQUAL "runtimes-fanout")
    _mode_runtimes_fanout()
else()
    message(FATAL_ERROR "Unknown MODE: ${MODE}")
endif()
