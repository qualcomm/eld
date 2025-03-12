# This module checks for cmake variables that must be defined.
include(ColorMsg)

# Emit message for missing dependency and error out.
function(missing_req_msg req_arg)
  error("${req_arg}")
  message(FATAL_ERROR "${req_arg}")
endfunction()

# Emit dependency information message.
function(req_info req_message)
  info("${req_message}")
endfunction()

if(NOT DEFINED LLVM_TARGETS_TO_BUILD)
  missing_req_msg("LLVM_TARGETS_TO_BUILD is not set")
endif()
if(NOT DEFINED LLVM_DEFAULT_TARGET_TRIPLE)
  missing_req_msg("LLVM_DEFAULT_TARGET_TRIPLE is not set")
endif()
