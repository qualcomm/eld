cmake_minimum_required(VERSION 3.20)

execute_process(COMMAND "${READELF}" -d "${PLUGIN_LIBRARY}"
  RESULT_VARIABLE result OUTPUT_VARIABLE output ERROR_VARIABLE error)
if(NOT result EQUAL 0)
  message(FATAL_ERROR "Cannot inspect ${PLUGIN_LIBRARY}: ${error}")
endif()
if(output MATCHES "[(](RPATH|RUNPATH)[)]")
  message(FATAL_ERROR
    "Standalone plugins must rely on environment runtime paths: ${output}")
endif()
message(STATUS "No RPATH/RUNPATH in ${PLUGIN_LIBRARY}")
