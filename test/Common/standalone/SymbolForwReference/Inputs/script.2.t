SECTIONS {
  .text : { *(.text*) }
  .data : { *(.data*) }
  .comment : { *(.comment*) }
  a = b + c;
  b = c + 0x100;
  c = 0x1000;
}
