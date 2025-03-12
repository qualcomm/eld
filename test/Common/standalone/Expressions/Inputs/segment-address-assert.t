PHDRS {
  C PT_LOAD AT(ASSERT(0, "LMA"));
}
SECTIONS {
  .text : { *(.text) } :C
  .data : { *(.data) } :C
}
