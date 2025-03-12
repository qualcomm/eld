SECTIONS {
  .foo : { *(.text) *(.text.foo) }
  .comment : { *(.comment) }
  .attributes : { *(*attributes) }
   _unrecognized_start = .;
  .unrecognized : { *(*) }
   _unrecognized_end = .;
  /DISCARD/ : { *(.note*) }
}
