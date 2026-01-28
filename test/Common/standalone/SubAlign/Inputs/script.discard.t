PHDRS {
  A PT_LOAD;
}

SECTIONS {
  .keep : SUBALIGN(64) {
    *(.text.foo)
    *(.text.bar)
  }:A
  /DISCARD/ : SUBALIGN(32) {
    *(.text.car)
    *(.ARM.exidx*)
  }
}
