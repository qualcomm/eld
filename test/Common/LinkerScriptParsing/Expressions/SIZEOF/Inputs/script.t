PHDRS {
  A PT_LOAD;
}

SECTIONS {
  .a : { *(.data.*) } :A
  size_of_segment_A = SIZEOF(:A);
  size_of_section_a = SIZEOF(a);
}
