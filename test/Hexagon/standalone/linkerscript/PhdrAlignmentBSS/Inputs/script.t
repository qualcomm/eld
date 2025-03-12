PHDRS {
  A PT_LOAD;
  B PT_LOAD;
}

SECTIONS {
.text : { *(.text) } :A
.a : { *(.bss.a) } :B
.b : { *(.bss.b) } :B
.c : { *(.bss.c) } :B
}
