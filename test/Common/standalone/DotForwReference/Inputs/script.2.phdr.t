PHDRS {
  TEXT PT_LOAD;
  DATA PT_LOAD;
  COMMENT PT_NOTE;
}
SECTIONS {
  TEXT_START = u;
  . = TEXT_START;
  .text : { *(.text*) } :TEXT
  .data : { *(.data*) } :DATA
  .comment : { *(.comment*) } :COMMENT
  u = 0x4000 + v;
  w = MIN(0x1000, w + 0xf00);
  v = ALIGN(w, 0x1000);
}
