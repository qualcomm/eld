INCLUDE memory.t
SECTIONS {
  .text : { *(.text*) } > ROM
  .data : { *(.data*) *(.sdata*) } > RAM AT > ROM
  .bss : { *(.bss*) *(.sbss*) *(COMMON) } > RAM
  .explicit : AT(0x30000) { *(.explicit) } > RAM
  .alias : { *(.alias) } > ALIAS AT > ALIAS
  .auto : { *(.auto) }
}
