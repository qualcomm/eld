SECTIONS {
  . = u;
  .text : { *(.text*) }
  .data : { *(.data*) }
  .comment : { *(.comment*) }
  u = 0x3000;
}
