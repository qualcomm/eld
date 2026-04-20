SECTIONS {
  .text : { *(.text.start) }
  /DISCARD/ : { *(.text.dep) }
}
