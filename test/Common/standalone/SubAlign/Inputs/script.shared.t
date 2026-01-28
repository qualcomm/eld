SECTIONS {
  .shared_text (0x1000) : SUBALIGN(64) {
    *(.text*)
  }
  /DISCARD/ : { *(.ARM.exidx*) }
}
