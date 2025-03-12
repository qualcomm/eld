PHDRS {
  TEXT PT_LOAD;
  RODATA PT_LOAD;
}
SECTIONS {
  .text (NOLOAD) : { *(.text*) }
}
