PHDRS {
  A PT_LOAD;
}

SECTIONS {
.c (0x4000) (PROGBITS, RX) : { *(.bss.c) } :A
.b (PROGBITS, RX) : { *(.bss.b) } :A
.a (PROGBITS, RX) : { *(.data.a) } :A
}
