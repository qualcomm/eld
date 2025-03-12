SECTIONS {
  .text : { *(.text) }
  .blah : { *(.rodata*) }
  .data : { *(.data) }
}
