PHDRS {
  A PT_LOAD;
}

SECTIONS {
  .data : { *(.data) } :A
  .newdata (0x2000) : { } :A
}
