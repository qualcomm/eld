SECTIONS {
  a = b;
  b = 0x100;
  .text : { *(.text*) }
  .data : { *(.data*) }
  .comment : { *(.comment) }
  .eh_frame : { *(.eh_frame) }
  .note.GNU-stack : { *(.note.GNU-stack) }
  b = 0x300;
}
