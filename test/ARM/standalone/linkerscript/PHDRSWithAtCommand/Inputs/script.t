PHDRS {
A PT_LOAD AT (0x50000);
}

SECTIONS {
.a : { *(.data.*) } :A
.c : { *(.bss.c) } :A
.b : { *(.bss.b) } :A
size_of_segment_a = SIZEOF(:A);
}
