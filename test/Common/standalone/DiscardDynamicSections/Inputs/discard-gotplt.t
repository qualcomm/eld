SECTIONS {
  .text : { *(.text) }
  /DISCARD/ : { *(.got.plt) }
}
