SECTIONS {
  .text : { *(.text) }
  /DISCARD/ : { *(.plt) }
}
