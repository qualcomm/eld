PHDRS {
  A PT_LOAD;
}

SECTIONS {
.c (PROGBITS) : { *(.bss.c) } :A
.b (PROGBITS) : { *(.bss.b) } :A
.a : { *(.data.a) } :A
}
