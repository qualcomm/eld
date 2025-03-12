SECTIONS {
  .rodata.island  : { *(.rodata.bar) }
  .rodata  : { *(.rodata*) }
  .text : { *(.text*) }
}
