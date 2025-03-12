PHDRS {
  A PT_LOAD;
}

SECTIONS {
  .foo : { *(.text.foo) } :A
  .bar  : { *(.text.bar) } :A
  .baz : { *(.text.baz) } :A
  .debug : { *(.comment) }
  .empty1 0 : { *(.empty1) }
  __start = .;
  .empty : { . = . + 1; }
  ___end = .;
   /DISCARD/ : { *(.ARM.*) *(.*.attributes) }
}
