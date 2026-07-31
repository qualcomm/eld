PHDRS {
  text PT_LOAD FLAGS(7);
}
SECTIONS {
  .text : { *(.text*) } :text
  .data : { *(.data*) } :text
}
