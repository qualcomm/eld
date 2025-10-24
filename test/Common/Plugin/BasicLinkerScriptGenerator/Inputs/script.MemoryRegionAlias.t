MEMORY {
  SDRAM (rwx) : ORIGIN = 0x1000, LENGTH = 0x1000
}

REGION_ALIAS("alias_sdram", SDRAM)

SECTIONS {
  .text2 : { *(.text*) } > alias_sdram
}

LINKER_PLUGIN("BasicLinkerScriptGenerator", "BasicLinkerScriptGenerator")
