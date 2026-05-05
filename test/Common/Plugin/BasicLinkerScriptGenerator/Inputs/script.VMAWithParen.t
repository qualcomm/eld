SECTIONS {
  .text (0x1000) : { *(.text*) }
}
LINKER_PLUGIN("BasicLinkerScriptGenerator", "BasicLinkerScriptGenerator")
