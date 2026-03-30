PHDRS {
  TEXT PT_LOAD;
  DATA PT_LOAD;
  COMMENT PT_NOTE;
}
SECTIONS {
  . = u;
  .text : { *(.text*) } :TEXT
  .data : { *(.data*) } :DATA
  .comment : { *(.comment*) } :COMMENT
  u = 0x3000;
}
