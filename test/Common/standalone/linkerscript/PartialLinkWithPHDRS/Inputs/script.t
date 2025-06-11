PHDRS {
  A PT_LOAD;
  B PT_LOAD;
}

SECTIONS {
  .text : { *(.text.*) } :B
  .data : { *(.data.*) }
}
