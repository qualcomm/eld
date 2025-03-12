PHDRS {
  A PT_LOAD;
}
SECTIONS {
  .baz (0x24d9c): AT(0x0) { *(.text.baz) } :A
  .foo : { *(.text.foo) } :A
  .bar : { *(.text.bar) } :A
}