PHDRS {
  TEXT PT_LOAD;
  RODATA PT_LOAD;
}
SECTIONS {
  .rodata : { *(.rodata*) } :RODATA
  .text : { *(.text*) } :TEXT
}
