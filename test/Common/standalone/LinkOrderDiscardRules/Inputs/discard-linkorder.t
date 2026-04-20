SECTIONS {
  .text : { *(.text.start) *(.text.dep) }
  /DISCARD/ : { *(.linkorder.dep) }
}
