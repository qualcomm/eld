# CMAKE_BINARY_DIR is context dependent
if(EXISTS ${CMAKE_BINARY_DIR}/eld_install.cmake)
  include(${CMAKE_BINARY_DIR}/eld_install.cmake)
else()
  include(${CMAKE_BINARY_DIR}/tools/eld/eld_install.cmake)
endif()

if(UNIX)
  set(LD_ELD_LINK_OR_COPY create_symlink)
  set(LD_ELD_DESTDIR $ENV{DESTDIR})
else()
  set(LD_ELD_LINK_OR_COPY copy)
endif()

# CMAKE_EXECUTABLE_SUFFIX is undefined on cmake scripts. See PR9286.
if(WIN32)
  set(EXECUTABLE_SUFFIX ".exe")
else()
  set(EXECUTABLE_SUFFIX "")
endif()

set(bindir "${CMAKE_INSTALL_PREFIX}/bin/")
set(ld_eld "ld.eld${EXECUTABLE_SUFFIX}")

if("${LINK_TARGETS}" MATCHES "Hexagon" AND "${TARGET_TRIPLE}" MATCHES
                                           "hexagon-unknown-linux")
  set(is_hexagon_linux 1)
endif()

foreach(target_name ${LINK_TARGETS})
  string(TOLOWER ${target_name} ld_eld_name)
  if(${is_hexagon_linux})
    set(ld_eld_symlink "${ld_eld_name}-linux-link${EXECUTABLE_SUFFIX}")
  else(NOT ${is_hexagon_linux})
    set(ld_eld_symlink "${ld_eld_name}-link${EXECUTABLE_SUFFIX}")
  endif(${is_hexagon_linux})

  message("Creating ${ld_eld_symlink} symlink based on ${ld_eld}")
  execute_process(
    COMMAND "${CMAKE_COMMAND}" -E ${LD_ELD_LINK_OR_COPY} "${ld_eld}"
            "${ld_eld_symlink}" WORKING_DIRECTORY "${bindir}")
endforeach(target_name)

if(NOT "${USE_LINKER_ALT_NAME}" STREQUAL "")
  message("Creating ${USE_LINKER_ALT_NAME} symlink based on ${ld_eld}")
  execute_process(
    COMMAND "${CMAKE_COMMAND}" -E ${LD_ELD_LINK_OR_COPY} "${ld_eld}"
            "${USE_LINKER_ALT_NAME}" WORKING_DIRECTORY "${bindir}")
endif()
