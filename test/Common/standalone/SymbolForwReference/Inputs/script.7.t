SECTIONS {
  a = b;
  f = 0x5 + g;
  .text : { *(.text*) }
  PROVIDE(b = d) ;
  .data : { *(.data*) }
  PROVIDE(b = c);
  .comment : { *(.comment) }
  PROVIDE(c = 0x11);
  g = 0x7;
  .eh_frame : { *(.eh_frame) }
  PROVIDE(d = e);
  .note.GNU-stack : { *(.note.GNU-stack) }
  PROVIDE(e = 0x13);
  PROVIDE(g = 0x9);
  g = 0x15;
}
