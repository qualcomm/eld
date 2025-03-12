PHDRS {
  A PT_LOAD;
  B PT_LOAD;
}

SECTIONS {
.foo : { *(.foo) } :A
.bar (0x300) : { *(.bar) } :B
}
