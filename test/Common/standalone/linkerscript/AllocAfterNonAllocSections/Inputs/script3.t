SECTIONS {
  .foo : { *(.text.foo) }
  .bar : { *(.text.bar) }
  .comment : { *(.comment) }
  .debug : { *(.debug*) }
   __start = .;
  .baz : { *(.text.baz) }
   __end = .;
   /DISCARD/ : { *(.ARM.*) *(.*.attributes) }
}
