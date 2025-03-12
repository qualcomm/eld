set(LLVM_LINK_COMPONENTS ${LLVM_TARGETS_TO_BUILD} irreader)

add_llvm_executable(ld.eld eld.cpp)
add_dependencies(ld.eld update-eld-verinfo)

set(LINKER_WRAPPER_LIB "LW")
target_link_libraries(ld.eld PRIVATE ${LINKER_WRAPPER_LIB})

if(UNIX)
  set(LD_ELD_LINK_OR_COPY create_symlink)
  set(ld_eld_binary "ld.eld${CMAKE_EXECUTABLE_SUFFIX}")
else()
  set(LD_ELD_LINK_OR_COPY copy)
  set(ld_eld_binary
      "${ELD_RUNTIME_OUTPUT_DIR}/ld.eld${CMAKE_EXECUTABLE_SUFFIX}")
endif()

# This is to support "install/local" from eld build directory
configure_file(eld_install.cmake.in
               ${CMAKE_CURRENT_BINARY_DIR}/eld_install.cmake)

# We can either reconfigure or copy the same file. This is to support "install"
# from top level llvm build directory
configure_file(eld_install.cmake.in ${CMAKE_BINARY_DIR}/eld_install.cmake)

install(TARGETS ld.eld RUNTIME DESTINATION bin)

# FIXME: We need to switch to ELD_TARGETS_TO_BUILD after the buildbot upgrade
if("${LLVM_TARGETS_TO_BUILD}" MATCHES "Hexagon" AND "${TARGET_TRIPLE}" MATCHES
                                                    "hexagon-unknown-linux")
  set(is_hexagon_linux 1)
endif()
foreach(target_name ${LLVM_TARGETS_TO_BUILD})
  string(TOLOWER ${target_name} ld_eld_name)
  if(${is_hexagon_linux})
    set(ld_eld_name "${ld_eld_name}-linux-link${CMAKE_EXECUTABLE_SUFFIX}")
  else(NOT ${is_hexagon_linux})
    set(ld_eld_name "${ld_eld_name}-link${CMAKE_EXECUTABLE_SUFFIX}")
  endif(${is_hexagon_linux})
  if(${target_name} MATCHES "X86")
    set(ld_eld_name "x86_64-link${CMAKE_EXECUTABLE_SUFFIX}")
  endif()
  add_custom_command(
    TARGET ld.eld
    POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E ${LD_ELD_LINK_OR_COPY} "${ld_eld_binary}"
            "${ld_eld_name}"
    WORKING_DIRECTORY "${ELD_RUNTIME_OUTPUT_DIR}")

  add_custom_target(
    ${ld_eld_name}
    DEPENDS ${ld_eld_binary}
    COMMAND ${CMAKE_COMMAND} -E ${LD_ELD_LINK_OR_COPY} "${ld_eld_binary}"
            "${ld_eld_name}"
    WORKING_DIRECTORY "${ELD_RUNTIME_OUTPUT_DIR}")
endforeach(target_name)
if(DEFINED USE_LINKER_ALT_NAME)
if(NOT "${USE_LINKER_ALT_NAME}" STREQUAL "")
  add_custom_command(
    TARGET ld.eld
    POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E ${LD_ELD_LINK_OR_COPY} "${ld_eld_binary}" "${USE_LINKER_ALT_NAME}"
    WORKING_DIRECTORY "${ELD_RUNTIME_OUTPUT_DIR}")
  add_custom_target(
    ${USE_LINKER_ALT_NAME}
    DEPENDS ${ld_eld_binary}
    COMMAND ${CMAKE_COMMAND} -E ${LD_ELD_LINK_OR_COPY} "${ld_eld_binary}"
            "${USE_LINKER_ALT_NAME}"
    WORKING_DIRECTORY "${ELD_RUNTIME_OUTPUT_DIR}")
endif()
endif()

# Create the symlink at installation time.
install(SCRIPT ${CMAKE_CURRENT_SOURCE_DIR}/ld_eld_symlink.cmake)
