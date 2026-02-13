SECTIONS {
  TEXT_START = u;
  . = TEXT_START;
  .text : { *(.text*) }
  .data : { *(.data*) }
  .comment : { *(.comment*) }
  u = 0x4000 + v;
  w = MIN(0x1000, w + 0xf00);
  v = ALIGN(w, 0x1000);
}
