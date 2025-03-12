find_package(Git REQUIRED)

function(set_up_git_revision_monitor)
  add_custom_target(update-eld-verinfo
    ALL
    COMMENT "Checking for any changes to Embedded Linker version information."
    DEPENDS ${ELD_VERSION_PRE_CONFIGURED_FILE}
    BYPRODUCTS
    ${ELD_VERSION_POST_CONFIGURED_FILE}
    ${ELD_VERSION_STATE_FILE}
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    COMMAND
    ${CMAKE_COMMAND}
    -D_UPDATE_VERSION_INFO=ON
    -DLLVM_MAIN_SRC_DIR=${CMAKE_SOURCE_DIR}
    -DELD_VERSION_PRE_CONFIGURED_FILE=${ELD_VERSION_PRE_CONFIGURED_FILE}
    -DELD_VERSION_POST_CONFIGURED_FILE=${ELD_VERSION_POST_CONFIGURED_FILE}
    -DELD_BINARY_DIR=${ELD_BINARY_DIR}
    -DELD_SOURCE_DIR=${ELD_SOURCE_DIR}
    -P ${CMAKE_CURRENT_LIST_FILE})
endfunction()

# Computes git revision for the working_dir. Returns the git revision in
# the parent scope variable specified by 'revision' parameter.
function(compute_git_revision working_dir revision)
  execute_process(COMMAND ${GIT_EXECUTABLE} rev-parse HEAD
    WORKING_DIRECTORY ${working_dir}
    OUTPUT_VARIABLE git_revision
    OUTPUT_STRIP_TRAILING_WHITESPACE)
  set(${revision} ${git_revision} PARENT_SCOPE)
endfunction()

# Checks if git revision of mclinker project has changed.
# eld_revision - Returns current GIT revision.
# has_changes - Returns true if eld revision has changed.
function(check_eld_revision eld_revision has_changes)
  compute_git_revision(${ELD_SOURCE_DIR} revision)
  set(${eld_revision} ${revision} PARENT_SCOPE)

  file(READ ${ELD_VERSION_PRE_CONFIGURED_FILE} preconfig_hash)
  string(SHA256 new_state "${preconfig_hash}${revision}")

  if(EXISTS "${ELD_VERSION_STATE_FILE}")
    file(READ "${ELD_VERSION_STATE_FILE}" old_state)
    if(old_state STREQUAL new_state)
      set(${has_changes} OFF PARENT_SCOPE)
      message(STATUS "No Embedded linker version information changes found.")
      return()
    endif()
  endif()

  message(STATUS "Reconfiguring version file to update Embedded linker revision.")
  # Update the file hash to the new state.
  file(WRITE "${ELD_VERSION_STATE_FILE}" "${new_state}")
  set(${has_changes} ON PARENT_SCOPE)
endfunction()

if(_UPDATE_VERSION_INFO)
  set(ELD_VERSION_STATE_FILE "${ELD_BINARY_DIR}/.eld-revision")
  check_eld_revision(ELD_REVISION has_changes)
  if(has_changes OR NOT EXISTS ${ELD_VERSION_POST_CONFIGURED_FILE})
    configure_file("${ELD_VERSION_PRE_CONFIGURED_FILE}"
      "${ELD_VERSION_POST_CONFIGURED_FILE}")
  endif()
else()
  set_up_git_revision_monitor()
endif()
