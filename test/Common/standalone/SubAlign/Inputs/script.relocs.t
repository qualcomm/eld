SECTIONS {
  .reloc_text : SUBALIGN(64) {
    *(.text*)
    *(.rel.*)
  }
  /DISCARD/ : { *(.ARM.exidx*) }
}
