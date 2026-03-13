PHDRS {
  A PT_LOAD;
}

SECTIONS {
  .a : SUBALIGN(64) {
   . = . + 1;
    *(.text*)
  }:A
  .b : SUBALIGN(64) {
   . = . + 1;
  }:A
  /DISCARD/ : { *(.ARM.exidx*) }
}
