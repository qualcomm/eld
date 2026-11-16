PHDRS {
  text PT_LOAD FLAGS(5);
  data PT_LOAD FLAGS(6);
}

SECTIONS {
  .text 0x0 : { *(.text) } :text
  .data 0x1000 : { *(.data) } :data
}
