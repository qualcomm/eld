PHDRS {
  A PT_LOAD;
}

SECTIONS {
  .foo : { *(.text.foo) } :A
  .bar : { *(.text.bar) } :A
  .comment : {
               . = . + 100;
               *(.comment)
             }
  .debug : { *(.debug*) }
   __start = .;
  .baz : { *(.text.baz) } :A
   __end = .;
   /DISCARD/ : { *(.ARM.*) *(.*.attributes) }
}
