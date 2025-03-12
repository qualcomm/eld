include(ColorMsg)

function(add_version_info_eld_git ELD_REPOSITORY ELD_GIT_REV ELD_REPO)
  execute_process(COMMAND git rev-parse HEAD
    WORKING_DIRECTORY ${ELD_REPO}
    OUTPUT_VARIABLE ELD_REV
    OUTPUT_STRIP_TRAILING_WHITESPACE)
  set(${ELD_GIT_REV} ${ELD_REV} PARENT_SCOPE)
  info("Linker repository revision: ${ELD_REV}")
endfunction(add_version_info_eld_git)
