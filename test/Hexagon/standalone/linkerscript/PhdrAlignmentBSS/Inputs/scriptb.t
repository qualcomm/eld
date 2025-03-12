/* cur section bss and previous section not bss */
PHDRS {
  A PT_LOAD;
  B PT_LOAD;
  C PT_LOAD;
}

SECTIONS {
.text : { *(.text) } :A
.a : { *(.data.a) } :B
.b : { *(.data.b) } :B
.c : { *(.bss.c) } :B
}
