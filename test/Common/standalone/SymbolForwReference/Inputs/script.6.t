SECTIONS {
  a = b;
  .text : { *(.text*) }
  .data : { *(.data*) }
  .comment : { *(.comment) }
  .eh_frame : { *(.eh_frame) }
  .note.GNU-stack : { *(.note.GNU-stack) }
  PROVIDE(b = non_existing_symbol);
}
