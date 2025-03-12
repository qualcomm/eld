PHDRS {
  A PT_LOAD;
}

SECTIONS {
.c (PROGBITS, RX) : { *(.bss.c) } :A
.b (PROGBITS, RX) : { *(.bss.b) } :A
.a (PROGBITS, RX) : { *(.data.a) } :A
}
