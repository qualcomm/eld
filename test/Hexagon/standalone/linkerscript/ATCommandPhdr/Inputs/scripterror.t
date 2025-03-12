PHDRS {
  TEXT PT_LOAD;
  DATA PT_LOAD;
}

SECTIONS {
  .text : { *(.text*) } :TEXT
  .data : AT(0x4000) { *(.data*) } :DATA
  .bss : AT(0x5000) { *(.data*) }
}
