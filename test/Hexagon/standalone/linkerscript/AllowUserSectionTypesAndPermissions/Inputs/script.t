PHDRS {
  A PT_LOAD;
}

SECTIONS {
.c : { *(.bss.c) } :A
.b : { *(.bss.b) } :A
.a : { *(.data.a) } :A
}
