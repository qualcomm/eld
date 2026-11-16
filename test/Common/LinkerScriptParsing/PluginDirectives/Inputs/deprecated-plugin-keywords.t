PLUGIN_SECTION_MATCHER("LibName", "PluginName")
PLUGIN_ITER_SECTIONS("LibName", "PluginName")
PLUGIN_OUTPUT_SECTION_ITER("LibName", "PluginName")
LINKER_PLUGIN("LibName", "PluginName")

SECTIONS {
  .filesz PLUGIN_CONTROL_FILESZ("LibName", "PluginName") : { *(.text) }
  .memsz PLUGIN_CONTROL_MEMSZ("LibName", "PluginName") : { *(.data) }
}
