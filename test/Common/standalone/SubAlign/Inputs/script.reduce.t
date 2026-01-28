SECTIONS {
  .reduce : SUBALIGN(2) {
    *(.text*)
  }
  /DISCARD/ : { *(.ARM.exidx*) }
}
