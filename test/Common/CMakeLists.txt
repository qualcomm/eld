add_custom_target(CommonPluginUnitTests)
set_target_properties(CommonPluginUnitTests PROPERTIES FOLDER
                                                       "plugin unit tests")

function(add_common_plugin pluginname)
  add_dependencies(CommonPluginUnitTests ${pluginname})
endfunction()

add_subdirectory(standalone)
add_subdirectory(Plugin)
