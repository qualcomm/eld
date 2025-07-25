PHDRS {
  A PT_LOAD;
}

SECTIONS {
  .text : { *(.text*) } :A
  .rodata : { *(.rodata*) } :A
  .data : { *(.data*) } :A
  /DISCARD/ : { *(.ARM.exidx*) }
}
