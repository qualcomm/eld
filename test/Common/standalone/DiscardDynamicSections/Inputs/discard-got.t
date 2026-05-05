SECTIONS {
  .text : { *(.text) }
  /DISCARD/ : { *(.got) *(got*) }
}
