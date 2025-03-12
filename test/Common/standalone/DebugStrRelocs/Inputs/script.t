PHDRS {
  TEXT PT_LOAD;
  DATA PT_LOAD;
}

SECTIONS {
  .text : { *(.text) } :TEXT
  .data : { *(.data) } :DATA
  .debug_str 0 : { *(.debug_str) }
}
