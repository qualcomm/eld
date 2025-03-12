PHDRS {
  A PT_LOAD;
  B PT_LOAD;
}

SECTIONS {
.foo : { *(.text.foo) } :A
.bar : { *(.text.bar) } :B
}
