PHDRS {
  A PT_LOAD FLAGS (0x03000000);
}

SECTIONS {
.a : { *(.data.*) } :A
.c : { *(.bss.c) } :A
.b : { *(.bss.b) } :A
size_of_segment_A = SIZEOF(:A);
}
