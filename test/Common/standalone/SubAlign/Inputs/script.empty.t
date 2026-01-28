SECTIONS {
  .nonempty : SUBALIGN(64) {
    *(.text.foo)
  }
  .empty1 : SUBALIGN(32) {
    *(.nonexistent1)
  }
  .empty2 : SUBALIGN(128) {
    /* Empty section with just location counter */
    . = . + 0;
  }
  /DISCARD/ : { *(.ARM.exidx*) }
}
