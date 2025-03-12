SECTIONS {
  .text : { *(.text*) }
  .dynamic : { *(.dynamic) }
  . = 0x174;
  .plt : { *(.plt*) }
  . = ADDR(.plt) + 0x4500c;
  .got : { *(.got.plt) *(.got) }
  .data : { *(.data*) }
  /DISCARD/: { *(.ARM.exidx*) }
}
