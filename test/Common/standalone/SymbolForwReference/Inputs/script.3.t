SECTIONS {
  .text : { *(.text*) }
  .data : { *(.data*) }
  .comment : { *(.comment*) }
  a = b + 0x100;
  b = a + 0x100;
}
