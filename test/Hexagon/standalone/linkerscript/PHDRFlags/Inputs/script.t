PHDRS {
  A PT_LOAD FLAGS (0xf0000003);
}

SECTIONS {
.a : { *(.sdata.*) } :A
.c : { *(.bss.c) } :A
.b : { *(.bss.b) } :A
}
