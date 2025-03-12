PHDRS {
  A PT_LOAD;
}

SECTIONS {
  .foo : { *(.text.foo) } :A
  .empty (PROGBITS, R) : { . = . + 100; } :A
  /DISCARD/ : { *(.eh_frame) *(.ARM.ex*) }
}
