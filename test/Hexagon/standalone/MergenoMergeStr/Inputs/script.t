SECTIONS {
  .text : { *(.text.foo) }
  /DISCARD/ :  { *(.text.bar) }
  .comment : { *(.comment) }
  .debug : { *(.debug*) }
}
