PHDRS {
  C PT_LOAD FLAGS (ASSERT(0, "FLAGS"));
}
SECTIONS {
  .text : { *(.text) } :C
  .data : { *(.data) } :C
}
