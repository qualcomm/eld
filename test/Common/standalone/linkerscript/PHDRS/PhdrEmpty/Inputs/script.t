PHDRS {
  A PT_LOAD;
  B PT_LOAD;
}

SECTIONS {
  .foo : { *(.text.foo) } :A
  .bar ALIGN(128) : {}
  .text (0x6040) : { *(.text*) }
}
