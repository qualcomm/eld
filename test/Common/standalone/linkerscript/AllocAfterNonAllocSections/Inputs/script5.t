SECTIONS {
  .foo : { *(.text.foo) }
  .bar : { *(.text.bar) }
  .comment : {
               . = . + 100;
               *(.comment)
             }
  .debug : { *(.debug*) }
   __start = .;
  .baz : { *(.text.baz) }
   __end = .;
   /DISCARD/ : { *(.ARM.*) *(.*.attributes) }
}
