include(ELDColorMsg)

function(add_version_info_eld_git ELD_REPOSITORY ELD_GIT_REV ELD_REPO)
  set(ELD_REV "")
  if(ELD_APPEND_VC_REV)
    execute_process(COMMAND git rev-parse HEAD
      WORKING_DIRECTORY ${ELD_REPO}
      OUTPUT_VARIABLE ELD_REV
      OUTPUT_STRIP_TRAILING_WHITESPACE)
  endif()
  set(${ELD_GIT_REV} ${ELD_REV} PARENT_SCOPE)
  info("Linker repository revision: ${ELD_REV}")
endfunction(add_version_info_eld_git)
