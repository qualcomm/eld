add_subdirectory(Driver)

configure_file(${CMAKE_CURRENT_SOURCE_DIR}/PluginAPI/PluginBase.h.inc
               ${CMAKE_CURRENT_BINARY_DIR}/PluginAPI/PluginBase.h)

if(ENABLE_ELD_PLUGIN_SUPPORT)
  # Linker Plugin include files.
  install(
    DIRECTORY PluginAPI/
    DESTINATION include/ELD/PluginAPI/
    COMPONENT PluginAPIHeaders
    FILES_MATCHING
    PATTERN "*.h"
    PATTERN "PluginConfig.h" EXCLUDE
    PATTERN ".git" EXCLUDE
    PATTERN ".svn" EXCLUDE)

  if(ENABLE_ELD_PLUGIN_SUPPORT)
    install(
      FILES ${CMAKE_CURRENT_BINARY_DIR}/PluginAPI/PluginBase.h
      DESTINATION include/ELD/PluginAPI/
      COMPONENT PluginAPIHeaders)
  endif()
endif()
