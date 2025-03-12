SECTIONS {
  .foo : { *(.text.foo) }
  .comment : { *(.comment) }
  .attributes : { *(.hexagon.attributes) }
   _unrecognized_start = .;
  .unrecognized : { *(*) }
   _unrecognized_end = .;
}
