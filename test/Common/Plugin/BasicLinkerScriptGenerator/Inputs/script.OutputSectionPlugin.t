SECTIONS {
  .foo PLUGIN_CONTROL_MEMSZ("copy","COPYBLOCKS", "G0") : { *(*foo*) }
  .bar : { *(*bar*) }
  .baz : { *(*baz*) }
}

LINKER_PLUGIN("BasicLinkerScriptGenerator", "BasicLinkerScriptGenerator")