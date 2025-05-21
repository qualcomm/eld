SECTIONS {
  .text : { *(.text*) }
  .rodata : { *(.rodata*) }
  .data : { *(.data*) }
  /DISCARD/ : { *(.ARM.exidx*) }
}
