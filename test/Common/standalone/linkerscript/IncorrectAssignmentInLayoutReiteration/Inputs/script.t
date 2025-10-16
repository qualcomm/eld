u = 0x100;
SECTIONS {
  v = u;
  .text : { *(.text*) }
  u = 0x300;
  .data : { *(.data*) }
}
