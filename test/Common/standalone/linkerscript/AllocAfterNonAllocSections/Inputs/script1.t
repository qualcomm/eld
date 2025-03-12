SECTIONS {
  .foo : { *(.text.foo) }
  .bar : { *(.text.bar) }
  .baz : { *(.text.baz) }
  .comment : { *(.comment) }
  .empty1 0 : { *(.empty1) }
  __start = .;
  .empty : { . = . + 1; }
  ___end = .;
   /DISCARD/ : { *(.ARM.*) *(.*.attributes) }
}
