SECTIONS {
  .partial : SUBALIGN(64) {
    *(.text*)
  }
  /DISCARD/ : { *(.ARM.exidx*) }
}
