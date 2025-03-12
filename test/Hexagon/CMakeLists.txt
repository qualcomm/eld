# Build plugins only for Hexagon
if("${ELD_TARGETS_TO_BUILD}" MATCHES "Hexagon")
  add_subdirectory(Plugin)
endif()
