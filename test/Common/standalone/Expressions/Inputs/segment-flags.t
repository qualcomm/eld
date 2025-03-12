PHDRS {
  C PT_LOAD FLAGS (1 / 0);
}
SECTIONS {
  .text : { *(.text) } :C
  .data : { *(.data) } :C
}
