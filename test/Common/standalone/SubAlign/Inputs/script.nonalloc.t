SECTIONS {
  .text : SUBALIGN(64) {
    *(.text*)
  }
  .debug_info : SUBALIGN(32) {
    *(.debug_info)
  }
  .comment : SUBALIGN(16) {
    *(.comment)
  }
  /DISCARD/ : { *(.ARM.exidx*) }
}
