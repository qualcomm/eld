PHDRS {
  A PT_LOAD;
}

SECTIONS {
  .foo : { *(.text.foo) } :A
  .text ALIGN(10) : { *(.text*) } :A
}
