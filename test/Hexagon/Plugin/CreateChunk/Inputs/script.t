PLUGIN_OUTPUT_SECTION_ITER("CreateChunk", "CreateChunk")

SECTIONS {
.text : { *(.text*) }
.rodata : { *(.rodata*) }
.bss  : { *(.bss*) }
}
