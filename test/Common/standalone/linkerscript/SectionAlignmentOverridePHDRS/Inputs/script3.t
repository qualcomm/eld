PHDRS {
  A PT_LOAD;
}

SECTIONS {
  .foo : { *(.text.foo) } :A
  .text (204) : { *(.text*) } :A
}
